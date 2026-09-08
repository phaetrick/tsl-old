#include "granulate_fft.h"
#include "vocoder.h"
#include "pv.h"
#include "track.h"
#include "grainstorm.h"

static inline int32_t irand(const int a, const int e) {
	double r = e - a + 1;
	return a + (int)(r * rand() / (RAND_MAX + 1.0));
}

void compute_fft(TRACK* track, int32_t channel, const bool isMultiThreaded) {
	auto _appState = track->_appState;
	track->xdc[channel] = track->ydc[channel] = 0;
	auto ringMask = _STATE->ringMask;
	auto ringPosInternal = (_STATE->ringPos + 1) & ringMask;

	for (int i = 0; i < _STATE->currentBufSize; i++) {
		track->ringbuffer[channel][ringPosInternal] = 0;
		ringPosInternal = (ringPosInternal + 1) & ringMask;
	}
	TRACK* source = track->source;
	TRACK* locker = track->lockerz;
	bool cross_power = track->cross_power_tmp;

	auto fft_help1 = track->fft_help1[channel];
	auto fft_help2 = track->fft_help2[channel];
	auto fft_out = track->fft_out[channel];
	auto grainbuffer = track->grain_buffer[channel];
	const int32_t grainsize = locker->grainsize_tmp;

	MYFLOAT seqgain, seqpitch, seqsize;
	track->grainsequencer.tick(channel, seqgain, seqpitch, seqsize);

	auto sr = _STATE->sr;



	MYFLOAT grainamp_const = dbToLinear60(_STATE->params[track->index][PREGAIN].load());
	MYFLOAT grainamp = grainamp_const;

	MYFLOAT pan_const =
		_STATE->channels == 2 ? _STATE->params[track->index][GRAINPAN].load() : .5;

	bool pv_power = track->pv_power_tmp;

	auto pvfunc = pv_funcs[track->pv_func_tmp];//track->fx_func2.load();
	auto crossfunc = cross_funcs[track->cross_func_tmp];


	MYFLOAT grainamp_range = 0;
	const LFO* grainamp_lfo = track->lfo[PREGAIN];
	const bool dograinamplfo = grainamp_lfo && grainamp_lfo->powertmp;
	if (dograinamplfo) {
		MYFLOAT a = dbToLinear60(_STATE->controls[track->index][PREGAIN].lfo_min.load());
		MYFLOAT b = dbToLinear60(_STATE->controls[track->index][PREGAIN].lfo_max.load());
		grainamp_range = b - a;
		grainamp_const = a;
		grainamp = grainamp_const;
	}

	MYFLOAT pan_range = 0;
	const LFO* pan_lfo = _STATE->channels == 2 ? track->lfo[GRAINPAN].load() : nullptr;
	MYFLOAT pan_lfo_phinc;
	const bool dopanlfo = pan_lfo && pan_lfo->powertmp;
	if (dopanlfo) {
		MYFLOAT a = _STATE->controls[track->index][GRAINPAN].lfo_min.load();
		MYFLOAT b = _STATE->controls[track->index][GRAINPAN].lfo_max.load();
		pan_range = b - a;
		pan_const = a;
		pan_lfo_phinc = LOG2NORMALF(pan_lfo->freq()) / (double)sr;
	}

	const auto off_start = track->off_start_tmp;
	const auto off_stop = track->off_stop_tmp;
	MYFLOAT playbackspeed_dir = track->playbackDirTmp;
	MYFLOAT fileoffset = track->offset_tmp;
	const int32_t bounce_type = track->bounce_type_tmp;


#if defined(PLUGIN_MODE) || defined(OS_IOS)
	const bool sync_to_host =
		track->syncDawTmp;

	const bool stopped =
		sync_to_host
		? !_DATA->hostTimeSnapshot.running
		: track->stopped_tmp;

	const double loopsPerBar = track->loopsPerBarTmp;
	const double beatsPerBar = _DATA->hostTimeSnapshot.beatsPerBar;
	const double loopBeats = beatsPerBar / loopsPerBar;

	if (sync_to_host && track->requestLoopPosTmp)
	{
		double phase = std::fmod(_DATA->hostTimeSnapshot.ppqNow / loopBeats, 1.0);
		fileoffset = off_start + phase * (off_stop - off_start);
	}

	double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
	double sampleLen = off_stop - off_start;
	double secondsPerLoop = (60.0 / bpm) * loopBeats;
	double hostSpeed =
		sampleLen / (secondsPerLoop * _STATE->sr);


	MYFLOAT playbackspeed_const =
		stopped ? 0.0f :
		sync_to_host
		? (MYFLOAT)hostSpeed
		: track->speed_tmp;


	const bool dospeedlfo = false;
	const LFO* speed_lfo = track->speed_lfo_tmp;

#else
	const bool stopped = track->stopped_tmp;
	MYFLOAT playbackspeed_const = stopped ? 0 :
		track->speed_tmp;
	const LFO* speed_lfo = track->speed_lfo_tmp;

	const bool dospeedlfo = !stopped && speed_lfo != nullptr;
#endif
	MYFLOAT playbackspeed = playbackspeed_const;
	MYFLOAT speed_range = 0;
	if (dospeedlfo) {
		auto a =
			track->speed_lfo_min_tmp, b =
			track->speed_lfo_max_tmp;
		speed_range = b - a;
		playbackspeed_const = playbackspeed = a;
	}

	MYFLOAT readoffset_range = 0;
	MYFLOAT readoffset_const = 0, readoffset = 0;
	const LFO* readoffset_lfo = track->lfo[READOFFSET_LFO];
	const bool doreadoffsetlfo = readoffset_lfo && readoffset_lfo->power();
	if (doreadoffsetlfo) {
		auto a = _STATE->controls[track->index][READOFFSET_LFO].lfo_min.load(), b =
			_STATE->controls[track->index][READOFFSET_LFO].lfo_max.load();
		readoffset_range = b - a;
		readoffset = readoffset_const = a;
	}


	auto randomoffset_const = (uint32_t)_STATE->params[track->index][RNDREAD].load();
	auto randomoffset = (uint32_t)randomoffset_const;

	MYFLOAT rndread_range = 0;
	const LFO* rndread_lfo = track->lfo[RNDREAD];
	const bool dorndreadlfo = rndread_lfo && rndread_lfo->power();
	if (dorndreadlfo) {
		auto a = _STATE->controls[track->index][RNDREAD].lfo_min.load(), b =
			_STATE->controls[track->index][RNDREAD].lfo_max.load();
		rndread_range = b - a;
		randomoffset_const = randomoffset = (uint32_t)a;
	}


	MYFLOAT pitch_range = 0;
	MYFLOAT pitchlfomin = 0;
	const LFO* pitch_lfo = track->lfo[PITCH];
	const bool dopitchlfo = pitch_lfo && pitch_lfo->power();
	if (dopitchlfo) {
		MYFLOAT a = _STATE->controls[track->index][PITCH].lfo_min.load();
		MYFLOAT b = _STATE->controls[track->index][PITCH].lfo_max.load();
		pitchlfomin = a;
		pitch_range = b - a;
	}
	const MYFLOAT pitch_const = pow(2., _STATE->params[track->index][PITCH].load());

	MYFLOAT pitch_min_const = _STATE->params[track->index][PITCHMIN].load(), pitch_max_const = _STATE->params[track->index][PITCHMAX].load();
	MYFLOAT pitch_min = pitch_min_const, pitch_max = pitch_max_const;

	const int32_t intervals_min = (int)_STATE->params[track->index][SEMITONES_MIN].load(), intervals_max = (int)_STATE->params[track->index][SEMITONES_MAX].load(), interval_size = (int)_STATE->params[track->index][SEMITONES].load();


	bool randompitch = false;
	bool randomintervals = false;
	if (track->fxpower[SPACE_PITCH]) {
		randomintervals =
			(intervals_min != 0 || intervals_max != 0) && interval_size != 0;
		randompitch = pitch_min != 0 || pitch_max != 0;
	}

	MYFLOAT pitch_min_range = 0;
	const LFO* pitch_min_lfo = track->lfo[PITCHMIN];
	const bool dopitchminlfo = pitch_min_lfo && pitch_min_lfo->power();
	if (dopitchminlfo) {
		randompitch = true;
		auto a = _STATE->controls[track->index][PITCHMIN].lfo_min.load(), b =
			_STATE->controls[track->index][PITCHMIN].lfo_max.load();
		pitch_min_range = b - a;
		pitch_min_const = pitch_min = a;
	}

	MYFLOAT pitch_max_range = 0;
	const LFO* pitch_max_lfo = track->lfo[PITCHMAX];
	const bool dopitchmaxlfo = pitch_max_lfo && pitch_max_lfo->power();
	if (dopitchmaxlfo) {
		randompitch = true;
		auto a = _STATE->controls[track->index][PITCHMAX].lfo_min.load(), b = _STATE->controls[track->index][PITCHMAX].lfo_max.load();
		pitch_max_range = b - a;
		pitch_max_const = pitch_max = a;
	}

	MYFLOAT semitones_range = 0;
	const LFO* semitones_lfo = track->lfo[SEMITONES_LFO];
	MYFLOAT semitoneslfomin = 0;
	const bool dosemitoneslfo = semitones_lfo && semitones_lfo->power();
	if (dosemitoneslfo) {
		auto a = _STATE->controls[track->index][SEMITONES_LFO].lfo_min.load(),
			b = _STATE->controls[track->index][SEMITONES_LFO].lfo_max.load();
		semitoneslfomin = a;
		semitones_range = b - a;
	}

	MYFLOAT looppos_range = 0;
	const LFO* looppos_lfo = track->looppos_lfo_tmp;
	MYFLOAT loopposlfomin = 0;

	const bool doloopposlfo =
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		sync_to_host ? false :
#endif
		!stopped && looppos_lfo != nullptr;

	if (doloopposlfo) {
		const MYFLOAT r = DISTANCE(off_start, off_stop);
		MYFLOAT a = track->looppos_lfo_min_tmp,
			b = track->looppos_lfo_max_tmp;
		loopposlfomin = std::min(off_start, off_stop) + a * r;
		looppos_range = (b - a) * r;
	}

	MYFLOAT gliss_range = 0;
	const LFO* gliss_lfo = track->lfo[GRAINGLISS];
	MYFLOAT glissconst;

	const bool doglisslfo = gliss_lfo && gliss_lfo->power();
	if (doglisslfo) {
		MYFLOAT a = _STATE->controls[track->index][GRAINGLISS].lfo_min.load(),
			b = _STATE->controls[track->index][GRAINGLISS].lfo_max.load();
		glissconst = a;
		gliss_range = b - a;
	}
	else
		glissconst = _STATE->params[track->index][GRAINGLISS].load();




	auto tindex = track->index;

	//LOGE("%g %g", step_length_swin, step_point_swin);

	const bool bandlimited_env = track->oldbandlimitedgrainenv == 1.0;
	auto& bandlimitedEnvtable1 = track->bandlimitedEnvtable[0];
	auto& bandlimitedEnvtable2 = track->bandlimitedEnvtable[1];
	auto& bandlimitedEnvtable3 = track->bandlimitedEnvtable[2];
	const auto& window1 = track->grainenv[0];
	const auto& window2 = track->grainenv[1];
	const auto& window3 = track->grainenv[2];


	const bool envonly = _STATE->params[track->index][NOINPUTFROMTRACK].load() == 1.0;

	auto hanningwin = _DATA->hanningwin;

	auto filebuffer = source->filebuffer.load();
	const auto off = filebuffer == nullptr ? 0 : filebuffer->buffer[channel].size();
	auto filebuffer1 = off > 0 ? filebuffer->buffer[channel].data() : nullptr;
	const bool isresizing = off == 0 || filebuffer->isresizing.load();


	const auto panenv = _DATA->pan[channel];

	const auto grainsizemin = (int)_STATE->parameters[GRAINSIZE].min;


	const auto reverseParam = _STATE->params[track->index][REVERSEGRAINS].load();
	const bool hqresampling = _STATE->params[track->index][HQ_RESAMPLING] == 1.0;
#ifdef IS_MULTITHREADED
	auto& cross_barrier = track->cross_barrier[channel];
#endif

#if defined DOES_INPUT_RESAMPLING
	const auto dawGrainInputGain = dbToLinear60(_STATE->params[track->index][GRAININPUT_GAIN_DAW].load());
	const auto grainGain = dbToLinear60(_STATE->params[track->index][INPUT_GAIN_SAMPLER].load());
	const auto inputMask = _STATE->inputMask;
	const auto inputSize = inputMask + 1;
	auto inBuf = _STATE->inputBuf[channel];
#endif

	auto awin = _DATA->hanningwin;


	const int func = locker->cross_func_tmp;

#ifdef IS_MULTITHREADED
	bool exitloop{};
	while (!exitloop) {
		MYFLOAT step_point{}, step_length{};
		bool silence{};
		if (isMultiThreaded) {
			locker->cross_barrier[channel].wait_rt(step_point, step_length, exitloop, silence);
			if (exitloop)break;
		}
		else {
			exitloop = true;
#else
	MYFLOAT step_point{}, step_length{}, silence{};
#endif
	step_point = locker->step_point_grain[channel], step_length = locker->stepLengthTmp[channel];
	silence = locker->silenceTmp[channel];
#ifdef IS_MULTITHREADED
		}
#endif
track->step_point_grain[channel] = step_point;

if (!silence) {
	if (dograinamplfo) {
		grainamp = (MYFLOAT)(grainamp_const +
			grainamp_lfo->buf[(int)step_point] * grainamp_range);
	}


	if (dorndreadlfo) {
		randomoffset = (uint32_t)(randomoffset_const +
			rndread_lfo->buf[(int)step_point] * rndread_range);
	}

	if (dopitchminlfo) {
		pitch_min = pitch_min_const +
			pitch_min_lfo->buf[(int)step_point] * pitch_min_range;

	}

	if (dopitchmaxlfo) {
		pitch_max = pitch_max_const +
			pitch_max_lfo->buf[(int)step_point] * pitch_max_range;

	}



	auto pitch = pitch_const * seqpitch * (dopitchlfo ? pow(2.,
		pitchlfomin +
		pitch_lfo->buf[(int)step_point] *
		pitch_range)
		: 1) * (randomintervals ? pow(2.,
			irand(intervals_min,
				intervals_max) *
			interval_size /
			12.)
			: 1) *
		(randompitch ? pow(2., tsl::random::randomfloat(pitch_min,
			pitch_max)) : 1) *
		(dosemitoneslfo ? pow(2.,
			(round(semitoneslfomin +
				semitones_lfo->buf[(int)step_point] *
				semitones_range) /
				12.))
			: 1);
	if (pitch > 8.)
		pitch = 8.;
	else if (pitch < .125)
		pitch = .125;

	track->pitchfact[channel] = pitch;
	track->glissfact[channel] = pow(2., doglisslfo ? glissconst +
		gliss_lfo->buf[(int)step_point] * gliss_range
		: glissconst);


	auto gliss = pitch * track->glissfact[channel];
	if (gliss > 8.)
		gliss = 8.;
	else if (gliss < .125)
		gliss = .125;

	const auto pitchinc = (gliss - pitch) / (double)grainsize;

	if (doreadoffsetlfo) {
		readoffset = readoffset_const +
			readoffset_lfo->buf[(int)step_point] * readoffset_range;
	}

	if (doloopposlfo)
		fileoffset = loopposlfomin + looppos_range * looppos_lfo->buf[(int)step_point];

	auto tempoffset =
		(randomoffset ? fileoffset + readoffset + irand(0, randomoffset) :
			fileoffset + readoffset);

#if defined DOES_INPUT_RESAMPLING
	auto tempoffsetDaw =
		(randomoffset ? _STATE->inputPos + step_point - readoffset - irand(0, randomoffset) :
			_STATE->inputPos + step_point - readoffset);
#endif


#if defined DOES_INPUT_RESAMPLING
	if (off != 0)
#endif
		track->tmpoffset[channel].store(tempoffset);

	int forward = playbackspeed_dir == 1 ||
		(playbackspeed_dir == -1 && bounce_type == BOUNCE_NO_REVERSE)
		? 1
		: -1;
	if (tsl::random::randomfloat(0, 1) < reverseParam) forward *= -1;


	if (pitch != 1. || pitchinc != 0.) {
		if (pitchinc == 0 && hqresampling) {
			if (track->rst[channel] == nullptr)
				track->rst[channel] =
				std::make_unique<PolyPhaseResampler<MYFLOAT, 32, 256 >>();
			auto& rs = track->rst[channel];
			rs->initRounded(pitch);
			if (forward == 1) {
				auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
				auto inputPos = (static_cast<int>(tempoffsetDaw - grainsize * pitch)) & inputMask;
#endif
				for (int i = 0; i < grainsize; i++) {
					while (rs->isWriteNeeded()) {
						if (to >= off || to < 0) {
							rs->writeNextFrame(0.0
#if defined DOES_INPUT_RESAMPLING
								+ inBuf[inputPos] * dawGrainInputGain
#endif											
							);
						}
						else {
							rs->writeNextFrame(filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
								* grainGain + inBuf[inputPos] * dawGrainInputGain
#endif											
							);
						}
						to++;
#if defined DOES_INPUT_RESAMPLING
						inputPos = (inputPos + 1) & inputMask;
#endif											
					}
					grainbuffer[i] = rs->readNextFrame();
				}
			}
			else {
				auto to = static_cast<int>(tempoffset + grainsize * pitch);
#if defined DOES_INPUT_RESAMPLING
				auto inputPos = static_cast<int>(tempoffsetDaw) & inputMask;
#endif
				for (int i = 0; i < grainsize; i++) {
					while (rs->isWriteNeeded()) {
						if (to >= off || to < 0) {
							rs->writeNextFrame(0.0
#if DOES_INPUT_RESAMPLING
								+ inBuf[inputPos] * dawGrainInputGain
#endif											
							);
						}
						else {
							rs->writeNextFrame(filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
								* grainGain + inBuf[inputPos] * dawGrainInputGain
#endif																						
							);
						}
						to--;
#if defined DOES_INPUT_RESAMPLING
						inputPos = (inputPos - 1) & inputMask;
#endif
					}
					grainbuffer[i] = rs->readNextFrame();
				}
			}
			rs->clear();
		}
		else {
			if (forward) {
#if DOES_INPUT_RESAMPLING
				auto inputPos = tempoffsetDaw - grainsize * pitch;
				// initial wrap
				while (inputPos < 0.0)
					inputPos += inputSize;
				while (inputPos >= inputSize)
					inputPos -= inputSize;
#endif
				for (int i = 0; i < grainsize; i++) {
					auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
					int   i0 = static_cast<int>(inputPos);
					int   i1 = (i0 + 1) & inputMask;
					double frac = inputPos - static_cast<double>(i0);
					auto inputSample = (inBuf[i0] +
						frac *
						(inBuf[i1] -
							inBuf[i0])) * dawGrainInputGain;
#endif
					if (to + 1 >= off || to < 0) {
						grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
						grainbuffer[i] += inputSample;
						tempoffset += pitch;
						inputPos += pitch;
						while (inputPos >= inputSize)
							inputPos -= inputSize;
						pitch += pitchinc;
						if (pitch > 8.)pitch = 8.;
						else if (pitch < .125)pitch = .125;
#endif

						continue;
					}
					int v1 = to;
					int v2 = v1 + 1;

					grainbuffer[i] = (MYFLOAT)(filebuffer1[v1] +
						(tempoffset - v1) *
						(filebuffer1[v2] -
							filebuffer1[v1])) *
						CONVMYFLT
#ifdef DOES_INPUT_RESAMPLING
						* grainGain + inputSample;
#endif
					;

					tempoffset += pitch;
#ifdef DOES_INPUT_RESAMPLING
					inputPos += pitch;
					while (inputPos >= inputSize)
						inputPos -= inputSize;
#endif
					pitch += pitchinc;
					if (pitch > 8.)pitch = 8.;
					else if (pitch < .125)pitch = .125;
				}
			}
			else {
				tempoffset += grainsize * pitch;
#if defined DOES_INPUT_RESAMPLING
				auto inputPos = tempoffsetDaw;
				while (inputPos < 0.0)
					inputPos += inputSize;
				while (inputPos >= inputSize)
					inputPos -= inputSize;
#endif
				for (int i = 0; i < grainsize; i++) {
					auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
					int   i0 = static_cast<int>(inputPos);
					int   i1 = (i0 + 1) & inputMask;
					double frac = inputPos - static_cast<double>(i0);
					auto inputSample = (inBuf[i0] +
						frac *
						(inBuf[i1] -
							inBuf[i0])) * dawGrainInputGain;
#endif
					if (to + 1 >= off || to < 0) {
						grainbuffer[i] = 0.0;
#ifdef DOES_INPUT_RESAMPLING
						grainbuffer[i] += inputSample;
						tempoffset -= pitch;
						inputPos -= pitch;
						while (inputPos < 0)
							inputPos += inputSize;
						pitch += pitchinc;
						if (pitch > 8.)pitch = 8.;
						else if (pitch < .125)pitch = .125;
#endif
						continue;
					}
					int v1 = to;
					int v2 = v1 + 1;

					grainbuffer[i] = (MYFLOAT)(filebuffer1[v1] +
						(tempoffset - v1) *
						(filebuffer1[v2] -
							filebuffer1[v1])) *
						CONVMYFLT
#ifdef DOES_INPUT_RESAMPLING
						* grainGain + inputSample;
#endif
					;
					tempoffset -= pitch;
#ifdef DOES_INPUT_RESAMPLING
					inputPos -= pitch;
					while (inputPos < 0)
						inputPos += inputSize;
#endif
					pitch += pitchinc;
					if (pitch > 8.)pitch = 8.;
					else if (pitch < .125)pitch = .125;
				}
			}
		}
	}
	else {
		if (forward == 1) {
			auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
			auto inputPos = (static_cast<int>(tempoffsetDaw - grainsize)) & inputMask;
#endif
			for (int32_t i = 0; i < grainsize; i++) {
				if (to >= off || to < 0) {
					grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
					grainbuffer[i] += inBuf[inputPos] * dawGrainInputGain;
					inputPos = (inputPos + 1) & inputMask;
#endif
				}
				else {
					grainbuffer[i] = (MYFLOAT)filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
						* grainGain + inBuf[inputPos] * dawGrainInputGain;
#endif                              
					;
					to++;
#if defined DOES_INPUT_RESAMPLING
					inputPos = (inputPos + 1) & inputMask;
#endif                              
				}
			}
		}
		else {
			auto to = static_cast<int>(tempoffset + grainsize);
#if defined DOES_INPUT_RESAMPLING
			auto inputPos = static_cast<int>(tempoffsetDaw) & inputMask;
#endif
			for (int i = 0; i < grainsize; i++) {
				if (to >= off || to < 0) {
					grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
					grainbuffer[i] += inBuf[inputPos] * dawGrainInputGain;
					inputPos = (inputPos - 1) & inputMask;
#endif                              
					continue;
				}
				grainbuffer[i] = (MYFLOAT)filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
					* grainGain + inBuf[inputPos] * dawGrainInputGain;
#endif                              
				;
				to--;
#if defined DOES_INPUT_RESAMPLING
				inputPos = (inputPos - 1) & inputMask;
#endif                              
			}
		}
	}

	MYFLOAT sp = 0;
	const  MYFLOAT sl = 1. / (MYFLOAT)grainsize;
	for (int i = 0; i < grainsize; i++) {
		grainbuffer[i] *= awin[PHS2INT(sp)] * grainamp;
		sp += sl;
	}


	auto n = track->fx_queue_grain[channel]._first;
	while (n) {
		auto next = n->next;
		if (n->data->readyToDestroy)
			track->fx_queue_grain[channel].del(n);
		else
			n->data->compute(grainbuffer, grainsize);
		n = next;
	}



	// memcpy(fft_in, grainbuffer, grainsize * sizeof(float));
	MYFLOAT max = 0;
	for (int i = 0; i < grainsize; i++) {
		auto tmp = std::abs(grainbuffer[i]);
		max = max > tmp ? max : tmp;
	}

	track->maxTmp[channel] = max;

	if (track->pv_power_tmp)
		pv_funcs[track->pv_func_tmp](track, channel, grainsize, playbackspeed);

	if (func == CROSS_FILTER1) {
		CHECKFFT(grainsize)
			int32_t cut_off = (int)floor(grainsize * (CUTOFFMIN + (CUTOFFRANGE *
				(LOG2NORMAL2F(
					_STATE->params[locker->index][CROSSCEP1CUTMOD].load()) -
					CUTOFFMIN))));
		tsl::fft::cepstrum(fft, grainbuffer, fft_out, fft_help1, fft_help2, grainsize,
			cut_off, true);
		for (int32_t i = 0; i < grainsize; i++) {
			UDD(grainbuffer[i])
				UDD(fft_out[i])
		}
	}
	else if (func == CROSS_FILTER2) {
		CHECKFFT(grainsize)
			int32_t cut_off = (int)floor(grainsize * (CUTOFFMIN + (CUTOFFRANGE *
				(LOG2NORMAL2F(
					_STATE->params[locker->index][CROSSCEP2CUTMOD].load()) -
					CUTOFFMIN))));
		tsl::fft::cepstrum(fft, grainbuffer, fft_out, fft_help1, fft_help2, grainsize,
			cut_off, false);
		for (int32_t i = 0; i < grainsize; i++) {
			UDD(grainbuffer[i])
				UDD(fft_out[i])
		}
	}
	else if (func == CROSS_INTER) {
		CHECKFFT(grainsize)
			tsl::fft::fftshift(grainbuffer, grainsize);
		int32_t cut_off = (int)floor(grainsize * (CUTOFFMIN + (CUTOFFRANGE *
			(LOG2NORMAL2F(
				_STATE->params[locker->index][CROSSINTCUTMOD].load()) -
				CUTOFFMIN))));
		tsl::fft::cepstrum(fft, grainbuffer, fft_out, fft_help1, fft_help2, grainsize,
			cut_off, false);
		for (int32_t i = 0; i < grainsize; i++) {
			UDD(grainbuffer[i])
				UDD(fft_out[i])
				UDD(fft_help1[i])
		}
	}
	else if (func == CROSS_POLAR || func == CROSS_TRANSPORT || func == CROSS_DUCK) {
		// CROSS_TRANSPORT wants the same thing CROSS_POLAR does: magnitudes at
		// the even slots. It builds its cumulative distribution from them.
		// CROSS_DUCK too: its gate compares those magnitudes to the frame RMS.
		CHECKFFT(grainsize)
			fft->forwardPolar(grainbuffer, fft_out);
		for (int32_t i = 0; i < grainsize; i++) {
			UDF(fft_out[i])
		}
	}
	else if (func == CROSS_STACK) {
		// The modulator's strongest spectral peaks, handed over as
		// { count, bin, mag, ... } ascending in frequency. Picking them here
		// rather than on the carrier side keeps the analysis with the signal it
		// describes, and keeps the carrier's inner loop to the resampling.
		CHECKFFT(grainsize)
			fft->forwardPolar(grainbuffer, fft_help1);
		const int32_t M2 = grainsize >> 1;
		// Always the full CROSS_STACK_MAX_PEAKS strongest. The carrier narrows
		// this down to PEAKS itself, so that knob can take an LFO on the carrier's
		// own track like every other cross parameter, instead of this side having
		// to reach across into the carrier's LFO buffers.
		const int32_t want = CROSS_STACK_MAX_PEAKS;

		MYFLOAT bins[CROSS_STACK_MAX_PEAKS]{}, mags[CROSS_STACK_MAX_PEAKS]{};
		int32_t n = 0;
		for (int32_t j = 2; j < M2 - 2; ) {
			const MYFLOAT m = fft_help1[j * 2];
			if (m > 0. && std::isfinite(m) &&
				m > fft_help1[(j - 1) * 2] && m > fft_help1[(j - 2) * 2] &&
				m > fft_help1[(j + 1) * 2] && m > fft_help1[(j + 2) * 2]) {
				// Parabolic refinement: the carrier turns these into transposition
				// ratios, so a half-bin error is a detuned copy.
				const MYFLOAT a1 = fft_help1[(j - 1) * 2], a3 = fft_help1[(j + 1) * 2];
				const MYFLOAT den = a1 - 2. * m + a3;
				MYFLOAT d = den != 0. ? .5 * (a1 - a3) / den : 0.;
				if (d > .5) d = .5; else if (d < -.5) d = -.5;
				if (!std::isfinite(d)) d = 0.;

				if (n < want) { bins[n] = (MYFLOAT)j + d; mags[n] = m; ++n; }
				else {
					int32_t worst = 0;
					for (int32_t q = 1; q < n; ++q) if (mags[q] < mags[worst]) worst = q;
					if (m > mags[worst]) { mags[worst] = m; bins[worst] = (MYFLOAT)j + d; }
				}
				j += 3;
			}
			else ++j;
		}
		// Ascending by frequency: the carrier takes the first entry as the root.
		for (int32_t a = 1; a < n; ++a) {
			const MYFLOAT bb = bins[a], mm = mags[a];
			int32_t c = a - 1;
			while (c >= 0 && bins[c] > bb) { bins[c + 1] = bins[c]; mags[c + 1] = mags[c]; --c; }
			bins[c + 1] = bb; mags[c + 1] = mm;
		}
		fft_out[0] = (MYFLOAT)n;
		for (int32_t a = 0; a < n; ++a) {
			fft_out[1 + 2 * a] = bins[a];
			fft_out[2 + 2 * a] = mags[a];
		}
	}
	else if (func == CROSS_CONV) {
		auto emph = _STATE->params[locker->index][CROSSCEP1EMPH].load() * .99;
		MYFLOAT tmp = 0;
		for (int i = 0; i < grainsize; i++) {
			auto tt = grainbuffer[i];
			grainbuffer[i] -= tmp * emph;
			tmp = tt;
		}
		auto M = static_cast<unsigned>(grainsize) << 1u;
		CHECKFFT(M)
			std::memset(grainbuffer + grainsize, 0, grainsize * sizeof(MYFLOAT));
		fft->forward(grainbuffer, fft_out);

	}
	else if (func == CROSS_LPC) {
		auto M = static_cast<unsigned>(grainsize) << 1u;
		CHECKFFT(M)
			// Reflection coefficients (fft_out[1..order]), not direct-form a[k]:
			// cross_lpc interpolates them across the grain and only |k| < 1
			// survives a linear blend. The lag window + bandwidth expansion that
			// keeps a high-order fit on a short grain from ringing lives inside
			// computeReflection.
			LPC2::computeReflection(grainbuffer, fft_out, fft_help1,
				locker->crossLPCOrderTmp, grainsize, *fft, _STATE->sr);
	}
	else {
		CHECKFFT(grainsize)
			fft->forward(grainbuffer, fft_out);
		for (int32_t i = 0; i < grainsize; i++) {
			UDF(fft_out[i])
		}

	}
}
#ifdef IS_MULTITHREADED
if (isMultiThreaded)
track->sem_cross[channel].signal();
#endif

if (!doloopposlfo)
// One playback hop per grain: step_length (the carrier's grain spacing,
// sr/graindens, passed via the barrier in MT or stepLengthTmp in non-MT) times
// the modulator's own playbackspeed. This keeps the modulator's read position
// in step with the carrier's grain rate.
fileoffset += playbackspeed * playbackspeed_dir * step_length;
if (fileoffset > off_stop && bounce_type != NO_BOUNCE) {
	if (isresizing)
		fileoffset = off_stop;
	else
		fileoffset = off_stop - DISTANCE(off_stop, fileoffset);
	if (fileoffset <= off_start)
		fileoffset = off_stop;
	playbackspeed_dir *= -1;
}
else if (fileoffset < off_start && bounce_type != NO_BOUNCE) {
	if (isresizing)
		fileoffset = off_start;
	else
		fileoffset = off_start + DISTANCE(off_start, fileoffset);
	if (fileoffset >= off_stop)
		fileoffset = off_start;
	playbackspeed_dir *= -1;
}
else if (fileoffset < off_start) {
	if (isresizing)
		fileoffset = off_stop;
	else {
		fileoffset = off_stop - DISTANCE(off_start, fileoffset);
		if (fileoffset < off_start)
			fileoffset = off_stop;
	}

}
else if (fileoffset > off_stop) {
	if (isresizing)
		fileoffset = off_start;
	else {
		fileoffset = off_start + DISTANCE(off_stop, fileoffset);
		if (fileoffset > off_stop)
			fileoffset = off_start;
	}
}
#ifdef IS_MULTITHREADED
	}
#endif
	std::memset(track->envf_buffer[channel], 0, sizeof(MYFLOAT) * _STATE->currentBufSize);
	if (channel == 0) {
		if (!isMultiThreaded) {
			track->offset_tmp = fileoffset;
			// Persist the read position on EVERY grain (last wins). In non-MT
			// compute_fft runs once per grain, and the updated_this_cycle CAS
			// below only fires on the first grain of the cycle -- gating the
			// state->offset write on it stored the FIRST grain's offset, and
			// setupTracks reloads offset_tmp from state->offset each buffer, so
			// the modulator's read position jumped back ~a buffer every cycle
			// (audible as a stalled/2x-ish modulator, factor depends on
			// graindens vs buffer size). In MT this block runs once after the
			// grain loop with the final offset, so it is already correct there.
			auto st = source->currentState;
			if (off > 0 && st && !filebuffer->ismoving.load())
				st->offset.store(fileoffset, std::memory_order_relaxed);
		}
		bool expected = false;

		if (source->updated_this_cycle.compare_exchange_strong(
			expected, true, std::memory_order_acq_rel)) {
			if (off > 0) {
				auto state = source->currentState;
				if (state) {
					if (state->playbackDir.exchange(playbackspeed_dir) != playbackspeed_dir)
						_DATA->snapShot.queue.try_push(tsl::parameters::Event::createEvent(source->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, playbackspeed_dir, tsl::parameters::EventSubtype::playbackDir));
					if (!filebuffer->ismoving.load()) {
						if (auto old = state->offset.exchange(fileoffset) != fileoffset)
							_DATA->snapShot.queue.try_push(tsl::parameters::Event::createEvent(source->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, fileoffset, tsl::parameters::EventSubtype::offset));
					}
				}
			}


		}
	}

}
