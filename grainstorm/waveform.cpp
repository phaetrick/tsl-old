#include "waveform.h"
#include "logger.h"
#include <cstdlib>
#include <cstring>
#include <SkCanvas.h>
#include <SkPath.h>
#include <SkSurface.h>
#include <defines.h>
#include "grainstorm.h"
#include "infopanel.h"
#include "view.h"
#include "colours.h"
#include "tools.h"
#include "track.h"
#include "synth.h"
#include "DecoderView.h"
#include "Input.h"
#include "button.h"
#include "buttonview.h"
#include "tools/StackVector.h"
#if defined USE_IMGUI
#include <imgui.h>
#else

#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/gl/GrGLInterface.h>
#include <include/gpu/ganesh/gl/GrGLDirectContext.h>
#include <include/gpu/ganesh/gl/GrGLBackendSurface.h>
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "MidiLearning.h"

#endif
#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif
using namespace tsl::graphics;

WAVE_S::~WAVE_S() {
	if (next)delete next;
}

Waveform::Waveform(tsl::AppState* appState, TRACK* _track) : _appState{ appState } {
	associatedtrack = _track;
	overlap = true;
	_track->waveform = this;
	orientation = VERTICAL;
}

Waveform2::Waveform2(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment)
	: View(appState,
		_scalefactor, _aspect_ratio, _alignment, 10, true, "Waveform"),
	waveforms(Waveform{ appState, appState->data->tracks[0] },
		Waveform{ appState, appState->data->tracks[1] },
		Waveform{ appState, appState->data->tracks[2] },
		Waveform{ appState, appState->data->tracks[3] }) {
	orientation = VERTICAL;
}


void Waveform::init() {
	//int width = view->width;
	//LOGE("init waveform %s : startx: %g stopx: %g starty: %g stopy: %g, width: %g height: %g", tem_DATA->name, tem_DATA->startx, tem_DATA->stopx, tem_DATA->starty, tem_DATA->stopy, tem_DATA->width, tem_DATA->height);
	orientation = width > height ? HORIZONTAL : VERTICAL;
	_STATE->WorkerQueue.add_task([this]() {
		auto rec = renderData.load();
		if (rec != nullptr)
			setup(rec->filebuffer);
		});
}

void Waveform2::init() {
	//int width = view->width;
	//LOGE("init waveform %s : startx: %g stopx: %g starty: %g stopy: %g, width: %g height: %g", tem_DATA->name, tem_DATA->startx, tem_DATA->stopx, tem_DATA->starty, tem_DATA->stopy, tem_DATA->width, tem_DATA->height);
	orientation = width > height ? HORIZONTAL : VERTICAL;
	if (orientation == VERTICAL) {
		markerheight = (int)(height / 7.);
		markerwidth = (int)(markerheight / 3.);

		markerheight_half = markerheight / 2.f;
		markerwidth_half = markerwidth / 2.f;
		width_triangle = markerwidth / 4.f,
			height_triangle = markerheight / 8.f,
			height_rect = markerheight - 2,
			corner_radius = height_rect / 20.f;
	}
	else {
		markerwidth = (int)(width / 7.f);
		markerheight = (int)(markerwidth / 3.f);
		markerheight_half = markerheight / 2.f;
		markerwidth_half = markerwidth / 2.f;
		width_triangle = markerheight / 4.f;
		height_triangle = markerwidth / 8.f;
		height_rect = markerwidth - 2;
		corner_radius = height_rect / 20.f;
	}
	for (auto t : _DATA->tracks) {
		t->waveform->markerwidth = markerwidth;
		t->waveform->markerheight = markerheight;
		t->waveform->markerheight_half = markerheight_half;
		t->waveform->markerwidth_half = markerwidth_half;
		t->waveform->width_triangle = width_triangle;
		t->waveform->height_triangle = height_triangle;
		t->waveform->height_rect = height_rect;
		t->waveform->corner_radius = corner_radius;
		t->waveform->width = width.load(std::memory_order_acquire);
		t->waveform->height = height.load(std::memory_order_acquire);
		t->waveform->startx = startx.load(std::memory_order_acquire);
		t->waveform->starty = starty.load(std::memory_order_acquire);
		t->waveform->stopx = stopx.load(std::memory_order_acquire);
		t->waveform->stopy = stopy.load(std::memory_order_acquire);
		t->waveform->init();
	}

	//  _STATE->worker_queue.add_task([_appState]() {ww->setup(); });

}


void Waveform::delRecursiveCB() {
	pointers.clear();
	auto data = renderData.load(std::memory_order_acquire);
	if (data && data->filebuffer) {
		data->filebuffer->isresizing.store(false, std::memory_order_release);
		data->filebuffer->ismoving.store(false, std::memory_order_release);
	}
}

void Waveform2::delRecursiveCB() {
	{
		std::lock_guard lock(mtx);
		gestureTarget = nullptr;
		gesturePointers = 0;
	}
	for (auto t : _DATA->tracks) {
		t->waveform->delRecursiveCB();
	}
	View::delRecursiveCB();
}

void Waveform2::callback(const InputEvent& event) {
	auto resolve = [this]() -> Waveform* {
		TRACK* active = _DATA->tracks[_STATE->active_track.load(std::memory_order_acquire)];
		TRACK* source =
			_DATA->tracks[(int)_STATE->params[active->index][DISTRSOURCE].load(std::memory_order_acquire)];
		TRACK* track = (active->fxpower[SPACE_LOOPER].load(std::memory_order_acquire) ==
			source->fxpower[SPACE_LOOPER].load(std::memory_order_acquire)) ? source : active;
		return (Waveform*)track->waveform;
	};
	std::lock_guard lock(mtx);
	if (event.action == ACTION_DOWN && gesturePointers == 0)
		gestureTarget = resolve();
	// Non-gesture events (wheel, keys) keep the per-event routing.
	Waveform* target = gestureTarget ? gestureTarget : resolve();
	if (event.action == ACTION_DOWN)
		++gesturePointers;
	target->callback(event);
	if (event.action == ACTION_UP && gesturePointers > 0 && --gesturePointers == 0)
		gestureTarget = nullptr;
}

struct alignas(4) WaveformData {
	int8_t playbackDir{};
	int8_t offsetMoved{};
	uint16_t reserved{};
};
static_assert(sizeof(WaveformData) == sizeof(uint32_t));

void Waveform::callback(const InputEvent& event) {
	// Pre-load shared pointers and validate
	auto data = renderData.load();
	auto off = data == nullptr || data->filebuffer == nullptr ? 0 : data->filebuffer->off;

	if (off == 0)return;
	auto filebuffer = data->filebuffer;
	auto state = filebuffer->state.load();
	if (!state)return;
	TRACK* track = associatedtrack;
	const bool hoz = orientation == HORIZONTAL;
	const double _xpos = event.x - startx;
	const double _ypos = event.y - starty;
	const int action = event.action;
	int pointerid = event.pointer_id;




	switch (action) {
	case ACTION_DOWN: {

		auto ws = state->waveformState.load(std::memory_order_acquire);
		// Cache frequently accessed atomic loads
		const double _startpos = ws.startPos;
		const double _zoom = ws.zoom;
		const double startframe = _startpos * off;
		const double framesvisible = off / _zoom;
		const double framesperpixel = framesvisible / (double)(hoz ? width : height);



		// Pre-calculate common values to avoid repetition
		const double offset = state->offset.load(std::memory_order_acquire);
		const double off_start = state->off_start.load(std::memory_order_acquire);
		const double off_stop = state->off_stop.load(std::memory_order_acquire);
		const double pos_start = (off_start - startframe) / framesperpixel;
		const double pos_stop = (off_stop - startframe) / framesperpixel;
		const double pos_grain = (offset - startframe) / framesperpixel;

		const auto _last = data->last.load(std::memory_order_acquire);
		const bool is_looper = track->fxpower[SPACE_LOOPER].load(std::memory_order_acquire);

		// Store loop positions once if needed
		if (is_looper) {
			data->posStartLoop.store(off_start, std::memory_order_release);
			data->posStopLoop.store(off_stop, std::memory_order_release);
			data->offsetLoop.store(offset, std::memory_order_release);
		}

		// Helper lambda for marker hit testing

		auto isInMarkerRange = [](double pos, double target, double half_width) {
			return pos >= target - half_width && pos <= target + half_width;
			};

		WaveformMode which{ OFF_STOP };

		const float markerPos = hoz ? _xpos : _ypos;   // along-axis
		const float markerCross = hoz ? _ypos : _xpos;   // cross-axis
		const float markerHalf = hoz ? markerwidth_half : markerheight_half;
		const float markerSize = hoz ? markerheight : markerwidth;
		const float crossSize = hoz ? height : width;   // cross-axis dimension, not along-axis
		const float dimSize = hoz ? width : height;
		const float grainCross = hoz ? height : width;
		const bool  grainCrossHit = hoz ? markerCross > grainCross - markerSize
			: markerCross > grainCross - markerSize;

		auto isInMarkerRangeStart = isInMarkerRange(markerPos, pos_start, markerHalf) && markerCross < markerSize;
		auto isInMarkerRangeStop = isInMarkerRange(markerPos, pos_stop, markerHalf) && markerCross < markerSize;

		if (isInMarkerRangeStart || isInMarkerRangeStop) {
			if (isInMarkerRangeStart && isInMarkerRangeStop) {
				which = _last;
				if (off_start < 1.0 && off_stop < 1.0) which = OFF_STOP;
				else if (off_start > off - 2 && off_stop > off - 2) which = OFF_START;
			}
			else if (isInMarkerRangeStart) {
				which = OFF_START;
			}
			if (_STATE->midilearning.load() && !_STATE->midilearning_waiting.load()) {
				_STATE->UiTasksQueue.add_task([this, id = which == OFF_START ? LOOPSTARTFROMEXT : LOOPSTOPFROMEXT, t = associatedtrack->index] {
					MidiLearning::wait(_STATE, t, id); });
				return;
			}

			filebuffer->isresizing.store(true, std::memory_order_release);
			data->last.store(which, std::memory_order_release);
			tsl::graphics::InputSystem::Pointer pointer(pointerid, _xpos, _ypos,
				tsl::graphics::InputSystem::WinState::WINPOINTER, which);


			if (which == OFF_START) {
				pointer.userData1 = static_cast<uint32_t>(off_start);
			}
			else {
				pointer.userData1 = static_cast<uint32_t>(off_stop);
			}
			pointer.userData2 = static_cast<uint32_t>(offset);

			auto pd = state->playbackDir.load();
			WaveformData data{};
			data.playbackDir = static_cast<int8_t>(state->playbackDir.load());
			if (hoz) pointer.userData4 = std::bit_cast<uint32_t>(data);
			else     pointer.userData3 = std::bit_cast<uint32_t>(data);
			pointers.addPointer(pointer);
			
		}
		else if (isInMarkerRange(markerPos, pos_grain, markerHalf) &&
			markerCross > crossSize - markerSize) {
			which = OFF_GRAIN;
			if (_STATE->midilearning.load() && !_STATE->midilearning_waiting.load()) {
				_STATE->UiTasksQueue.add_task([this, id = LOOPPOSFROMEXT, t = associatedtrack->index] {
					MidiLearning::wait(_STATE, t, id); });
				return;
			}

			filebuffer->ismoving.store(true, std::memory_order_release);
			tsl::graphics::InputSystem::Pointer pointer(pointerid, _xpos, _ypos,
				tsl::graphics::InputSystem::WinState::WINPOINTER, OFF_GRAIN);
			pointer.userData1 = static_cast<uint32_t>(offset);
			pointers.addPointer(pointer);


		}
		else {
			std::array<tsl::graphics::InputSystem::Pointer*, 8> drag_pointers{};
			size_t drag_count = pointers.getPointersByMode(
				tsl::graphics::InputSystem::WinState::WINDRAG, drag_pointers);

			if (drag_count == 0) {
				tsl::graphics::InputSystem::Pointer pointer(pointerid, _xpos, _ypos,
					tsl::graphics::InputSystem::WinState::WINDRAG, WAVE);
				pointers.addPointer(pointer);
				if (pointers.timer.elapsedReplace() < .3) {

					auto bt = state->bounceType.load();
					if (++bt >= NUM_BOUNCE_TYPES) bt = 0;
					state->bounceType.store(bt);
						_DATA->snapShot.addEvent({ tsl::parameters::Event::createEvent(
							associatedtrack->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, bt, tsl::parameters::EventSubtype::bounceType, 0, tsl::parameters::Event::History | tsl::parameters::Event::Info) });

				}
			}
			else if (drag_count == 1) {
				tsl::graphics::InputSystem::Pointer pointer(pointerid, _xpos, _ypos,
					tsl::graphics::InputSystem::WinState::WINDRAG, ZOOMWAVEFORM);
				pointers.addPointer(pointer);
				drag_pointers[0]->mode = tsl::graphics::InputSystem::WinState::WINDRAG;
				//drag_pointers[0]->target = ZOOMWAVEFORM;
				olddist = spacing(drag_pointers[0]->xpos, _xpos, drag_pointers[0]->ypos, _ypos);
			}
		}
		break;
	}

	case ACTION_UP: {
		auto hasChanged = [hoz](tsl::graphics::InputSystem::Pointer& p) {
			if (hoz)return p.xpos != p.xposWhenCreated;
			else return p.ypos != p.yposWhenCreated;
			};

		auto pt = pointers.getById(pointerid);
		if (!pt) return;

		if (pt->mode == tsl::graphics::InputSystem::WinState::WINPOINTER) {
			const bool is_looper = track->fxpower[SPACE_LOOPER].load(std::memory_order_acquire);
			bool isPlaying = is_looper ? _STATE->player.isPlaying() : true;

			switch (pt->target) {

			case OFF_START:

				if (is_looper) {
					auto o = data->posStartLoop.load(std::memory_order_acquire);
					auto stateNew = std::make_shared<RecordingState>(*state);

					const auto current_off_stop = stateNew->off_stop.load();
					if (o > current_off_stop) o = current_off_stop;
					stateNew->off_start.store(o);
					auto offset = stateNew->offset.load();
					if (offset < o) {
						stateNew->offset.store(o);
					}
					filebuffer->state.store(stateNew, std::memory_order_release);
					filebuffer->isresizing.store(false, std::memory_order_release);

				}
				else {

					filebuffer->isresizing.store(false, std::memory_order_release);
					if (hasChanged(*pt)) {
						auto start = state->off_start.load();
						std::vector<tsl::parameters::Event> ev{};
						auto wd = std::bit_cast<WaveformData>(hoz ? pt->userData4 : pt->userData3);

						WaveformUndoRedo undo{};
						undo.offsetChanged = wd.offsetMoved;
						WaveformUndoRedo redo = undo;
						redo.loopBound = start;
						undo.loopBound = pt->userData1;
						if (wd.offsetMoved) {
							auto offset = state->offset.load();
							redo.offset = offset;
							ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, offset, tsl::parameters::EventSubtype::offset, 0, 0));

							auto pd = state->playbackDir.load();
							redo.playbackDir = pd;
							ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, pd, tsl::parameters::EventSubtype::playbackDir, 0, 0));

							undo.offset = pt->userData2;
							undo.playbackDir = wd.playbackDir;
						}
						if (filebuffer->poolHandle < NO_SOUND_PRESENT) {
							auto& pd = _STATE->recordings[filebuffer->poolHandle];
							std::lock_guard lk(pd.mutex);
							if (pd.isValid()) {
								auto& ur = pd.waveformUndoRedos[track->index];
								auto& pos = pd.posWaveform[track->index];
								ur.resize(pos);
								ur.push_back(redo);
								ur.push_back(undo);
								pos = ur.size();
								ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, pos, tsl::parameters::EventSubtype::offStartFromUi, 0, tsl::parameters::Event::EventFlags::History));
							}
						}
						ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, start, tsl::parameters::EventSubtype::offStart, 0, 0));

							std::lock_guard lk(_DATA->snapShot);
							for (auto& e : ev)
								_DATA->snapShot.addEvent(std::move(e));
						}

				}

				break;

			case OFF_STOP:
				if (is_looper) {
					auto o = data->posStopLoop.load(std::memory_order_acquire);
					auto stateNew = std::make_shared<RecordingState>(*state);

					const auto current_off_start = stateNew->off_start.load();
					if (o < current_off_start) o = current_off_start;

					stateNew->off_stop.store(o);
					if (stateNew->offset.load() > o) {
						stateNew->offset.store(o);
					}
					filebuffer->state.store(stateNew, std::memory_order_release);
					filebuffer->isresizing.store(false, std::memory_order_release);

				}
				else {
					filebuffer->isresizing.store(false, std::memory_order_release);
					if (hasChanged(*pt)) {

						auto stop = state->off_stop.load();
						std::vector<tsl::parameters::Event> ev{};
						auto wd = std::bit_cast<WaveformData>(hoz ? pt->userData4 : pt->userData3);
						WaveformUndoRedo undo{};
						undo.offsetChanged = wd.offsetMoved;
						WaveformUndoRedo redo = undo;
						undo.loopBound = pt->userData1;
						redo.loopBound = stop;
						if (wd.offsetMoved) {
							auto offset = state->offset.load();
							redo.offset = offset;
							ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, offset, tsl::parameters::EventSubtype::offset, 0, 0));

							auto pd = state->playbackDir.load();
							redo.playbackDir = pd;
							ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, pd, tsl::parameters::EventSubtype::playbackDir, 0, 0));
							
							undo.offset = pt->userData2;
							undo.playbackDir = wd.playbackDir;
						}
						if (filebuffer->poolHandle < NO_SOUND_PRESENT) {
							auto& pd = _STATE->recordings[filebuffer->poolHandle];
							std::lock_guard lk(pd.mutex);
							if (pd.isValid()) {
								auto& ur = pd.waveformUndoRedos[track->index];
								auto& pos = pd.posWaveform[track->index];
								ur.resize(pos);
								ur.push_back(redo);
								ur.push_back(undo);
								pos = ur.size();
								ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, pos, tsl::parameters::EventSubtype::offStopFromUi, 0, tsl::parameters::Event::EventFlags::History));
							}
						}
						ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, stop, tsl::parameters::EventSubtype::offStop, 0, 0));
							std::lock_guard lk(_DATA->snapShot);

							for (auto& e : ev)
								_DATA->snapShot.addEvent(std::move(e));
					}
				}
				break;

			case OFF_GRAIN:
				if (is_looper) {
					auto o = data->offsetLoop.load(std::memory_order_acquire);
					auto stateNew = std::make_shared<RecordingState>(*state);;
					const auto current_off_start = stateNew->off_start.load();
					const auto current_off_stop = stateNew->off_stop.load();

					o = std::clamp(o, current_off_start, current_off_stop);
					if (stateNew->offset.exchange(o) != o) {
						filebuffer->state.store(stateNew, std::memory_order_release);
							_DATA->snapShot.addEvent({ tsl::parameters::Event::createEvent(associatedtrack->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, o, tsl::parameters::EventSubtype::offsetFromUi, 0, tsl::parameters::Event::History | tsl::parameters::Event::Info | tsl::parameters::Event::ToAudioThread) });
					}
					filebuffer->ismoving.store(false, std::memory_order_release);
				}
				else {
					filebuffer->ismoving.store(false, std::memory_order_release);
					if (hasChanged(*pt)) {
						auto offset = state->offset.load();
						std::vector<tsl::parameters::Event> ev{};
						if (filebuffer->poolHandle < NO_SOUND_PRESENT) {
							auto& pd = _STATE->recordings[filebuffer->poolHandle];
							std::lock_guard lk(pd.mutex);
							if (pd.isValid()) {
								WaveformUndoRedo undo{};
								WaveformUndoRedo redo{};
								redo.offset = offset;
								undo.offset = pt->userData1;
								auto& ur = pd.waveformUndoRedos[track->index];
								auto& pos = pd.posWaveform[track->index];
								ur.resize(pos);
								ur.push_back(redo);
								ur.push_back(undo);
								pos = ur.size();
								ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, pos, tsl::parameters::EventSubtype::offsetFromUi, 0, tsl::parameters::Event::EventFlags::History));
							}
						}
						ev.push_back(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, offset, tsl::parameters::EventSubtype::offset, 0, 0));

							std::lock_guard lk(_DATA->snapShot);

							for (auto& e : ev)
								_DATA->snapShot.addEvent(std::move(e));
					}

				}
				break;
			}
		}
		else {
			if (pt->mode == tsl::graphics::InputSystem::WinState::WINDRAG && (pt->target == WAVE || pt->target == ZOOM)) {
					_DATA->snapShot.addEvent({ tsl::parameters::Event::createEvent(associatedtrack->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, std::bit_cast<double>(state->waveformState.load()), tsl::parameters::EventSubtype::waveformView, 0, tsl::parameters::Event::History | tsl::parameters::Event::Info) });
				}
			// Handle drag mode cleanup
			std::array<tsl::graphics::InputSystem::Pointer*, 8> drag_pointers{};
			size_t drag_count = pointers.getPointersByMode(
				tsl::graphics::InputSystem::WinState::WINDRAG, drag_pointers);

			if (drag_count == 2) {
				for (size_t i = 0; i < drag_count; ++i) {
					drag_pointers[i]->target = WAVE;
				}
				olddist = 0;
			}
		}
		pointers.removePointer(pointerid);
		break;
	}

	case ACTION_MOVE: {
		auto pt = pointers.getById(pointerid);
		if (!pt) return;

		const bool looper = track->fxpower[SPACE_LOOPER].load(std::memory_order_acquire);
		const bool is_resizing = filebuffer->isresizing.load(std::memory_order_acquire);
		const bool is_moving = filebuffer->ismoving.load(std::memory_order_acquire);

		// Calculate movement delta
		const double diffx = _xpos - pt->xpos;
		const double diffy = _ypos - pt->ypos;

		// Update pointer position
		pt->updatePosition(_xpos, _ypos);

		const int mode = pt->target;

		// Helper references for looper mode
		auto& offset_ref = (looper && (is_resizing || is_moving)) ? data->offsetLoop : state->offset;
		auto& off_start_ref = (looper && is_resizing) ? data->posStartLoop : state->off_start;
		auto& off_stop_ref = (looper && is_resizing) ? data->posStopLoop : state->off_stop;

		switch (mode) {
		case OFF_START: {

			const double _zoom = state->waveformState.load(std::memory_order_acquire).zoom;
			const double framesvisible = off / _zoom;
			const double framesperpixel = framesvisible / (double)(hoz ? width : height);

			const double movement = (hoz ? diffx : diffy) * framesperpixel;
			auto tmp = off_start_ref.load(std::memory_order_acquire) + movement;
			const auto ostop = off_stop_ref.load(std::memory_order_acquire);
			tmp = std::clamp(tmp, 0.0, ostop);
			track->play_dur.store(ostop - tmp, std::memory_order_release);
			if (tmp > offset_ref.load(std::memory_order_acquire)) {
				offset_ref.store(tmp, std::memory_order_release);
				auto wd = std::bit_cast<WaveformData>(hoz ? pt->userData4 : pt->userData3);
				wd.offsetMoved = 1;
				if (hoz) pt->userData4 = std::bit_cast<uint32_t>(wd);
				else     pt->userData3 = std::bit_cast<uint32_t>(wd);
			}
			if (off_start_ref.exchange(tmp, std::memory_order_release) != tmp);
			track->computeLoopTime();
			break;
		}

		case OFF_STOP: {
			const double _zoom = state->waveformState.load(std::memory_order_acquire).zoom;
			const double framesvisible = off / _zoom;
			const double framesperpixel = framesvisible / (double)(hoz ? width : height);

			const auto movement = (hoz ? diffx : diffy) * framesperpixel;
			const auto ostart = off_start_ref.load(std::memory_order_acquire);
			auto tmp = off_stop_ref.load(std::memory_order_acquire) + movement;

			tmp = std::clamp(tmp, ostart, (double)off);

			if (tmp < offset_ref.load(std::memory_order_acquire)) {
				offset_ref.store(tmp, std::memory_order_release);
				auto wd = std::bit_cast<WaveformData>(hoz ? pt->userData4 : pt->userData3);
				wd.offsetMoved = 1;
				if (hoz) pt->userData4 = std::bit_cast<uint32_t>(wd);
				else     pt->userData3 = std::bit_cast<uint32_t>(wd);
			}
			track->play_dur.store(tmp - ostart, std::memory_order_release);
			if (off_stop_ref.exchange(tmp, std::memory_order_release) != tmp)
				track->computeLoopTime();
			break;
		}

		case OFF_GRAIN: {
			const double _zoom = state->waveformState.load(std::memory_order_acquire).zoom;
			const double framesvisible = off / _zoom;
			const double framesperpixel = framesvisible / (double)(hoz ? width : height);
			const auto movement = (hoz ? diffx : diffy) * framesperpixel;

			auto tmp = offset_ref.load(std::memory_order_acquire) + movement;
			const auto ostart = off_start_ref.load(std::memory_order_acquire);
			const auto ostop = off_stop_ref.load(std::memory_order_acquire);

			tmp = std::clamp(tmp, ostart, ostop);
			offset_ref.store(tmp, std::memory_order_release);
			break;
		}

		case WAVE: {
			auto ws = state->waveformState.load(std::memory_order_acquire);

			const double max = 1.0 - 1.0 / ws.zoom;
			if (max == 0) {
				ws.startPos = 0.f;
				if (state->waveformState.exchange(ws, std::memory_order_release) != ws)
					filebuffer->dorendering.store(true, std::memory_order_release);
			}
			else {
				const double movement_factor = hoz ? (diffx / width) : (diffy / height);
				const double tmp = ws.startPos - movement_factor * (1.0 - max);
				auto des = std::clamp(tmp, 0.0, max);
				ws.startPos = des;
				if (state->waveformState.exchange(ws,
					std::memory_order_release) != ws)
					filebuffer->dorendering.store(true, std::memory_order_release);;
			}
			;
			break;
		}

		case ZOOMWAVEFORM: {
			auto ws = state->waveformState.load(std::memory_order_acquire);

			std::array<tsl::graphics::InputSystem::Pointer*, 8> drag_pointers{};
			size_t drag_count = pointers.getPointersByMode(
				tsl::graphics::InputSystem::WinState::WINDRAG, drag_pointers);

			if (drag_count < 2) return;

			const double newdist = spacing(drag_pointers[0]->xpos, drag_pointers[1]->xpos,
				drag_pointers[0]->ypos, drag_pointers[1]->ypos);
			if (newdist == -9999) {
				for (size_t i = 0; i < drag_count; ++i) {
					pointers.removePointer(drag_pointers[i]->id);
				}
				return;
			}
			// Improved zoom and pan calculation with better readability and safety checks

			// Safety check for division by zero
			if (olddist <= 0 || ws.zoom <= 0) {
				return; // or handle error appropriately
			}

			// Calculate new zoom level
			double tmp = (newdist / olddist) * ws.zoom;

			// Enforce minimum zoom level
			tmp = std::max(tmp, 1.0);

			// Calculate content bounds to prevent zooming beyond available content
			double contentSize = hoz ? width : height;
			double maxZoom = off / contentSize;

			// Prevent zooming out beyond content bounds
			if (tmp > maxZoom && maxZoom > 0) {
				tmp = maxZoom;
			}

			// Calculate zoom change factor
			double zoomRatio = tmp / ws.zoom;
			double diff = (1.0 - zoomRatio) / ws.zoom;

			// Calculate midpoint of touch points
			double midpoint = hoz ?
				(drag_pointers.at(0)->xpos + drag_pointers.at(1)->xpos) /
				(2.0f * width) :
				(drag_pointers.at(0)->ypos + drag_pointers.at(1)->ypos) /
				(2.0f * height);

			// Calculate new position based on zoom change and touch midpoint
			double newpos = ws.startPos - (diff * midpoint);

			// Calculate maximum valid position (prevents scrolling beyond content)
			double maxPos = std::max(0.0, 1.0 - 1.0 / tmp);

			// Clamp position to valid range [0, maxPos]
			newpos = std::clamp(newpos, 0.0, maxPos);

			// Thread-safe atomic updates
			ws.startPos = newpos;
			ws.zoom = tmp;
			olddist = newdist;
			if (state->waveformState.exchange(ws, std::memory_order_release) != ws) {
				filebuffer->dorendering.store(true, std::memory_order_release);
					_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, std::bit_cast<double>(ws), tsl::parameters::EventSubtype::waveformView, 0, tsl::parameters::Event::History | tsl::parameters::Event::Info));
			}
			break;

		}
		}
		break;
	}

	case(ACTION_MOUSE_WHEEL): {
		auto ws = state->waveformState.load(std::memory_order_acquire);

		auto tmp = (1. + 0.1 * event.pointer_id) * ws.zoom;

		if (tmp < 1.)
			tmp = 1.;
		if (hoz) {
			if (off / tmp < width)
				tmp = off / width;
		}
		else {
			if (off / tmp < height)
				tmp = off / height;
		}
		double diff = (1. - tmp / ws.zoom) / ws.zoom;
		double newpos = hoz ? (ws.startPos - diff * (_xpos /
			width)) : (ws.startPos - diff *
				(_ypos /
					height));
		double max = 1. - 1. / tmp;
		newpos = std::clamp(newpos, 0., max);

		ws.startPos = newpos;
		ws.zoom = tmp;

		if (state->waveformState.exchange(ws, std::memory_order_release) != ws) {
			filebuffer->dorendering.store(true, std::memory_order_release);
				_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(track->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, std::bit_cast<double>(ws), tsl::parameters::EventSubtype::waveformView, 0, tsl::parameters::Event::History | tsl::parameters::Event::Info));
			}
		break;
	}

	default:
		break;
	}
}

void WAVE_S::render(std::shared_ptr<tsl::Recording>& rec, int orientation) {
	waveformarray.clear();
	const int end = orientation == VERTICAL ? height : width;
	auto off = rec == nullptr ? 0 : rec->off;
	if (off == 0)return;
	channels = rec->channels;
	//PANEL *temp = g_ptr_array_index(_DATA->drawview->elements, WAVEFORMPANEL);
	const float draw_offset = (orientation == VERTICAL ? width : height) / (float)channels;
	const float halb = 0.5f * draw_offset;
	const float gain = 0.9 * CONVMYFLT;

	float frames_per_bin = off / (float)(orientation == VERTICAL ? height : width);

	long frames_per_buf = (long)floorf(frames_per_bin);

	long offset = 0;

	auto fb = rec->buffer;
	for (int x = 0; x < end; x++) {
		float min[2]{}, max[2]{};

		for (auto frame = 0; frame < frames_per_buf; frame++) {
			if (offset + frame >= off) {
				break;
			};
			for (int i = 0; i < channels; i++) {
				const float sample_val1 = (fb[i])[offset + frame];// + offsetdata1[frame];
				max[i] = MAX(max[i], sample_val1);
				min[i] = MIN(min[i], sample_val1);
			}
			// LOGE("min1: %f max1: %f min2: %f max2: %fminon1: %d maxoff1: %d minon2: %d maxoff2: %d", min1, max1, min2, max2, minon1, maxoff1, minon2, maxoff2);
		}

		tsl::array<int16_t, 4> arr{};
		for (int i = 0; i < channels; i++) {
			float minon1 = halb - ABS(halb * min[i] * gain);
			float maxoff1 = halb + ABS(halb * max[i] * gain);

			arr[i * 2] = (int16_t)(i * draw_offset + minon1);
			arr[i * 2 + 1] = (int16_t)(i * draw_offset + maxoff1);
		}
		waveformarray.emplace_back(arr);
		offset += frames_per_buf;

		frames_per_buf = (long)floorf((x + 2) * frames_per_bin) - offset;
	}

}

#if defined USE_IMGUI

void Waveform::render_surface_direct() {
	const int end = orientation == VERTICAL ? height : width;
	const int channels = _STATE->channels;
	unsigned long off;
	std::shared_ptr<std::vector<short>> fb[2];
	for (int i = 0; i < channels; i++) {
		fb[i] = std::atomic_load(&filebuffer[i]);
		if (fb[i] == nullptr || fb[i]->empty()) {
			waveformarray.clear();
			return;
		}
		if (i == 0) {
			off = fb[0]->size();
		}
		else if (off > fb[i]->size())
			off = fb[i]->size();

	}
	const float draw_offset = (orientation == VERTICAL ? width : height) / (float)channels;
	const float halb = 0.5f * draw_offset;
	const long startframe = startpos * off;
	const float framesvisible = off / waveformzoom;
	const long new_length = framesvisible;

	float frames_per_bin = new_length / (float)(orientation == VERTICAL ? height : width);

	long frames_per_buf = (long)(floor(frames_per_bin));

	long offset = 0;

	_STATE->memoryUsage -= waveformarray.size() * 4 * sizeof(int16_t);
	waveformarray.resize(end);
	_STATE->memoryUsage += waveformarray.size() * 4 * sizeof(int16_t);

	for (int x = 0; x < end; x++) {
		int16_t min[2]{}, max[2]{};

		for (int frame = 0; frame < frames_per_buf; frame++) {
			if (startframe + offset + frame >= off) {
				break;
			};

			for (int i = 0; i < channels; i++) {
				const short sample_val1 = (*fb[i])[startframe + offset +
					frame];
				//const short sample_val2 = data2[frame];
				max[i] = MAX(max[i], sample_val1);
				min[i] = MIN(min[i], sample_val1);
			};
			// LOGE("min1: %f max1: %f min2: %f max2: %fminon1: %d maxoff1: %d minon2: %d maxoff2: %d", min1, max1, min2, max2, minon1, maxoff1, minon2, maxoff2);
		}


		const float gain = 0.9;

		for (int i = 0; i < channels; i++) {
			double minon1 = halb - ABS(halb * min[i] * CONVMYFLT * gain);
			double maxoff1 = halb + ABS(halb * max[i] * CONVMYFLT * gain);

			waveformarray.at(x)[i * 2] = (short)(i * draw_offset + minon1);
			waveformarray.at(x)[i * 2 + 1] = (short)(i * draw_offset + maxoff1);
		}
		offset += frames_per_buf;

		frames_per_buf = (long)(floor((x + 2) * frames_per_bin) - offset);
	}
}



void Waveform::render(void* context) {
	TRACK* active = _DATA->tracks[_STATE->active_track.load(std::memory_order_acquire)];
	TRACK* source =
		_DATA->tracks[(int)_STATE->params[active->index][DISTRSOURCE].load(std::memory_order_acquire)];
	TRACK* t = (active->fxpower[SPACE_LOOPER].load(std::memory_order_acquire) ==
		source->fxpower[SPACE_LOOPER].load(std::memory_order_acquire)) ? source : active;
	auto* waveform = (Waveform*)t->waveform;
	std::shared_ptr<std::vector<short>> fb[2];
	unsigned long off = 0;
	for (int i = 0; i < _STATE->channels; i++) {
		fb[i] = std::atomic_load(&waveform->filebuffer[i]);
		if (fb[i] == nullptr || fb[i]->empty()) {
			flush(static_cast<SkCanvas*>(context));
			return;
		}
		if (i == 0) {
			off = fb[0]->size();
		}
		else if (off > fb[i]->size())
			off = fb[i]->size();
	}

	ImGui::SetNextWindowPos(ImVec2(startx - 2, starty), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(width + 4, height), ImGuiCond_Always);
	//ImGui::GetStyle().AntiAliasedFill = true;
	//ImGui::GetStyle().AntiAliasedLines = true;
	bool test = true;

	if (!ImGui::Begin(t->name, &test,
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBringToFrontOnFocus)) {
		ImGui::End();
		return;
	}

	ImVec2 canvas_size = { (float)width,
						  (float)height };//ImGui::GetContentRegionAvail();        // Resize canvas to what's available
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
	ImVec2 canvas_pos = { (float)startx,
						 (float)starty };//ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	float w = canvas_size.x, h = canvas_size.y;

	int channels = _STATE->channels;
	const bool hoz = orientation == HORIZONTAL;

	float grainsize = _STATE->params[active->index][GRAINSIZE].load(std::memory_order_acquire);
	auto& offset = normalloop && t->ismoving ? waveform->offsetLoop : t->offset;
	auto& off_start = normalloop && t->isresizing ? waveform->posStartLoop : t->off_start;
	auto& off_stop = normalloop && t->isresizing ? waveform->posStopLoop : t->off_stop;

	const float zoom = waveform->waveformzoom;
	const float start = waveform->startpos;
	const float startframe = start * off;
	const float framesvisible = off / zoom;
	const float stopframe = startframe + framesvisible;
	const float framesperpixel = framesvisible / (float)(hoz ? width : height);
	const float pos_start = (off_start - startframe) / framesperpixel;
	const float pos_stop = (off_stop - startframe) / framesperpixel;
	const float pos_grain = (offset - startframe) / framesperpixel;
	float markersize = grainsize / (float)off * (orientation == VERTICAL ? h : w) * zoom;

	// Tip: If you do a lot of custom rendering, you probably want to use your own geometrical types and benefit of overloaded operators, etc.
	// Define IM_VEC2_CLASS_EXTRA in imconfig.h to create implicit conversions between your types and ImVec2/ImVec4.
	// ImGui defines overloaded operators but they are internal to imgui.cpp and not exposed outside (to avoid messing with your types)
	// In this example we are not using the maths operators!

	float pos_first = waveform->last == OFF_START ? pos_stop : pos_start;
	float pos_second = pos_first == pos_stop ? pos_start : pos_stop;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::bg));

	//draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y +
	//                                                                         canvas_size.y));// clip lines within the canvas (if we resize it, etc.)

	if (waveform->dorendering.load(std::memory_order_acquire)) {
		waveform->render_surface_static();
		waveform->dorendering = false;
	}
	auto& array = waveform->waveformarray;
	for (int row = 0; row < array.size(); row++) {
		for (int channel = 0;
			channel < channels; channel++) {
			if (hoz)
				draw_list->AddLine(
					ImVec2(canvas_pos.x + row, canvas_pos.y + array[row][channel * 2]),
					ImVec2(canvas_pos.x + row, canvas_pos.y + array[row][channel * 2 + 1]),
					IM_Colour(skcol::waveform), 2.0);
			else
				draw_list->AddLine(
					ImVec2(canvas_pos.x + array[row][channel * 2], canvas_pos.y + row),
					ImVec2(canvas_pos.x + array[row][channel * 2 + 1], canvas_pos.y + row),
					IM_Colour(skcol::waveform), 2.0);

		}
	}


	if (hoz) {
		draw_list->
			AddLine(ImVec2(canvas_pos.x + pos_first, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_first, canvas_pos.y + +canvas_size.y),
				IM_Colour(skcol::bounce_colours[t->bounce_type]), 3.0);
		draw_list->
			AddLine(ImVec2(canvas_pos.x + pos_second, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_second, canvas_pos.y + canvas_size.y),
				IM_Colour(skcol::bounce_colours[t->bounce_type]), 3.0);

	}
	else {
		draw_list->
			AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_first),
				ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_first),
				IM_Colour(skcol::bounce_colours[t->bounce_type]), 3.0);
		draw_list->
			AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_second),
				ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_second),
				IM_Colour(skcol::bounce_colours[t->bounce_type]), 3.0);

	}
	if (!normalloop) {
		if (hoz) {
			draw_list->
				AddRectFilled(ImVec2(canvas_pos.x + pos_grain, canvas_pos.y),
					ImVec2(canvas_pos.x + pos_grain + markersize,
						canvas_pos.y + canvas_size.y),
					IM_COL32(127, 127, 127, 127));
			draw_list->
				AddLine(ImVec2(canvas_pos.x + pos_grain, canvas_pos.y),
					ImVec2(canvas_pos.x + pos_grain,
						canvas_pos.y + pos_grain + +canvas_size.y),
					IM_Colour(skcol::yellow), 3.0);

		}
		else {
			draw_list->
				AddRectFilled(ImVec2(canvas_pos.x, canvas_pos.y + pos_grain),
					ImVec2(canvas_pos.x + canvas_size.x,
						canvas_pos.y + pos_grain + markersize),
					IM_COL32(127, 127, 127, 127));
			draw_list->
				AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_grain),
					ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_grain),
					IM_Colour(skcol::yellow), 3.0);
		}
	}
	else {
		float vala, valb, fob = 0, dist;
		const int dir = t->playbackspeed_dir.load(std::memory_order_acquire);
		const int bounce_type = t->bounce_type.load(std::memory_order_acquire);

		int fadesamples = 1 + (int)((float)_STATE->sr / 1000.f *
			_STATE->params[active->index][LOOPERFADE].load(std::memory_order_acquire));
		float distance = DISTANCEF(off_start, off_stop);
		if (distance < 1.f)
			distance = 1.f;
		if (fadesamples > distance)
			fadesamples = (int)distance;
		float onedfadesamples = 1.f / (float)fadesamples;
		if (bounce_type != NO_BOUNCE) {

			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_stop + dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_start - dist;
			}
		}
		else {
			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_start - dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_stop + dist;
			}
		}

		if (dist <= fadesamples) {
			vala = (dist * onedfadesamples);
			valb = (1.0f - vala);
		}
		else {
			vala = 1.0;
			valb = .0f;
		}

		float pos_fade = (fob - startframe) / framesperpixel;


		uint colpos = SK_Colour(255.f, 235.f, 59.f, 204.f * vala);
		uint colfade = SK_Colour(255.f, 235.f, 59.f, 204.f * valb);


		if (hoz) {
			draw_list->
				AddLine(ImVec2(canvas_pos.x + pos_grain, canvas_pos.y),
					ImVec2(canvas_pos.x + pos_grain, canvas_pos.y + canvas_size.y),
					IM_Colour(colpos), 3.0);
			draw_list->
				AddLine(ImVec2(canvas_pos.x + pos_fade, canvas_pos.y),
					ImVec2(canvas_pos.x + pos_fade, canvas_pos.y + canvas_size.y),
					IM_Colour(colfade), 3.0);

		}
		else {
			draw_list->
				AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_grain),
					ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_grain),
					IM_Colour(colpos), 3.0);
			draw_list->
				AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_fade),
					ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_fade),
					IM_Colour(colfade), 3.0);

		}

		if (bounce_type == BOUNCE_NO_REVERSE) {
			float pos_bi;
			if (dir > 0) {
				pos_bi = (off_start + dist - startframe) / framesperpixel;
				pos_fade = (off_start - dist - startframe) / framesperpixel;

			}
			else {
				pos_bi = (off_stop - dist - startframe) / framesperpixel;
				pos_fade = (off_stop + dist - startframe) / framesperpixel;

			}
			if (hoz) {
				draw_list->
					AddLine(ImVec2(canvas_pos.x + pos_bi, canvas_pos.y),
						ImVec2(canvas_pos.x + pos_bi, canvas_pos.y + canvas_size.x),
						IM_Colour(colpos), 3.0);
				draw_list->
					AddLine(ImVec2(canvas_pos.x + pos_fade, canvas_pos.y),
						ImVec2(canvas_pos.x + pos_fade, canvas_pos.y + canvas_size.x),
						IM_Colour(colfade), 3.0);
			}
			else {
				draw_list->
					AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_bi),
						ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_bi),
						IM_Colour(colpos), 3.0);
				draw_list->
					AddLine(ImVec2(canvas_pos.x, canvas_pos.y + pos_fade),
						ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + pos_fade),
						IM_Colour(colfade), 3.0);
			}

		}
	}

	if (hoz) {
		draw_list->
			AddTriangleFilled(ImVec2(canvas_pos.x + pos_first - width_triangle / 2,
				canvas_pos.y + markerheight),
				ImVec2(canvas_pos.x + +pos_first,
					canvas_pos.y + markerheight + height_triangle),
				ImVec2(canvas_pos.x + pos_first + width_triangle / 2,
					canvas_pos.y + +markerheight),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(
				ImVec2(canvas_pos.x + pos_first - width_triangle / 2, canvas_pos.y + markerheight),
				ImVec2(canvas_pos.x + pos_first, canvas_pos.y + markerheight + height_triangle),
				ImVec2(canvas_pos.x + pos_first + width_triangle / 2, canvas_pos.y + markerheight),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(
				ImVec2(canvas_pos.x + pos_first - markerwidth * .5f, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_first + markerwidth * .5f,
					canvas_pos.y + markerheight),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x + pos_first - markerwidth * .5f, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_first + markerwidth * .5f,
					canvas_pos.y + markerheight),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersRight, 3.0);

		draw_list->
			AddTriangleFilled(
				ImVec2(canvas_pos.x + pos_second - width_triangle / 2, canvas_pos.y + markerheight),
				ImVec2(canvas_pos.x + pos_second, canvas_pos.y + markerheight + height_triangle),
				ImVec2(canvas_pos.x + pos_second + width_triangle / 2, canvas_pos.y + markerheight),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(
				ImVec2(canvas_pos.x + pos_second - width_triangle / 2, canvas_pos.y + markerheight),
				ImVec2(canvas_pos.x + pos_second, canvas_pos.y + markerheight + height_triangle),
				ImVec2(canvas_pos.x + pos_second + width_triangle / 2, canvas_pos.y + markerheight),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(
				ImVec2(canvas_pos.x + pos_second - markerwidth * .5f, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_second + markerwidth * .5f,
					canvas_pos.y + markerheight),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x + pos_second - markerwidth * .5f, canvas_pos.y),
				ImVec2(canvas_pos.x + pos_second + markerwidth * .5f,
					canvas_pos.y + markerheight),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersRight, 3.0);

		draw_list->
			AddTriangleFilled(ImVec2(canvas_pos.x + pos_grain - width_triangle / 2,
				canvas_pos.y + canvas_size.y - markerheight),
				ImVec2(canvas_pos.x + pos_grain,
					canvas_pos.y + canvas_size.y - markerheight -
					height_triangle),
				ImVec2(canvas_pos.x + pos_grain + width_triangle / 2,
					canvas_pos.y + canvas_size.y - markerheight),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(ImVec2(canvas_pos.x + pos_grain - width_triangle / 2,
				canvas_pos.y + canvas_size.y - markerheight),
				ImVec2(canvas_pos.x + pos_grain,
					canvas_pos.y + canvas_size.y - markerheight -
					height_triangle),
				ImVec2(canvas_pos.x + pos_grain + width_triangle / 2,
					canvas_pos.y + canvas_size.y - markerheight),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(ImVec2(canvas_pos.x + pos_grain - markerwidth * .5f,
				canvas_pos.y + canvas_size.y - markerheight),
				ImVec2(canvas_pos.x + pos_grain + markerwidth * .5f,
					canvas_pos.y + canvas_size.y),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x + pos_grain - markerwidth * .5f,
				canvas_pos.y + canvas_size.y - markerheight),
				ImVec2(canvas_pos.x + pos_grain + markerwidth * .5f,
					canvas_pos.y + canvas_size.y),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersLeft, 3.0);
	}
	else {
		draw_list->
			AddTriangleFilled(ImVec2(canvas_pos.x + markerwidth,
				canvas_pos.y + pos_first - height_triangle / 2),
				ImVec2(canvas_pos.x + markerwidth + width_triangle,
					canvas_pos.y + pos_first),
				ImVec2(canvas_pos.x + markerwidth,
					canvas_pos.y + pos_first + height_triangle / 2),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_first - height_triangle / 2),
				ImVec2(canvas_pos.x + markerwidth + width_triangle, canvas_pos.y + pos_first),
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_first + height_triangle / 2),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(
				ImVec2(canvas_pos.x, canvas_pos.y + pos_first - markerheight * .5f),
				ImVec2(canvas_pos.x + markerwidth,
					canvas_pos.y + pos_first + markerheight * .5f),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x, canvas_pos.y + pos_first - markerheight * .5f),
				ImVec2(canvas_pos.x + markerwidth,
					canvas_pos.y + pos_first + markerheight * .5f),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersRight, 3.0);

		draw_list->
			AddTriangleFilled(
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_second - height_triangle / 2),
				ImVec2(canvas_pos.x + markerwidth + width_triangle, canvas_pos.y + pos_second),
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_second + height_triangle / 2),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_second - height_triangle / 2),
				ImVec2(canvas_pos.x + markerwidth + width_triangle, canvas_pos.y + pos_second),
				ImVec2(canvas_pos.x + markerwidth, canvas_pos.y + pos_second + height_triangle / 2),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(
				ImVec2(canvas_pos.x, canvas_pos.y + pos_second - markerheight * .5f),
				ImVec2(canvas_pos.x + markerwidth,
					canvas_pos.y + pos_second + markerheight * .5f),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x, canvas_pos.y + pos_second - markerheight * .5f),
				ImVec2(canvas_pos.x + markerwidth,
					canvas_pos.y + pos_second + markerheight * .5f),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersRight, 3.0);

		draw_list->
			AddTriangleFilled(ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
				canvas_pos.y + pos_grain - height_triangle / 2),
				ImVec2(canvas_pos.x + canvas_size.x - markerwidth -
					width_triangle,
					canvas_pos.y + pos_grain),
				ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
					canvas_pos.y + pos_grain + height_triangle / 2),
				IM_COL32(0, 0, 0, 255));
		draw_list->
			AddTriangle(ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
				canvas_pos.y + pos_grain - height_triangle / 2),
				ImVec2(canvas_pos.x + canvas_size.x - markerwidth - width_triangle,
					canvas_pos.y + pos_grain),
				ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
					canvas_pos.y + pos_grain + height_triangle / 2),
				IM_Colour(skcol::fg), 4.0);
		draw_list->
			AddRectFilled(ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
				canvas_pos.y + pos_grain - markerheight * .5f),
				ImVec2(canvas_pos.x + canvas_size.x,
					canvas_pos.y + pos_grain + markerheight * .5f),
				IM_COL32(0, 0, 0, 255), corner_radius);
		draw_list->
			AddRect(ImVec2(canvas_pos.x + canvas_size.x - markerwidth,
				canvas_pos.y + pos_grain - markerheight * .5f),
				ImVec2(canvas_pos.x + canvas_size.x,
					canvas_pos.y + pos_grain + markerheight * .5f),
				IM_Colour(skcol::fg),
				corner_radius, ImDrawFlags_::ImDrawFlags_RoundCornersLeft, 3.0);

	}
	//draw_list->PopClipRect();

	ImGui::End();

}
void Waveform::render_surface_static() {
	auto wave_s = _wave_s;
	auto tmp = wave_s.get();
	while (tmp && waveformzoom > tmp->zoom)
		tmp = tmp->next;
	if (!tmp) {
		render_surface_direct();
		return;
	}
	auto& src = wave_s->waveformarray;
	int rows = orientation == HORIZONTAL ? width : height;



	_STATE->memoryUsage -= waveformarray.size() * 4 * sizeof(int16_t);
	waveformarray.resize(orientation == HORIZONTAL ? width : height, { 0, 0, 0, 0 });
	_STATE->memoryUsage += waveformarray.size() * 4 * sizeof(int16_t);

	int offset_draw = (int)floor(startpos * src.size());
	float inc = wave_s->zoom / waveformzoom;
	for (int row = 0; row < waveformarray.size(); row++) {
		int row_src = offset_draw + (int)(row * inc);
		if (row_src >= src.size())
			break;
		waveformarray[row] = src[row_src];
	}
}





#else

#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include <memory>
#include <vector>
#include <algorithm>


void Waveform::render_surface_static() {
	auto data = renderData.load(std::memory_order_acquire);
	auto rec = data != nullptr ? data->filebuffer : nullptr;
	auto state = rec ? rec->state.load(std::memory_order_acquire) : nullptr;
	auto off = state == nullptr ? 0 : rec->off;
	if (off == 0) {
		render_surface_direct();
		return;
	}
	SkCanvas* canvas = _appState->graphics.getCanvas(windex, 0, 0, width, height, false);
	if (!canvas) {
		LOGE("Failed to create waveform Skia surface");
		return;
	}


	const auto ws = state->waveformState.load(std::memory_order_acquire);
	const auto zoom = ws.zoom;
	auto tmp = data->wave_s.get();
	while (tmp) {
		if (tmp->next && zoom < tmp->next->zoom)
			break;
		tmp = tmp->next;
	}
	if (!tmp) {
		render_surface_direct();
		return;
	}

	const auto _startpos = ws.startPos;


	auto& src = tmp->waveformarray;

	int offset_draw = (int)floor(_startpos * src.size());
	float inc = tmp->zoom / zoom;

	// Build a single path containing all waveform segments
	SkPath waveformPath;
	waveformPath.setFillType(SkPathFillType::kWinding);


	int channels = tmp->channels;

	int rows = orientation == HORIZONTAL ? width : height;

	for (int row = 0; row < rows; row++) {
		int row_src = offset_draw + static_cast<int>(row * inc);
		if (row_src >= src.size())
			break;
		auto& vals = src[row_src];

		for (int c = 0; c < channels; c++) {
			float v0 = (float)vals[c * 2];
			float v1 = (float)vals[c * 2 + 1];
			if (orientation == HORIZONTAL) {
				float x = (float)row;
				waveformPath.moveTo(x, v0);
				waveformPath.lineTo(x, v1);
			}
			else {
				float y = (float)row;
				waveformPath.moveTo(v0, y);
				waveformPath.lineTo(v1, y);
			}
		}
	}

	canvas->clear(skcol::bg);
	// Draw the path
	SkPaint paint;
	paint.setColor(skcol::waveform);
	paint.setStrokeWidth(2.0f);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setAntiAlias(true);
	canvas->drawPath(waveformPath, paint);

	/*
	auto f = _STATE->font_normal;
	f.setSize(_STATE->textsize1 * 0.9);
	std::stringstream s;
	paint.setColor(skcol::blue_violet);
	s << tmp->zoom << "s: " << tmp->height <<std::endl;
	canvas->drawSimpleText(s.str().c_str(), s.str().length(), SkTextEncoding::kUTF8, 10, _STATE->textsize1, f, paint);
	*/
}


void Waveform::render_surface_direct() {
	SkCanvas* canvas = _appState->graphics.getCanvas(windex, 0, 0, width, height, false);
	if (!canvas) {
		LOGE("Failed to create waveform Skia surface");
		return;
	}
	auto data = renderData.load(std::memory_order_acquire);
	auto rec = data != nullptr ? data->filebuffer : nullptr;
	auto state = rec ? rec->state.load(std::memory_order_acquire) : nullptr;
	auto off = state == nullptr ? 0 : rec->off;
	if (off == 0) {
		_appState->graphics.deleteWindow(windex);
		return;
	}
	const int end = orientation == VERTICAL ? height : width;
	const int channels = rec->channels;
	auto fb = rec->buffer;

	const float draw_offset = (orientation == VERTICAL ? width : height) / (float)channels;
	const float halb = 0.5f * draw_offset;

	const auto ws = state->waveformState.load(std::memory_order_acquire);
	const long startframe = ws.startPos * off;
	const float framesvisible = off / ws.zoom;
	const long new_length = framesvisible;

	float frames_per_bin = new_length / (float)(orientation == VERTICAL ? height : width);

	long frames_per_buf = (long)(floor(frames_per_bin));

	long offset = 0;
	SkPath waveformPath;
	waveformPath.setFillType(SkPathFillType::kWinding);

	for (int x = 0; x < end; x++) {
		float min[2]{}, max[2]{};

		for (int frame = 0; frame < frames_per_buf; frame++) {
			if (startframe + offset + frame >= off) {
				break;
			};

			for (int i = 0; i < channels; i++) {
				const float sample_val1 = (fb[i])[startframe + offset +
					frame];
				//const short sample_val2 = data2[frame];
				max[i] = MAX(max[i], sample_val1);
				min[i] = MIN(min[i], sample_val1);
			};
			// LOGE("min1: %f max1: %f min2: %f max2: %fminon1: %d maxoff1: %d minon2: %d maxoff2: %d", min1, max1, min2, max2, minon1, maxoff1, minon2, maxoff2);
		}


		const float gain = 0.9;
		for (int i = 0; i < channels; i++) {
			double minon1 = halb - ABS(halb * min[i] * CONVMYFLT * gain);
			double maxoff1 = halb + ABS(halb * max[i] * CONVMYFLT * gain);
			if (orientation == VERTICAL) {
				waveformPath.moveTo(i * draw_offset + minon1, x);
				waveformPath.lineTo(i * draw_offset + maxoff1, x);
			}
			else {
				waveformPath.moveTo(x, i * draw_offset + minon1);
				waveformPath.lineTo(x, i * draw_offset + maxoff1);

			}
		}

		offset += frames_per_buf;

		frames_per_buf = (long)(floor((x + 2) * frames_per_bin) - offset);
	}
	canvas->clear(skcol::bg);
	// Draw the path
	SkPaint paint;
	paint.setColor(skcol::waveform);
	paint.setStrokeWidth(2.0f);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setAntiAlias(true);
	canvas->drawPath(waveformPath, paint);
}


void Waveform::render(void* context) {
	// --- 1. Setup and buffer fetch ---
	bool normalloop = associatedtrack->fxpower[SPACE_LOOPER].load(std::memory_order_acquire);

	auto data = renderData.load(std::memory_order_acquire);
	auto rec = data != nullptr ? data->filebuffer : nullptr;
	auto state = rec ? rec->state.load(std::memory_order_acquire) : nullptr;
	auto off = state == nullptr ? 0 : rec->off;
	if (off == 0) {
		flush(static_cast<SkCanvas*>(context));
		return;
	}
	auto fb = rec->buffer;
	auto w = (double)width;
	auto h = (double)height;
	auto x0 = (double)startx;
	auto y0 = (double)starty;
	auto channels = rec->channels;
	bool hoz = orientation == HORIZONTAL;

	auto grainsize = _STATE->params[associatedtrack->index][GRAINSIZE].load(std::memory_order_acquire);

	double start, zoom, off_stop, off_start, offset;
	int wlast;
	auto surface = _appState->graphics.getSurface(windex);
	{
		if (rec->dorendering.exchange(false, std::memory_order_acq_rel) || !surface ||
			surface->width() != width || surface->height() != height) {
			render_surface_static();
			surface = _appState->graphics.getSurface(windex);
		}

		// --- 2. Compute geometry & parameters ---
		offset = (normalloop && rec->ismoving ? data->offsetLoop : state->offset);
		off_start = (normalloop && rec->isresizing ? data->posStartLoop : state->off_start);
		off_stop = (normalloop && rec->isresizing ? data->posStopLoop : state->off_stop);
		const auto ws = state->waveformState.load();
		zoom = ws.zoom;
		start = ws.startPos;
		wlast = data->last.load(std::memory_order_acquire);
	}
	double startframe = start * off;
	double framesvis = off / zoom;
	double framesperpx = framesvis / (hoz ? w : h);

	double pos_start = (off_start - startframe) / framesperpx;
	double pos_stop = (off_stop - startframe) / framesperpx;
	double pos_grain = (offset - startframe) / framesperpx;

	double markerSize = grainsize / off * (hoz ? w : h) * zoom;

	double pos_first = (wlast == OFF_START ? pos_stop : pos_start);
	double pos_second = (pos_first == pos_stop ? pos_start : pos_stop);

	// --- 3. Setup Skia paints ---
	SkCanvas* canvas = static_cast<SkCanvas*>(context);
	canvas->save();

	SkPaint bgPaint;
	bgPaint.setColor(skcol::bg);
	canvas->drawRect(SkRect::MakeXYWH(x0, y0, w, h), bgPaint);

	SkPaint wavePaint;
	wavePaint.setColor(skcol::waveform);
	wavePaint.setStrokeWidth(2.0f);

	SkPaint bouncePaint;
	bouncePaint.setColor(skcol::bounce_colours[state->bounceType.load(std::memory_order_acquire)]);
	bouncePaint.setStrokeWidth(3.0f);

	SkPaint grainPaint;
	grainPaint.setColor(SK_Colour(127, 127, 127, 127));
	grainPaint.setStyle(SkPaint::kFill_Style);

	SkPaint grainLine;
	grainLine.setColor(skcol::yellow);
	grainLine.setStrokeWidth(3.0f);
	// --- 4. Clip to waveform rect ---


	canvas->clipRect(SkRect::MakeXYWH(x0, y0, w, h));
	if (surface) {
		surface->draw(canvas, x0, y0);
	}

	// --- 6. Draw bounce markers ---
	if (hoz) {
		canvas->drawLine(x0 + pos_first, y0, x0 + pos_first, y0 + h, bouncePaint);
		canvas->drawLine(x0 + pos_second, y0, x0 + pos_second, y0 + h, bouncePaint);
	}
	else {
		canvas->drawLine(x0, y0 + pos_first, x0 + w, y0 + pos_first, bouncePaint);
		canvas->drawLine(x0, y0 + pos_second, x0 + w, y0 + pos_second, bouncePaint);
	}

	// --- 7. Grain marker or fade region ---
	if (!normalloop) {
		// simple grain marker
		if (hoz) {
			canvas->drawRect(
				SkRect::MakeXYWH(x0 + pos_grain, y0, markerSize, h),
				grainPaint
			);
			canvas->drawLine(
				x0 + pos_grain, y0,
				x0 + pos_grain, y0 + h,
				grainLine
			);
		}
		else {
			canvas->drawRect(
				SkRect::MakeXYWH(x0, y0 + pos_grain, w, markerSize),
				grainPaint
			);
			canvas->drawLine(
				x0, y0 + pos_grain,
				x0 + w, y0 + pos_grain,
				grainLine
			);
		}
	}
	else {
		double vala, valb, fob = 0, dist;
		const int dir = state->playbackDir.load(std::memory_order_acquire);
		const int bounce_type = state->bounceType.load(std::memory_order_acquire);

		int fadesamples = 1 + (int)(_STATE->sr / 1000. *
			_STATE->params[associatedtrack->index][LOOPERFADE].load(
				std::memory_order_acquire));
		double distance = DISTANCEF(off_start, off_stop);
		if (distance < 1.)
			distance = 1.;
		if (fadesamples > distance)
			fadesamples = (int)distance;
		double onedfadesamples = 1. / (double)fadesamples;
		if (bounce_type != NO_BOUNCE) {

			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_stop + dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_start - dist;
			}
		}
		else {
			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_start - dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_stop + dist;
			}
		}

		if (dist <= fadesamples) {
			vala = (dist * onedfadesamples);
			valb = (1.0 - vala);
		}
		else {
			vala = 1.0;
			valb = .0;
		}

		float pos_fade = (fob - startframe) / framesperpx;


		auto colpos = SK_Colour(255.f, 235.f, 59.f, 204.f * vala);
		auto colfade = SK_Colour(255.f, 235.f, 59.f, 204.f * valb);
		SkPaint pPos, pFade;
		pPos.setColor(colpos);
		pPos.setStrokeWidth(3.0f);
		pFade.setColor(colfade);
		pFade.setStrokeWidth(3.0f);

		if (hoz) {
			canvas->drawLine(x0 + pos_grain, y0, x0 + pos_grain, y0 + h, pPos);
			canvas->drawLine(x0 + pos_fade, y0, x0 + pos_fade, y0 + h, pFade);

		}
		else {
			canvas->drawLine(x0, y0 + pos_grain, x0 + w, y0 + pos_grain, pPos);
			canvas->drawLine(x0, y0 + pos_fade, x0 + w, y0 + pos_fade, pFade);


		}

		if (bounce_type == BOUNCE_NO_REVERSE) {
			double pos_bi;
			if (dir > 0) {
				pos_bi = (off_start + dist - startframe) / framesperpx;
				pos_fade = (off_start - dist - startframe) / framesperpx;

			}
			else {
				pos_bi = (off_stop - dist - startframe) / framesperpx;
				pos_fade = (off_stop + dist - startframe) / framesperpx;

			}
			if (hoz) {
				canvas->drawLine(x0 + pos_bi, y0, x0 + pos_bi, y0 + h, pPos);
				canvas->drawLine(x0 + pos_fade, y0, x0 + pos_fade, y0 + h, pFade);
			}
			else {
				canvas->drawLine(x0, y0 + pos_bi, x0 + w, y0 + pos_bi, pPos);
				canvas->drawLine(x0, y0 + pos_fade, x0 + w, y0 + pos_fade, pFade);
				canvas->drawLine(x0, y0 + pos_bi, x0 + w, y0 + pos_bi, pPos);
			}

		}
	}
	SkPaint mkFill, mkStroke;
	mkFill.setColor(SK_ColorBLACK);
	mkFill.setStyle(SkPaint::kFill_Style);
	mkStroke.setColor(skcol::fg);
	mkStroke.setStyle(SkPaint::kStroke_Style);
	mkStroke.setStrokeWidth(2.0f);
	if (hoz) {
		// Top triangle at pos_first
		{
			SkPath tri;
			float x = x0 + pos_first;
			float y = y0 + markerheight;
			tri.moveTo(x - width_triangle / 2, y);
			tri.lineTo(x, y + height_triangle);
			tri.lineTo(x + width_triangle / 2, y);
			tri.close();
			canvas->drawPath(tri, mkFill);
			canvas->drawPath(tri, mkStroke);
		}
		// Top rect at pos_first
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_first - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_first - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		// Repeat for pos_second (top only)
		{
			SkPath tri2;
			float x2 = x0 + pos_second;
			float y2 = y0 + markerheight;
			tri2.moveTo(x2 - width_triangle / 2, y2);
			tri2.lineTo(x2, y2 + height_triangle);
			tri2.lineTo(x2 + width_triangle / 2, y2);
			tri2.close();
			canvas->drawPath(tri2, mkFill);
			canvas->drawPath(tri2, mkStroke);
		}
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_second - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_second - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		// Bottom triangle at pos_grain
		{
			SkPath tri3;
			float x3 = x0 + pos_grain;
			float y3 = y0 + height - markerheight;
			tri3.moveTo(x3 - width_triangle / 2, y3);
			tri3.lineTo(x3, y3 - height_triangle);
			tri3.lineTo(x3 + width_triangle / 2, y3);
			tri3.close();
			canvas->drawPath(tri3, mkFill);
			canvas->drawPath(tri3, mkStroke);
		}
		// Bottom rect at pos_grain
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_grain - markerwidth * 0.5f,
				y0 + height - markerheight,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_grain - markerwidth * 0.5f,
				y0 + height - markerheight,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);
	}
	else {
		// Left triangle at pos_first
		{
			SkPath tri;
			float x = x0 + markerwidth;
			float y = y0 + pos_first;
			tri.moveTo(x, y - height_triangle / 2);
			tri.lineTo(x + width_triangle, y);
			tri.lineTo(x, y + height_triangle / 2);
			tri.close();
			canvas->drawPath(tri, mkFill);
			canvas->drawPath(tri, mkStroke);
		}
		// Left rect at pos_first
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0,
				y0 + pos_first - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0,
				y0 + pos_first - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		{
			SkPath tri2;
			float x = x0;                                  // left edge
			float y2 = y0 + pos_second;
			tri2.moveTo(x + markerwidth, y2 - height_triangle / 2);
			tri2.lineTo(x + markerwidth + width_triangle, y2);
			tri2.lineTo(x + markerwidth, y2 + height_triangle / 2);
			tri2.close();
			canvas->drawPath(tri2, mkFill);
			canvas->drawPath(tri2, mkStroke);
		}

		// Left rect at pos_second
		{
			SkRect r = SkRect::MakeXYWH(
				x0,
				y0 + pos_second - markerheight * 0.5f,
				markerwidth,
				markerheight
			);
			canvas->drawRoundRect(r, corner_radius, corner_radius, mkFill);
			canvas->drawRoundRect(r, corner_radius, corner_radius, mkStroke);
		}
		// Bottom triangle at pos_grain (vertical mode)
		{
			SkPath tri3;
			float x3 = x0 + width - markerwidth;
			float y3 = y0 + pos_grain;
			tri3.moveTo(x3, y3 - height_triangle / 2);
			tri3.lineTo(x3 - width_triangle, y3);
			tri3.lineTo(x3, y3 + height_triangle / 2);
			tri3.close();
			canvas->drawPath(tri3, mkFill);
			canvas->drawPath(tri3, mkStroke);
		}
		// Bottom rect at pos_grain (vertical)
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + width - markerwidth,
				y0 + pos_grain - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + width - markerwidth,
				y0 + pos_grain - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);
	}

	// --- 8. Cleanup ---
	canvas->restore();
}

void Waveform2::render(void* context) {

	// --- 1. Setup and buffer fetch ---
	TRACK* active = _DATA->tracks[_STATE->active_track.load(std::memory_order_acquire)];
	bool normalloop = active->fxpower[SPACE_LOOPER].load(std::memory_order_acquire);

	TRACK* source =
		_DATA->tracks[(int)_STATE->params[active->index][DISTRSOURCE].load(std::memory_order_acquire)];

	TRACK* t = (normalloop ==
		source->fxpower[SPACE_LOOPER].load(std::memory_order_acquire)) ? source : active;
	
	//waveforms[t->index].render(context);
	//return;
	auto& waveform = waveforms[t->index];
	const bool ml = _STATE->midilearning.load();
	auto data = waveform.renderData.load(std::memory_order_acquire);
	auto rec = data != nullptr ? data->filebuffer : nullptr;
	auto state = rec ? rec->state.load(std::memory_order_acquire) : nullptr;
	auto off = state == nullptr ? 0 : rec->off;
	if (off == 0) {
		flush(static_cast<SkCanvas*>(context));
		return;
	}
	auto fb = rec->buffer;
	auto w = (double)width;
	auto h = (double)height;
	auto x0 = (double)startx;
	auto y0 = (double)starty;
	auto channels = rec->channels;
	bool hoz = orientation == HORIZONTAL;

	auto grainsize = _STATE->params[active->index][GRAINSIZE].load(std::memory_order_acquire);

	double start, zoom, off_stop, off_start, offset;
	int wlast;
	auto surface = _appState->graphics.getSurface(waveform.windex);
	{
		std::lock_guard lock(mtx);
		if (rec->dorendering.exchange(false, std::memory_order_acq_rel) || !surface ||
			surface->width() != width || surface->height() != height) {
			waveform.render_surface_static();
			surface = _appState->graphics.getSurface(waveform.windex);
		}

		// --- 2. Compute geometry & parameters ---
		offset = (normalloop && rec->ismoving ? data->offsetLoop : state->offset);
		off_start = (normalloop && rec->isresizing ? data->posStartLoop : state->off_start);
		off_stop = (normalloop && rec->isresizing ? data->posStopLoop : state->off_stop);
		const auto ws = state->waveformState.load();
		zoom = ws.zoom;
		start = ws.startPos;
		wlast = data->last.load(std::memory_order_acquire);
	}
	double startframe = start * off;
	double framesvis = off / zoom;
	double framesperpx = framesvis / (hoz ? w : h);

	double pos_start = (off_start - startframe) / framesperpx;
	double pos_stop = (off_stop - startframe) / framesperpx;
	double pos_grain = (offset - startframe) / framesperpx;

	double markerSize = grainsize / off * (hoz ? w : h) * zoom;

	double pos_first = (wlast == OFF_START ? pos_stop : pos_start);
	double pos_second = (pos_first == pos_stop ? pos_start : pos_stop);

	// --- 3. Setup Skia paints ---
	SkCanvas* canvas = static_cast<SkCanvas*>(context);
	canvas->save();

	SkPaint bgPaint;
	bgPaint.setColor(skcol::bg);
	canvas->drawRect(SkRect::MakeXYWH(x0, y0, w, h), bgPaint);

	SkPaint wavePaint;
	wavePaint.setColor(skcol::waveform);
	wavePaint.setStrokeWidth(2.0f);

	SkPaint bouncePaint;
	bouncePaint.setColor(skcol::bounce_colours[state->bounceType.load(std::memory_order_acquire)]);
	bouncePaint.setStrokeWidth(3.0f);

	SkPaint grainPaint;
	grainPaint.setColor(SK_Colour(127, 127, 127, 127));
	grainPaint.setStyle(SkPaint::kFill_Style);

	SkPaint grainLine;
	grainLine.setColor(skcol::yellow);
	grainLine.setStrokeWidth(3.0f);
	// --- 4. Clip to waveform rect ---


	canvas->clipRect(SkRect::MakeXYWH(x0, y0, w, h));
	if (surface) {
		surface->draw(canvas, x0, y0);
	}

	// --- 6. Draw bounce markers ---
	if (hoz) {
		canvas->drawLine(x0 + pos_first, y0, x0 + pos_first, y0 + h, bouncePaint);
		canvas->drawLine(x0 + pos_second, y0, x0 + pos_second, y0 + h, bouncePaint);
	}
	else {
		canvas->drawLine(x0, y0 + pos_first, x0 + w, y0 + pos_first, bouncePaint);
		canvas->drawLine(x0, y0 + pos_second, x0 + w, y0 + pos_second, bouncePaint);
	}

	// --- 7. Grain marker or fade region ---
	if (!normalloop) {
		// simple grain marker
		if (hoz) {
			canvas->drawRect(
				SkRect::MakeXYWH(x0 + pos_grain, y0, markerSize, h),
				grainPaint
			);
			canvas->drawLine(
				x0 + pos_grain, y0,
				x0 + pos_grain, y0 + h,
				grainLine
			);
		}
		else {
			canvas->drawRect(
				SkRect::MakeXYWH(x0, y0 + pos_grain, w, markerSize),
				grainPaint
			);
			canvas->drawLine(
				x0, y0 + pos_grain,
				x0 + w, y0 + pos_grain,
				grainLine
			);
		}
	}
	else {
		double vala, valb, fob = 0, dist;
		const int dir = state->playbackDir.load(std::memory_order_acquire);
		const int bounce_type = state->bounceType.load(std::memory_order_acquire);

		int fadesamples = 1 + (int)(_STATE->sr / 1000. *
			_STATE->params[active->index][LOOPERFADE].load(
				std::memory_order_acquire));
		double distance = DISTANCEF(off_start, off_stop);
		if (distance < 1.)
			distance = 1.;
		if (fadesamples > distance)
			fadesamples = (int)distance;
		double onedfadesamples = 1. / (double)fadesamples;
		if (bounce_type != NO_BOUNCE) {

			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_stop + dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_start - dist;
			}
		}
		else {
			if (dir > 0) {
				dist = DISTANCEF(offset, off_stop);
				fob = off_start - dist;

			}
			else {
				dist = DISTANCEF(offset, off_start);
				fob = off_stop + dist;
			}
		}

		if (dist <= fadesamples) {
			vala = (dist * onedfadesamples);
			valb = (1.0 - vala);
		}
		else {
			vala = 1.0;
			valb = .0;
		}

		float pos_fade = (fob - startframe) / framesperpx;


		auto colpos = SK_Colour(255.f, 235.f, 59.f, 204.f * vala);
		auto colfade = SK_Colour(255.f, 235.f, 59.f, 204.f * valb);
		SkPaint pPos, pFade;
		pPos.setColor(colpos);
		pPos.setStrokeWidth(3.0f);
		pFade.setColor(colfade);
		pFade.setStrokeWidth(3.0f);

		if (hoz) {
			canvas->drawLine(x0 + pos_grain, y0, x0 + pos_grain, y0 + h, pPos);
			canvas->drawLine(x0 + pos_fade, y0, x0 + pos_fade, y0 + h, pFade);

		}
		else {
			canvas->drawLine(x0, y0 + pos_grain, x0 + w, y0 + pos_grain, pPos);
			canvas->drawLine(x0, y0 + pos_fade, x0 + w, y0 + pos_fade, pFade);


		}

		if (bounce_type == BOUNCE_NO_REVERSE) {
			double pos_bi;
			if (dir > 0) {
				pos_bi = (off_start + dist - startframe) / framesperpx;
				pos_fade = (off_start - dist - startframe) / framesperpx;

			}
			else {
				pos_bi = (off_stop - dist - startframe) / framesperpx;
				pos_fade = (off_stop + dist - startframe) / framesperpx;

			}
			if (hoz) {
				canvas->drawLine(x0 + pos_bi, y0, x0 + pos_bi, y0 + h, pPos);
				canvas->drawLine(x0 + pos_fade, y0, x0 + pos_fade, y0 + h, pFade);
			}
			else {
				canvas->drawLine(x0, y0 + pos_bi, x0 + w, y0 + pos_bi, pPos);
				canvas->drawLine(x0, y0 + pos_fade, x0 + w, y0 + pos_fade, pFade);
				canvas->drawLine(x0, y0 + pos_bi, x0 + w, y0 + pos_bi, pPos);
			}

		}
	}
	SkPaint mkFill, mkStroke;
	mkFill.setColor(SK_ColorBLACK);
	mkFill.setStyle(SkPaint::kFill_Style);
	mkStroke.setColor(ml ? skcol::midilearning : skcol::fg);
	mkStroke.setStyle(SkPaint::kStroke_Style);
	mkStroke.setStrokeWidth(2.0f);
	if (hoz) {
		// Top triangle at pos_first
		{
			SkPath tri;
			float x = x0 + pos_first;
			float y = y0 + markerheight;
			tri.moveTo(x - width_triangle / 2, y);
			tri.lineTo(x, y + height_triangle);
			tri.lineTo(x + width_triangle / 2, y);
			tri.close();
			canvas->drawPath(tri, mkFill);
			canvas->drawPath(tri, mkStroke);
		}
		// Top rect at pos_first
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_first - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_first - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		// Repeat for pos_second (top only)
		{
			SkPath tri2;
			float x2 = x0 + pos_second;
			float y2 = y0 + markerheight;
			tri2.moveTo(x2 - width_triangle / 2, y2);
			tri2.lineTo(x2, y2 + height_triangle);
			tri2.lineTo(x2 + width_triangle / 2, y2);
			tri2.close();
			canvas->drawPath(tri2, mkFill);
			canvas->drawPath(tri2, mkStroke);
		}
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_second - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_second - markerwidth * 0.5f,
				y0,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		// Bottom triangle at pos_grain
		{
			SkPath tri3;
			float x3 = x0 + pos_grain;
			float y3 = y0 + height - markerheight;
			tri3.moveTo(x3 - width_triangle / 2, y3);
			tri3.lineTo(x3, y3 - height_triangle);
			tri3.lineTo(x3 + width_triangle / 2, y3);
			tri3.close();
			canvas->drawPath(tri3, mkFill);
			canvas->drawPath(tri3, mkStroke);
		}
		// Bottom rect at pos_grain
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_grain - markerwidth * 0.5f,
				y0 + height - markerheight,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + pos_grain - markerwidth * 0.5f,
				y0 + height - markerheight,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);
	}
	else {
		// Left triangle at pos_first
		{
			SkPath tri;
			float x = x0 + markerwidth;
			float y = y0 + pos_first;
			tri.moveTo(x, y - height_triangle / 2);
			tri.lineTo(x + width_triangle, y);
			tri.lineTo(x, y + height_triangle / 2);
			tri.close();
			canvas->drawPath(tri, mkFill);
			canvas->drawPath(tri, mkStroke);
		}
		// Left rect at pos_first
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0,
				y0 + pos_first - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0,
				y0 + pos_first - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);

		{
			SkPath tri2;
			float x = x0;                                  // left edge
			float y2 = y0 + pos_second;
			tri2.moveTo(x + markerwidth, y2 - height_triangle / 2);
			tri2.lineTo(x + markerwidth + width_triangle, y2);
			tri2.lineTo(x + markerwidth, y2 + height_triangle / 2);
			tri2.close();
			canvas->drawPath(tri2, mkFill);
			canvas->drawPath(tri2, mkStroke);
		}

		// Left rect at pos_second
		{
			SkRect r = SkRect::MakeXYWH(
				x0,
				y0 + pos_second - markerheight * 0.5f,
				markerwidth,
				markerheight
			);
			canvas->drawRoundRect(r, corner_radius, corner_radius, mkFill);
			canvas->drawRoundRect(r, corner_radius, corner_radius, mkStroke);
		}
		// Bottom triangle at pos_grain (vertical mode)
		{
			SkPath tri3;
			float x3 = x0 + width - markerwidth;
			float y3 = y0 + pos_grain;
			tri3.moveTo(x3, y3 - height_triangle / 2);
			tri3.lineTo(x3 - width_triangle, y3);
			tri3.lineTo(x3, y3 + height_triangle / 2);
			tri3.close();
			canvas->drawPath(tri3, mkFill);
			canvas->drawPath(tri3, mkStroke);
		}
		// Bottom rect at pos_grain (vertical)
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + width - markerwidth,
				y0 + pos_grain - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkFill
		);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(
				x0 + width - markerwidth,
				y0 + pos_grain - markerheight * 0.5f,
				markerwidth,
				markerheight
			),
			corner_radius, corner_radius,
			mkStroke
		);
	}

	// --- 8. Cleanup ---
	canvas->restore();
}


#endif

void Waveform::setup(std::shared_ptr<tsl::Recording>& rec) {
	const double multi = 1.5;

	if (rec == nullptr || rec->off == 0) {
		renderData.store(nullptr, std::memory_order_release);
		return;
	}
	else rec->trackIndex = associatedtrack->index;

	const int end = orientation == VERTICAL ? height : width;
	const auto channels = rec->channels;
	const auto offset = rec->off;
	//offset -= 100;
	const float zoomgap = offset / (30. * _STATE->sr);
	float zoom = 1.0f;

	if (rec->fileName == "Microphone Recording") {
		auto& fb = rec->buffer; // reference not copy
		float max = 0;
		for (int i = 0; i < channels; i++)
			for (auto j = 0; j < offset; j++)
				max = std::max(max, std::abs((float)fb[i].at(j)));

		if (max > CONV16BIT) { // only scale down if clipping
			float scalefactor = CONV16BIT / max;
			for (int i = 0; i < channels; i++)
				for (auto j = 0; j < offset; j++)
					fb[i].at(j) = (short)(fb[i].at(j) * scalefactor);
		}
	}
	auto data = std::make_shared<WaveformRenderData>();
	data->filebuffer = rec;

	float zooms[] = { 5, 3, 2, 2, 2, 2, 1, 1 };

	if (zoomgap > 1) {
		data->wave_s = std::make_unique<WAVE_S>(_appState);

		auto first = data->wave_s.get();
		first->width = (int)width;
		first->height = (int)height;
		first->zoom = zoom;
		first->render(rec, orientation);
		int count = 0;
		while (zoom < zoomgap) {
			zoom *= 1.5;
			auto temp = new WAVE_S(_appState);
			temp->width = (int)(orientation == VERTICAL ? width : width * zoom);
			temp->height = (int)(orientation == VERTICAL ? height * zoom : height);
			temp->zoom = zoom;
			temp->render(rec, orientation);
			first->next = temp;
			first = temp;
		}
	}
	rec->dorendering.store(true);
	renderData.store(data, std::memory_order_release);
}