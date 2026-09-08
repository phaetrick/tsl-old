#include "track.h"
#include "infopanel.h"
#include "logger.h"
#include "tools/aligned_memalloc.h"
#include "defines.h"
#include "grainstorm.h"
#include "view.h"
#include "tools.h"
#include "envelope.h"
#include "synth.h"
#include "filter.h"
#include "phaser.h"
#include "compressor.h"
#include "eq.h"
#include "oscil.h"
#include "ffttools.h"
#include "knob.h"
#include "Convolver.h"
#include "lfo.h"
#include "pv.h"
#include "chorus.h"
#include "waveform.h"
#include <audio/Recording.h>
#include <Follower.h>
#include "degradation.h"
#include "platform_config.h"
#include "Effects/gaintask.h"


namespace {
	/* Tells the user what the pause is, rather than letting the app look hung.
	   Recording::pushSwap() archives the track's OUTGOING take into a swap page
	   and is O(that recording), so replacing a long take stalls the snapShot
	   thread for seconds -- measured at 3.5 s on device. It cannot move off
	   that thread: snapShot serialises everything touching the events
	   instance, so archiving elsewhere would let an undo run between
	   filebuffer.store() and the swap page being written.

	   Only shown above a couple of seconds of outgoing audio; below that the
	   swap is milliseconds and the flash would be noise.

	   Saves and restores the panel's whole state rather than forcing
	   RenderStand afterwards: loadAudio is also called from the decoder, the
	   Editor and undo, and those have their own panel state to return to. */
	struct SwapBusyIndicator {
		tsl::graphics::InfoPanel* panel{};
		tsl::graphics::InfoPanel::RenderFuncInfoPanel* prevFunc{};
		const char* prevText{};
		const char* prevText2{};
		float prevProgress{};
		long prevBytes{};
		long prevDecOffset{};

		tsl::Recording::SwapProgressSink progressSink{};

		/* render_progress always draws info->bytes as MB and info->dec_offset
		   as m:s:ms, so leaving them alone showed whatever last wrote them (the
		   recorder's wrapper.tell()) -- a stale size during archiving. They are
		   driven from the copy loop instead, counting up toward the total
		   exactly as the decoder does (DecoderAndroid.cpp:551). */
		SwapBusyIndicator(tsl::graphics::InfoPanel* p, const char* trackName,
			bool worthShowing) {
			if (!p || !worthShowing)
				return;
			panel = p;
			prevFunc = p->renderfunc.load();
			prevText = p->text.load();
			prevText2 = p->text2.load();
			prevProgress = p->progress.load();
			prevBytes = p->bytes.load();
			prevDecOffset = p->dec_offset.load();

			p->progress.store(0.f);
			p->bytes.store(0);
			p->dec_offset.store(0);
			progressSink.fraction = &p->progress;
			progressSink.frames = &p->dec_offset;
			progressSink.bytes = &p->bytes;
			p->text.store(trackName);
			p->text2.store(" ARCHIVING");
			p->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
			/* No redraw(): InfoPanel is perm, so it is in queue_draw
			   permanently and repaints every frame. It picks the progress up on
			   its own while this thread is inside the copy. */
		}

		// Null when not showing, so saveForPool skips reporting entirely.
		const tsl::Recording::SwapProgressSink* sink() const {
			return panel ? &progressSink : nullptr;
		}

		~SwapBusyIndicator() {
			if (!panel)
				return;
			panel->text.store(prevText);
			panel->text2.store(prevText2);
			panel->progress.store(prevProgress);
			panel->bytes.store(prevBytes);
			panel->dec_offset.store(prevDecOffset);
			panel->renderfunc.store(prevFunc);
		}

		SwapBusyIndicator(const SwapBusyIndicator&) = delete;
		SwapBusyIndicator& operator=(const SwapBusyIndicator&) = delete;
	};
}

static constexpr double loopdummy[8 * 5]{ 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,0, 0, 0, 0, 1,0, 0, 0, 0, 1,0, 0, 0, 0, 1 };

TRACK::TRACK(DATA* data, tsl::AppState* appState, const char* _name, int _index, const char* modulatorname)
	: _appState(appState),
	SyncTarget(appState, "LOOP", this, TRACK_CONTROLS_ACTIVE, TRACK_SYNC_FACTOR),
	lfo1(appState, this, 0, "LFO1"), lfo2(appState, this, 1, "LFO2"),
	lfo3(appState, this, 2, "LFO3"), graingen(this, appState),
	grainsequencer(appState, "GRAIN SEQUENCER", this, GRAINSEQINPUT, GRAINSEQFACT),
	bpmSyncer(appState, "BPM SYNC", this, BPMSYNCINPUT, BPMSYNCSYNCFACTOR),
	delaySyncTargets{ {appState, "DELAY",   this, DELAY_CONTROLS_ACTIVE,   DELAY_SYNC_FACTOR,   DELAYDEL},
					 {appState, "MDELAY1", this, MDELAY1_CONTROLS_ACTIVE, MDELAY1_SYNC_FACTOR, MDELAY1DEL},
					 {appState, "MDELAY2", this, MDELAY2_CONTROLS_ACTIVE, MDELAY2_SYNC_FACTOR, MDELAY2DEL},
					 {appState, "MDELAY3", this, MDELAY3_CONTROLS_ACTIVE, MDELAY3_SYNC_FACTOR, MDELAY3DEL},
					 {appState, "MDELAY4", this, MDELAY4_CONTROLS_ACTIVE, MDELAY4_SYNC_FACTOR, MDELAY4DEL},
					 {appState, "MDELAY5", this, MDELAY5_CONTROLS_ACTIVE, MDELAY5_SYNC_FACTOR, MDELAY5DEL},
					 {appState, "MDELAY6", this, MDELAY6_CONTROLS_ACTIVE, MDELAY6_SYNC_FACTOR, MDELAY6DEL},
					 {appState, "MDELAY7", this, MDELAY7_CONTROLS_ACTIVE, MDELAY7_SYNC_FACTOR, MDELAY7DEL},
					 {appState, "MDELAY8", this, MDELAY8_CONTROLS_ACTIVE, MDELAY8_SYNC_FACTOR, MDELAY8DEL},
					 {appState, "PPDELAY", this, PP_CONTROLS_ACTIVE,      PP_SYNC_FACTOR,      PPDELAY} }
	, spectrumAnalyzer(_STATE, data->inputspectrum[_index]), gainTask{ this } {
	index = _index;
	number = index + 1;
	name = _name;
	modulatorName = modulatorname;
}



TRACK::~TRACK() {}

tsl::parameters::Event TRACK::loadAudio(std::shared_ptr<tsl::Recording>& rec) {
	if (rec) {
		rec->trackIndex = index;
		if (rec->off > 0 && rec->poolHandle >= tsl::NO_SOUND_PRESENT) {
			// tiny initial allocation
			if ((rec->poolHandle =
				allocRecording(
					_appState,
					1,
					rec->channels))
				< tsl::NO_SOUND_PRESENT)
			{
				auto& pd =
					_appState->recordings[rec->poolHandle];
				std::lock_guard lk(pd.mutex);

				pd.incRefCount();

				pd.fileName = rec->fileName;
				pd.numEdits = rec->numEdits;
				pd.lastEdit = rec->lastEdit;
			}
		}
		//	rec->pushSwap();
	}
	auto current = filebuffer.load();

	/* Publish the new audio BEFORE archiving the old, not after.
	   current->pushSwap() copies the entire outgoing recording into a swap
	   page sample-by-sample -- measured at 3.5 s on device for a track that
	   already held a take, and the cost is the OLD recording's length, so a
	   two-second mic recording paid it in full. Everything else in this
	   function totals under a millisecond.

	   Nothing below the store needs the old buffer: pushSwap works on
	   `current`, which is a shared_ptr already captured above, so the archival
	   is unaffected by the store. currentAudioToEvent() DOES read
	   filebuffer.load() -- it takes the outgoing recording's poolHandle for
	   the undo event -- so it has to stay ahead of the store. That is the only
	   ordering constraint here.

	   The decoder never had this problem because it chains loadAudio into its
	   own snapShot task and calls waveform->setup() first
	   (DecoderAndroid.cpp:663); it pays the same cost off the visible path.
	   The recorder calls loadAudio inline, so the cost was the delay between
	   stopping a recording and the track going live. */
	auto e = currentAudioToEvent();
	//filebufferold.store(current);
	filebuffer.store(rec);

	if (current) {
		tsl::RecordingDiff diff;
		diff.type = tsl::RecordingDiff::Replacement;
		diff.oldSize = current->off;
		diff.newSize = rec ? rec->off : 0;
		SwapBusyIndicator busy{ _DATA->views.infopanel, name,
			current->off > (decltype(current->off))(_STATE->sr * 2) };
		current->pushSwap(diff, busy.sink());
	}

	_DATA->snapShot.addRecording(index, rec);

	auto tmp = rec == nullptr ? 0 : rec->off;
	offToInfo = tmp;

	auto state = rec ? rec->state.load() : nullptr;
	if (state) {
		play_dur = state->off_stop - state->off_start;

	}
	else play_dur = 0;

	time_convert(time_file, _STATE->sr, tmp);

	computeLoopTime();
	//resetwaveform = true;

#ifndef RELEASEBUILD
	auto usage = tsl::get_memory_stats();
	LOGI("Memory Usage %g MB  Phys Avail:  %g MB Total: %g MB ", static_cast<double>(usage.process_rss_bytes / (1024. * 1024.)), static_cast<double>(usage.available_phys_bytes / (1024. * 1024.)), static_cast<double>(usage.total_phys_bytes / (1024. * 1024.)));
#endif

	return e;
}

tsl::parameters::Event TRACK::currentAudioToEvent() {
	auto current = filebuffer.load();
	auto recNum = current == nullptr ? tsl::NO_SOUND_PRESENT : current->poolHandle;
	tsl::parameters::Event e{};
	e.trackIndex = index;
	e.eventType = tsl::parameters::Eventtype::Recording;
	e.subType = tsl::parameters::EventSubtype::recordingChange;
	e.poolHandle = recNum;
	e._appState = _STATE;
	e.flags |= tsl::parameters::Event::EventFlags::History;
	if (recNum < tsl::NO_SOUND_PRESENT) {
		auto& pd = _STATE->recordings[recNum];
		std::lock_guard lk(pd.mutex);
		pd.incRefCount();
	}
	return std::move(e);
};

tsl::parameters::Event TRACK::loadAudio(std::shared_ptr<tsl::Recording>& rec, tsl::RecordingDiff& diff) {

	if (rec) {
		rec->trackIndex = index;
		if (rec->off > 0 && rec->poolHandle >= tsl::NO_SOUND_PRESENT) {
			// tiny initial allocation
			if ((rec->poolHandle =
				allocRecording(
					_appState,
					1,
					rec->channels))
				< tsl::NO_SOUND_PRESENT)
			{
				auto& pd =
					_appState->recordings[rec->poolHandle];
				std::lock_guard lk(pd.mutex);

				pd.incRefCount();

				pd.fileName = rec->fileName;
				pd.numEdits = rec->numEdits;
				pd.lastEdit = rec->lastEdit;
			}
		}
	}

	auto current = filebuffer.load();
	if (current) {
		SwapBusyIndicator busy{ _DATA->views.infopanel, name,
			current->off > (decltype(current->off))(_STATE->sr * 2) };
		current->pushSwap(diff, busy.sink());
	}
	auto e = currentAudioToEvent();
	//filebufferold.store(current);
	filebuffer.store(rec);

	_DATA->snapShot.addRecording(index, rec);

	auto tmp = rec == nullptr ? 0 : rec->off;
	offToInfo = tmp;

	auto state = rec ? rec->state.load() : nullptr;
	if (state) {
		play_dur = state->off_stop - state->off_start;

	}
	else play_dur = 0;

	time_convert(time_file, _STATE->sr, tmp);

	computeLoopTime();
	//resetwaveform = true;

#ifndef RELEASEBUILD
	auto usage = tsl::get_memory_stats();
	LOGI("Memory Usage %g MB  Phys Avail:  %g MB Total: %g MB ", static_cast<double>(usage.process_rss_bytes / (1024. * 1024.)), static_cast<double>(usage.available_phys_bytes / (1024. * 1024.)), static_cast<double>(usage.total_phys_bytes / (1024. * 1024.)));
#endif

	return e;
}



std::shared_ptr<tsl::Recording> TRACK::getAudioCopy() {
	auto current = filebuffer.load();
	if (current) {
		try {
			return std::make_shared<tsl::Recording>(*current);
		}
		catch (std::bad_alloc&) {
			// explicitly fall through to empty recording
		}
	}
	try {
		return std::make_shared<tsl::Recording>(_STATE, noSoundLoaded, _STATE->sr, _STATE->channels);
	}
	catch (std::bad_alloc&) {
		return nullptr; // truly out of memory, caller must handle null
	}
}

bool TRACK::getFilesCopy(std::vector<int16_t> files[], int32_t& offTmp) {
	offTmp = 0;
	auto rec = filebuffer.load();
	if (rec != nullptr) {
		try {
			for (int i = 0; i < rec->channels; i++) {
				auto& f = rec->buffer[i];
				if (i == 0)offTmp = f.size();
				else if (f.size() < offTmp)offTmp = f.size();
				files[i] = f;
			}
		}
		catch (const std::bad_alloc&) {
			for (int i = 0; i < rec->channels; i++) {
				files[i].clear();
			}
			offTmp = 0;
			return false;
		}
	}
	return true;
}


template<typename T, typename T2>
void atomic_max(std::atomic<T>& a, T2 x) {
	T cur = a.load(std::memory_order_relaxed);
	while (cur > x && !a.compare_exchange_weak(cur, x,
		std::memory_order_relaxed, std::memory_order_relaxed));
}





static void lfo_set_tables(LFO* lfo) {
	auto _appState = lfo->_appState;
	lfo->env[0] = _DATA->eq["SINE LFO"].win;
	lfo->env[1] = _DATA->eq["TRIANGLE"].win;
	lfo->env[2] = _DATA->eq["SAWTOOTH"].win;
	lfo->env[3] = _DATA->eq["RECTPULS"].win;
	lfo->store(LFORECOMPUTE, 1.);
	lfo->reCompute();


	/*
	auto table = lfo->drawTable;

	const int32_t nsegs = lfo->segments();
	MYFLOAT xx[32], yy[32];
	for (int32_t i = 0; i <= nsegs; i++) {
		xx[i] = *(lfo->getpos0() + i);
		yy[i] = *(lfo->getval0() + i);
	}

	tsl::envelope::compute[lfo->editfunc()](table, LFO_TBL_SIZE, xx,
											yy, nsegs, true);
											*/
}

void TRACK::OnGotSampleRate() {

	for (int32_t i = 0; i < NUM_PARAMS; i++) {
		//if(!_STATE->parameters[i].noasignment)
		_STATE->controls[index][i].lfo_min = _STATE->controls[index][i].lfo_max = _STATE->params[index][i] = _STATE->parameters[i].initvalue;
	}
	init();
	auto rec = filebuffer.load();
	auto state = rec ? rec->state.load() : nullptr;
	time_convert(time_file, _STATE->sr, state ? state->offset.load() : 0);
	computeLoopTime();
	//time_convert(time_play_dur, _STATE->sr, offToInfo.load());

	for (auto tmp : lfos) {
		tmp->setRandomPhinc(.2 / _STATE->sr);
		tmp->buf = lfobuffer[tmp->index];
	}

	graingen.onSampleRateChanged();
}

void TRACK::reset() {
	for (int32_t i = 0; i < NUM_PARAMS; i++) {
		lfo[i].store(nullptr);
		_STATE->controls[index][i].lfo_min = _STATE->controls[index][i].lfo_max = _STATE->parameters[i].initvalue;
		if (!(_STATE->parameters[i].flags & Param::NoAssignment))
			_STATE->params[index][i].store(_STATE->parameters[i].initvalue);
	}

	for (auto& [key, val] : _STATE->followerMap[index]) {
		val.reset();
	}

	for (int32_t i = 0; i < NUM_PARAMSPACES; i++) {
		bypass[i].store(false);
		fxpower[i].store(false);
	}

	for (int32_t chan = 0; chan < _STATE->channels; chan++) {
		fx_queue[chan].reset();
		fx_queue_grain[chan].reset();
		std::memset(ringbuffer[chan], 0, _STATE->ringSize * sizeof(MYFLOAT));
	}
	auto deg = fx_queue_stereo.find(tsl::DEGRADATION_STEREO_ID);
	fx_queue_stereo.reset();
	if (deg != nullptr)fx_queue_stereo.push(deg);

}

void TRACK::setDefaults() {
	_STATE->params[index][MONOCOMPSRC].store(index);

	_STATE->params[index][PITCHDETECTFXTRACK].store(index);

	_STATE->params[index][FOLLOWER1SRC].store(index);
	_STATE->params[index][FOLLOWER2SRC].store(index);
	_STATE->params[index][FOLLOWER3SRC].store(index);
	_STATE->params[index][DISTRSOURCE] = index;
	setLfoDest(0, GRAINSIZE);
	setLfoDest(1, SPEED);
	setLfoDest(2, PREGAIN);
	_STATE->params[index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[index][RECOMPUTEGRAINENV3] = 1.0;

	lfo1.store(LFORECOMPUTE, 1.0);
	lfo1.store(LFOREDRAW, 1.0);
	lfo2.store(LFORECOMPUTE, 1.0);
	lfo2.store(LFOREDRAW, 1.0);
	lfo3.store(LFORECOMPUTE, 1.0);
	lfo3.store(LFOREDRAW, 1.0);

	for (auto& [key, val] : _STATE->followerMap[index]) {
		val.reset();
	}

	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}
}




tsl::parameters::Event  TRACK::setToInitState() {
	reset();
	setDefaults();
	//sprintf(track->name, "%s%d", "TRACK", track->number);
	//ASSERT(!sem_init(&track->stereo_fx_sem, 0, 0));
	std::shared_ptr<tsl::Recording> rec{};
	auto e = loadAudio(rec);
	if (waveform) {
		waveform->setup(rec);
	}
	return std::move(e);
}

void TRACK::setEnvfDest(int32_t envIndex, int target) {
	if (target == PITCHSHIFTERSHIFT)
		target = PITCHSHIFTSHIFT;
#if defined USE_CHAR_FOR_CHAR8_T
	if (_STATE->followerMap[index].count(target)) {
#else
	if (_STATE->followerMap[index].contains(target)) {
#endif
		auto& fol = _STATE->followerMap[index].at(target);
		// Fallbacks are the FOLLOWERATT/FOLLOWERREL defaults, already on the Log10 curve.
		fol.att.store(_STATE->params[index][FOLLOWER1ATTACK + envIndex * 3].load() != 0 ? _STATE->params[index][FOLLOWER1ATTACK + envIndex * 3].load() : _STATE->parameters[FOLLOWERATT].initvalue);
		fol.rel.store(_STATE->params[index][FOLLOWER1RELEASE + envIndex * 3].load() != 0 ? _STATE->params[index][FOLLOWER1RELEASE + envIndex * 3].load() : _STATE->parameters[FOLLOWERREL].initvalue);
		fol.gain.store(_STATE->params[index][FOLLOWER1GAIN + envIndex * 3].load());
		fol.envpower.store(_STATE->params[index][FOLLOWER1POW + envIndex].load() == 1.);
		fol.source.store(_STATE->params[index][FOLLOWER1SRC + envIndex].load());
		if (_STATE->params[index][FOLLOWER1ADD + envIndex].load() == DETECT_ADD) {
			fol.min.store(_STATE->params[index][target].load());
			fol.max.store(_STATE->parameters[target].max);
		}
		else {
			fol.max.store(_STATE->params[index][target].load());
			fol.min.store(_STATE->parameters[target].max);
		}
		std::vector<tsl::parameters::Event> events;
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.att, tsl::parameters::EventSubtype::followerAtt, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.rel, tsl::parameters::EventSubtype::followerDec, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.gain, tsl::parameters::EventSubtype::followerGain, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Power, target, fol.envpower, tsl::parameters::EventSubtype::followerPower, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.source, tsl::parameters::EventSubtype::followerSidechain, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.min, tsl::parameters::EventSubtype::followerMin, 0, 0));
		events.push_back(tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::Follower, target, fol.max, tsl::parameters::EventSubtype::followerMax, 0, 0));
		std::lock_guard lk(_DATA->snapShot);
		for (auto& e : events) {
				_DATA->snapShot.addEvent(e);
			}
		}

}

void TRACK::setLfoDest(int32_t lfoindex, int target) {
	if (target == PITCHSHIFTERSHIFT)
		target = PITCHSHIFTSHIFT;
	else if (target == RNDWRITE)
		target = DENSDEV;
	else if (target == LFO1OLDRATE)
		target = LFO1CPS;
	else if (target == LFO2OLDRATE)
		target = LFO2CPS;
	else if (target == LFO3OLDRATE)
		target = LFO3CPS;

	// Bound by lfoValues, not by lfo[]: lfo[] is TRACK::lfo[NUM_PARAMS], nearly
	// two thousand entries, while lfoValues holds only the assignable
	// destinations. The old bound read past the end of lfoValues for every
	// target not in the list, and the guard below only caught the case where the
	// walk happened to stop exactly at the end.
	int32_t i = 0;
	for (; i < ARRAY_LEN(lfoValues); i++) {
		if ((int)lfoValues[i] == target)
			break;
	}
	if (i == ARRAY_LEN(lfoValues)) {
		return;
	}
	auto e = tsl::parameters::Event::createEvent(index, tsl::parameters::Eventtype::lfoDest, target, 1.0, lfoindex, 0, 0);
	e.apply(_STATE, tsl::parameters::None);
	//		addEvent(Event::createEvent(i, lfoDest, SPEED, 1.0, 0, 0, 0));
//	LFO* lfotmp = lfos[lfoindex];
//	lfo[target].store(lfotmp);
	// lfotmp->store(LFODEST, target);
}


void TRACK::init() {
	_STATE->params[index][FOLLOWERDEST] = DISTMIX;

	_STATE->params[index][MONOCOMPSRC].store(index);
	_STATE->params[index][PITCHDETECTFXTRACK].store(index);
	_STATE->params[index][FOLLOWER1SRC].store(index);
	_STATE->params[index][FOLLOWER2SRC].store(index);
	_STATE->params[index][FOLLOWER3SRC].store(index);
	//sprintf(track->name, "%s%d", "TRACK", track->number);
	//ASSERT(!sem_init(&track->stereo_fx_sem, 0, 0));
	_STATE->params[index][DISTRSOURCE] = index;


	setLfoDest(0, GRAINSIZE);
	setLfoDest(1, SPEED);
	setLfoDest(2, PREGAIN);
	lfo_set_tables(&lfo1);
	lfo_set_tables(&lfo2);
	lfo_set_tables(&lfo3);

	_STATE->params[index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[index][RECOMPUTEGRAINENV3] = 1.0;

	//destroy=destroy;


	offToInfo = 0;


	// MDELAY tap times used to be seeded here with an octave series while
	// ParameterInit declared a harmonic one as the default. Both OnGotSampleRate
	// and reset() copy initvalue into params[][], so seeding again here is
	// redundant now that the default carries the octave series -- and a second
	// copy of the formula is exactly how the two drifted apart.

	for (int32_t i = 0; i < _STATE->channels; i++) {
		//filebuffer[i] = std::make_shared<std::vector<short>>();
#ifdef LARGE_GRAINS
		gqueue[i] = std::make_unique<grainqueue>(this, i);
#endif
	}
	zoom = .8;

	int32_t nsegs = _STATE->params[index][GRAINNSEGS].load();

	MYFLOAT xx[32], yy[32];

	for (int32_t i = 0; i < nsegs + 1; i++) {
		xx[i] = _STATE->params[index][GRAINENVX0 + i].load();
		yy[i] = _STATE->params[index][GRAINENVY0 + i].load();

	}

	tsl::envelope::compute<MYFLOAT>[(int)_STATE->params[index][GRAINCURVE].load()](
		grainenv[SPACE_GRAINENV2].data(),
		WINDOW_SIZE,
		xx,
		yy,
		nsegs,
		true);
	grainsequencer.init(this);
	for (auto f : env_detectorvalues) {
		auto num = static_cast<int>(f);
		auto& param = _STATE->parameters[num];
		// att/rel defaults come from their own params -- they sit on a Log10 curve.
		Follower x{ _STATE, index, num, param.initvalue, param.initvalue,
					_STATE->parameters[FOLLOWERATT].initvalue,
					_STATE->parameters[FOLLOWERREL].initvalue, index };
		_STATE->followerMap[index].emplace(num, x);
	}
}


void TRACK::setupTracks(tsl::AppState * _appState) {

	_DATA->track1.destinationz = &_DATA->track2;
	_DATA->track2.destinationz = &_DATA->track3;
	_DATA->track3.destinationz = &_DATA->track4;
	_DATA->track4.destinationz = &_DATA->track1;
	_DATA->track1.lockerz = &_DATA->track4;
	_DATA->track2.lockerz = &_DATA->track1;
	_DATA->track3.lockerz = &_DATA->track2;
	_DATA->track4.lockerz = &_DATA->track3;
}

#include "infopanel.h"
using namespace tsl::parameters;
using namespace tsl::Syncing;

SyncResult TRACK::syncBySamples(double samples, std::vector<Event> &events) {
	SyncResult res{};
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	if (_STATE->params[index][LOOPSYNCDAWTRANSPORT].load() == 1.0) {
		if (samples <= 0) {
			res |= SyncFlags::LoopTooShort;
			return res;
		}
		// We need to make our loop region match the incoming sample duration
		double sampleRate = _STATE->sr;
		double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
		double beatsPerBar = _DATA->hostTimeSnapshot.beatsPerBar;

		// Incoming duration in seconds and beats
		double incomingSeconds = samples / sampleRate;
		double incomingBeats = (incomingSeconds * bpm) / 60.0;

		// We want our loop to match this duration, so:
		// loopBeats = incomingBeats
		// loopsPerBar = beatsPerBar / loopBeats
		double loopsPerBar = beatsPerBar / incomingBeats;
		auto syncfactor = _STATE->params[index][TRACK_SYNC_FACTOR].load();

		// Clamp to parameter range
		while (loopsPerBar > octaveCurveMax)
			loopsPerBar /= syncfactor;
		while (loopsPerBar < octaveCurveMin)
			loopsPerBar *= syncfactor;// Convert to normalized parameter (octave curve inverse)
		double loopsPerBarNormalized = (std::log2(loopsPerBar) + 4.0) / 8.0;
		loopsPerBarNormalized = std::clamp(loopsPerBarNormalized, 0.0, 1.0);
		if (_STATE->params[index][LOOPSYNCDAWBEATS].exchange(loopsPerBarNormalized, std::memory_order_release) != loopsPerBarNormalized) {
			Event e;
			e.trackIndex = index;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = LOOPSYNCDAWBEATS;
			e.value = loopsPerBarNormalized;
			events.push_back(e);
			res |= SyncFlags::Ok;
			_STATE->toUiThreadQueue.try_push([this, loopsPerBarNormalized]() {
			if (_STATE->active_track.load() == index && GETView(LOOPSYNCDAWBEATS)->visible_) {
				GETView(LOOPSYNCDAWBEATS)->redraw();
			}
				});
		}
		else res |= SyncFlags::UnChanged;

	}
	else
#endif
	{
		// Free-running mode: adjust playback speed
		long double diff = samples;
		auto rec = filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state == nullptr) {
			res |= SyncFlags::LoopTooShort;
			return res;
		}
		double diff_samples = state ? state->off_stop - state->off_start : 0;
		double playbackspeed = diff_samples / diff;
		auto syncfactor = _STATE->params[index][TRACK_SYNC_FACTOR].load();

		while (playbackspeed > 10)
			playbackspeed /= syncfactor;

		auto val = LOG10D20F(SPEED_OFFSET + playbackspeed);

		if (_STATE->params[index][SPEED].exchange(val) != val) {
			_STATE->toUiThreadQueue.try_push([this] {
				if (_STATE->active_track.load() == index && GETView(SPEED)->visible_) {
					GETView(SPEED)->redraw();
				}
				});
			Event e;
			e.trackIndex = index;
			e.paramIndex = SPEED;
			e.eventType = Eventtype::paramUpdate;
			e.value = val;
			events.push_back(e);
			res |= SyncFlags::Ok;
			computeLoopTime();
			return res;
		}
		else {
			res |= SyncFlags::UnChanged;
			return res;
		}
	}
	return res;
}


double TRACK::getSamples() {
	auto samples = getLoopSamples();
	if (samples <= 0) {
		showToast(_STATE, "EMPTY LOOP. CANNOT SYNC");
		return 0;
	}
	return samples;
}

SyncResult TRACK::control(uint16_t todo, std::vector<Event> &events) {
	SyncResult ret = 0;
	MYFLOAT syncfactor = syncFact();
	switch (todo) {

	case STEPBACK:
	{
		auto rec = filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state) {
			auto des = state->off_start.load();
			if (state->offset.exchange(des) != des) {
				Event e;
				e.trackIndex = index;
				e.poolHandle = rec->poolHandle;
				e.eventType = Eventtype::Recording;
				e.subType = EventSubtype::offset;
				e.value = des;
				events.push_back(e);
				ret |= SyncFlags::Ok;
			}
			else {
				ret |= SyncFlags::UnChanged;
			}
		}
		else {
			ret |= SyncFlags::LoopTooShort;
		}
		return ret;
	}

	case STEPFORW:
	{
		auto rec = filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state) {
			auto des = state->off_stop.load();
			if (state->offset.exchange(des) != des) {
				Event e;
				e.trackIndex = index;
				e.poolHandle = rec->poolHandle;
				e.eventType = Eventtype::Recording;
				e.subType = EventSubtype::offset;
				e.value = des;
				events.push_back(e);
				ret |= SyncFlags::Ok;
			}
			else {
				ret |= SyncFlags::UnChanged;
			}
		}
		else {
			ret |= SyncFlags::LoopTooShort;
		}
		return ret;

	}

	case STOPButton:
		if (_STATE->params[index][TRACKSTOPPED].exchange(1.) != 1.) {
			Event e;
			e.trackIndex = index;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = TRACKSTOPPED;
			e.value = 1;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else {
			ret |= SyncFlags::UnChanged;
		}
		return ret;

	case PLAYButton:
		if (_STATE->params[index][TRACKSTOPPED].exchange(0.) != 0.) {
			Event e;
			e.trackIndex = index;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = TRACKSTOPPED;
			e.value = 0.;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else {
			ret |= SyncFlags::UnChanged;
		}
		return ret;
	case SLOWButton:
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (_STATE->params[index][LOOPSYNCDAWTRANSPORT].load() == 1.0) {
			// DAW sync: divide loopsPerBar by syncfactor
			double loopsPerBarNorm = _STATE->params[index][LOOPSYNCDAWBEATS].load();
			double loopsPerBar = _STATE->parameters[LOOPSYNCDAWBEATS].toDisplay(_STATE->sr, loopsPerBarNorm);

			loopsPerBar /= syncfactor;
			if (loopsPerBar < octaveCurveMin) {
				ret |= SyncFlags::MinReached;
				return ret;
			}
			else if (loopsPerBar > octaveCurveMax) {
				ret |= SyncFlags::MaxReached;
				return ret;
			}

			// Convert back to normalized
			double newNorm = std::clamp(_STATE->parameters[LOOPSYNCDAWBEATS].fromDisplay(_STATE->sr, loopsPerBar), 0., 1.);
			if (_STATE->params[index][LOOPSYNCDAWBEATS].exchange(newNorm) != newNorm) {
				Event e;
				e.trackIndex = index;
				e.eventType = Eventtype::paramUpdate;
				e.paramIndex = LOOPSYNCDAWBEATS;
				e.value = newNorm;
				events.push_back(e);
				ret |= SyncFlags::Ok;
				_STATE->toUiThreadQueue.try_push([this] {if (_STATE->active_track.load() == index &&
					GETView(LOOPSYNCDAWBEATS)->visible_) {
					GETView(LOOPSYNCDAWBEATS)->redraw();  // Update appropriate view
				}
					});

				computeLoopTime();
			}
			else ret |= SyncFlags::UnChanged;

		}
		else
#endif
		{// Free-running: divide speed by syncfactor
			auto des = LOG10D20((LOG2NORMAL(_STATE->params[index][SPEED].load()) -
				SPEED_OFFSET) / syncfactor + SPEED_OFFSET);
			if (_STATE->params[index][SPEED].exchange(des) != des) {
				_STATE->toUiThreadQueue.try_push([this] {
					if (_STATE->active_track.load() == index && GETView(SPEED)->visible_) {
						GETView(SPEED)->redraw();
					}
					});
				Event e;
				e.trackIndex = index;
				e.paramIndex = SPEED;
				e.eventType = Eventtype::paramUpdate;
				e.value = des;
				events.push_back(e);
				ret |= SyncFlags::Ok;
				computeLoopTime();
			}
			else ret |= SyncFlags::UnChanged;

		}
		return ret;
	case FASTButton:
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (_STATE->params[index][LOOPSYNCDAWTRANSPORT].load() == 1.0) {
			// DAW sync: divide loopsPerBar by syncfactor
			double loopsPerBarNorm = _STATE->params[index][LOOPSYNCDAWBEATS].load();
			double loopsPerBar = _STATE->parameters[LOOPSYNCDAWBEATS].toDisplay(_STATE->sr, loopsPerBarNorm);

			loopsPerBar *= syncfactor;
			if (loopsPerBar < octaveCurveMin) {
				ret |= SyncFlags::MinReached;
				return ret;
			}
			else if (loopsPerBar > octaveCurveMax) {
				ret |= SyncFlags::MaxReached;
				return ret;
			}

			// Convert back to normalized
			double newNorm = std::clamp(_STATE->parameters[LOOPSYNCDAWBEATS].fromDisplay(_STATE->sr, loopsPerBar), 0.0, 1.0);

			if (_STATE->params[index][LOOPSYNCDAWBEATS].exchange(newNorm) != newNorm) {
				Event e;
				e.trackIndex = index;
				e.eventType = Eventtype::paramUpdate;
				e.paramIndex = LOOPSYNCDAWBEATS;
				e.value = newNorm;
				events.push_back(e);
				ret |= SyncFlags::Ok;
				_STATE->toUiThreadQueue.try_push([this] {
					if (_STATE->active_track.load() == index &&
						GETView(LOOPSYNCDAWBEATS)->visible_) {
						GETView(LOOPSYNCDAWBEATS)->redraw();  // Update appropriate view
					}
					});
				computeLoopTime();
			}
			else ret |= SyncFlags::UnChanged;
		}
		else
#endif
		{
			MYFLOAT val = (LOG2NORMAL(_STATE->params[index][SPEED].load()) -
				SPEED_OFFSET) * syncfactor + SPEED_OFFSET;
			if (val > 10. + SPEED_OFFSET) {
				ret |= SyncFlags::MaxReached;
			}
			else {
				auto des = LOG10D20F(val);
				if (_STATE->params[index][SPEED].exchange(des) != des) {
					_STATE->toUiThreadQueue.try_push([this] {
						if (_STATE->active_track.load() == index && GETView(SPEED)->visible_) {
							GETView(SPEED)->redraw();
						}
						});
					Event e;
					e.trackIndex = index;
					e.paramIndex = SPEED;
					e.eventType = Eventtype::paramUpdate;
					e.value = des;
					events.push_back(e);
					ret |= SyncFlags::Ok;
					computeLoopTime();
				}
				else ret |= SyncFlags::UnChanged;
			}
		}


		return ret;

	case DIRButton:
	{
		auto rec = filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state) {
			int expected = state->playbackDir.load(std::memory_order_acquire);
			while (!state->playbackDir.compare_exchange_weak(
				expected, expected * -1,
				std::memory_order_acq_rel,
				std::memory_order_acquire));
			Event e;
			e.trackIndex = index;
			e.poolHandle = rec->poolHandle;
			e.eventType = Eventtype::Recording;
			e.subType = EventSubtype::playbackDir;
			e.value = expected * -1;
			events.push_back(e);
			ret |= SyncFlags::Ok;

		}
		else ret |= SyncFlags::LoopTooShort;

	}
	return ret;

	default:
		return ret;
	}
}

void TRACK::computeLoopTime() {
#if defined(PLUGIN_MODE) || defined(OS_IOS)  
	if (_STATE->params[index][LOOPSYNCDAWTRANSPORT].load() == 1.0) {

		const double loopsPerBar = _STATE->parameters[LOOPSYNCDAWBEATS]
			.toDisplay(_STATE->sr,
				_STATE->params[index][LOOPSYNCDAWBEATS].load());

		const double beatsPerBar = _DATA->hostTimeSnapshot.beatsPerBar;

		const double loopBeats = beatsPerBar / loopsPerBar;

		double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
		const double loopSamples = loopBeats * _STATE->sr * 60.0 / bpm;

		time_convert(time_play_dur, _STATE->sr, loopSamples);
	}
	else
#endif
		time_convert_p(time_play_dur, _STATE->sr, play_dur,
			std::max(0., LOG2NORMALF(_STATE->params[index][SPEED].load()) -
				SPEED_OFFSET));
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	requestLoopPos.store(true);
#endif    

};

double TRACK::getLoopSamples() {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	if (_STATE->params[index][LOOPSYNCDAWTRANSPORT].load() == 1.0) {
		// Synced to DAW: compute loop duration from loopsPerBar
		double loopsPerBar = _STATE->parameters[LOOPSYNCDAWBEATS].toDisplay(_STATE->sr, _STATE->params[index][LOOPSYNCDAWBEATS].load());

		double beatsPerBar = _DATA->hostTimeSnapshot.beatsPerBar;;
		double loopBeats = beatsPerBar / loopsPerBar;

		double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
		double sampleRate = _STATE->sr;

		double secondsPerLoop = (60.0 / bpm) * loopBeats;
		if (secondsPerLoop <= 0)
			return 0;
		return secondsPerLoop * sampleRate;
	}
	else {
#endif
		// Free-running mode: compute from playback speed
		auto rec = filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		auto dist = state ? state->off_stop.load() - state->off_start.load() : 0;
		auto playbackSpeed = std::max(0., LOG2NORMALF(_STATE->params[index][SPEED].load()) -
			SPEED_OFFSET);
		if (dist <= 0 || playbackSpeed == 0.) {
			return 0;
		}
		else
			return dist / playbackSpeed;
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	}
#endif
}
