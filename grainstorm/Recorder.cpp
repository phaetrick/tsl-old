//
// Created by pr on 29.03.18.
//
#include "Recorder.h"
#ifdef _WIN32
#include <intrin.h>
#endif
#include "grainstorm.h"
#include "player.h"
#include "track.h"
#include "view.h"
#include "tools/queuetsl.h"
#include "colours.h"
#include "button.h"
#include "infopanel.h"
#include "waveform.h"
#include "gui/gui.h"
#include "Input.h"
#include "logger.h"
#include <app.h>

#if defined USE_IMGUI
#include <imgui.h>
#else

void tsl::graphics::RecorderView::delRecursiveDraw() {
	_appState->graphics.deleteWindow(windex);
	windex = -1;
	View::delRecursiveDraw();
};


#endif

#include <atomic>
#include <array>
#include <functional>
#include <optional>
#include <cstdint>
#include <atomic>
#include <array>
#include <functional>
#include <cstdint>

#include <atomic>
#include <array>
#include <cstdint>
#include <functional>


#include <thread>
#include <vector>
#include <random>
#include <atomic>
#include <array>
#include <chrono>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

using namespace tsl::graphics;

void RecorderView::init() {
	width = _STATE->windowWidth * .5f;
	height = _STATE->windowHeight * .5f;
	startx = (_STATE->windowWidth - width) * .5f;
	starty = (_STATE->windowHeight - height) * .5f;
	stopx = startx + width;
	stopy = starty + height;
}


void RecorderView::callback(const InputEvent& event) {
	float xpos = event.x;
	float ypos = event.y;
	int32_t action = event.action;
	switch (action) {
	case ACTION_DOWN:
		lastx = xpos;
		lasty = ypos;
		dragid = event.pointer_id;
		break;

	case ACTION_UP:
		if (event.pointer_id == dragid)
			dragid = -1;
		break;
	case ACTION_MOVE:
		// Only the finger that started the drag moves the window -- a second
		// finger used to apply its delta against the first one's lastx/lasty.
		if (event.pointer_id == dragid) {
			startx += (xpos - lastx);
			stopx = startx + width;
			starty += (ypos - lasty);
			stopy = starty + height;
			lastx = xpos;
			lasty = ypos;
		}
		break;

	default:
		break;
	}
}


void RecorderView::render(void* context) {
#if defined PLUGIN_MODE || defined STANDALONE_MODE
	const int channels = MAX_CHANNELS;
#else
	const int channels = 1;
#endif


#if defined USE_IMGUI


	ImGui::SetNextWindowPos(ImVec2(startx, starty),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(width, height),
		ImGuiCond_Always);
	bool test = true;
	if (!ImGui::Begin("Decode", &test,
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDecoration)) {
		ImGui::End();
		return;
	}
	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	ImVec2 canvas_size = ImGui::GetContentRegionAvail();        // Resize canvas to what's available
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
	float w = canvas_size.x, h = canvas_size.y;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y +
		canvas_size.y));// clip lines within the canvas (i

#else
	auto canvas = _appState->graphics.getCanvas(windex, startx, starty, width, height);
	if (!canvas) {
		return;
	}
	SkPath waveformPath;
	waveformPath.setFillType(SkPathFillType::kWinding);
	float w = width, h = height;

#endif
	Recorder& r = _DATA->recorder;
	const int32_t orientation = VERTICAL; //view->orientation;
	const int32_t end = orientation == VERTICAL ? height : width;
	const int32_t threeseconds = _STATE->sr * 3;
	auto samples = r.fileBufferSize.load();
	int startframe = samples - threeseconds;
	const auto frames_per_bin = (float)threeseconds / (float)end;

	int x = 0;

	const float gain = .9 * CONVMYFLT;

	auto frames_per_buf = (int32_t)(floor(frames_per_bin));


	int offset = 0;
	const float drawSizePerChannel = orientation == VERTICAL ? (float)w / (float)channels : (float)h / (float)channels;
	const float half = 0.5f * drawSizePerChannel;

	{
		std::lock_guard lk(r.mutex);
		/* Bound against the live size of each channel, read under the lock.
		   fileBufferSize above is only a hint: the stop path stores 0 into it
		   and then resizes the channels one at a time, so it is neither current
		   nor equal across channels by the time we get here. Indexing on the
		   stale value read a freed buffer -- SIGSEGV in this function.

		   This used to be impossible: deldraw() removed the view under
		   queue_draw's mutex, which the render walk held for its whole
		   duration, so the recorder blocked until the frame ended and the view
		   was gone before the teardown ran. The removal is a deferred command
		   now, so the view can still render for one more frame after the
		   recorder has started freeing the buffers under it. */
		int32_t chanSamples[MAX_CHANNELS]{};
		for (int chan = 0; chan < channels; chan++)
			chanSamples[chan] = static_cast<int32_t>(r.filebuffer[chan].size());

		while (x++ < end) {
			float min[channels]{}, max[channels]{};
			for (int32_t frame = 0; frame < frames_per_buf; frame++) {
				for (int chan = 0; chan < channels; chan++) {
					const float sample_val =
						startframe < 0 || startframe >= chanSamples[chan] ? 0 : r.filebuffer[chan][startframe];
					max[chan] = std::max(max[chan], sample_val);
					min[chan] = std::min(min[chan], sample_val);
				}
				++startframe;
			}
			for (int chan = 0; chan < channels; chan++) {

				float minon1 = (chan * drawSizePerChannel) + half + half * min[chan] * gain;
				float maxoff1 = (chan * drawSizePerChannel) +
					half + half * max[chan] * gain;
#if defined USE_IMGUI
				draw_list->AddLine(
					ImVec2(canvas_pos.x + minon1, canvas_pos.y + x),
					ImVec2(canvas_pos.x + maxoff1, canvas_pos.y + x),
					IM_COL32(255, 255, 255, 255), 2.0);
#else
				if (orientation == VERTICAL) {
					waveformPath.moveTo(minon1, x);
					waveformPath.lineTo(maxoff1, x);
				}
				else {
					waveformPath.moveTo(x, minon1);
					waveformPath.lineTo(x, maxoff1);
				}
#endif
			}
			offset += frames_per_buf;
			frames_per_buf = (int)(
				floorf((x + 2) * frames_per_bin) - offset);
		}
	}
#if defined USE_IMGUI
	ImGui::End();
#else
	canvas->clear(SK_Colour(15, 15, 15, 230));
	// Draw the path
	SkPaint paint;
	paint.setColor(skcol::waveform);
	paint.setStrokeWidth(2.0f);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setAntiAlias(true);
	canvas->drawPath(waveformPath, paint);
	borderWindow(canvas, skcol::grey);
#endif
}


#define GAP 420

#define NUM_REC_BUFFERS 20;
#ifdef __ANDROID__

int32_t java_recorder_control(JNIEnv* env, jclass obj, jint action) {
	auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 1;

	Recorder& recorder = _DATA->recorder;
	switch ((int)action) {
	case RECORDER_START:;//recorder.Record(_STATE->active_track.load(), getInstance()->bufsize_init);
		break;
	case RECORDER_STOP:
		recorder._isRecording.store(false);
		break;
	default:
		break;
	}
	return 0;
}

jlong java_recorder_callback(JNIEnv* env, jclass obj, jlong track, jint bufsize) {
	auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 0;

	return _DATA->recorder.Record((TRACK*)track, bufsize);
};


void startrecfunc(TRACK* track) {
	ATTACH
		mid = env->GetStaticMethodID(tsl::android::recorderclass, "Record", "(J)J");
	env->CallStaticLongMethod(tsl::android::recorderclass, mid, (jlong)track);
	DETACH
}


void OboeRecorder::onErrorBeforeClose(AudioStream* audioStream, Result error) {
	const auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;
	_DATA->recorder._isRecording = false;
}

void OboeRecorder::onErrorAfterClose(AudioStream* audioStream, Result error) {
	if (error == Result::ErrorDisconnected) {
		const auto _appState = __STATE;
		if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)
			return;
		_DATA->recorder._isRecording = false;
	}
}

static int32_t processBuf(const std::shared_ptr<tsl::Player::RecordingContext>& w, int16_t* buf,
	const int32_t bufsize) {
	const auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 0;
	Recorder& recorder = _DATA->recorder;
	if (!recorder._isRecording.load()) {
		return 0;
	}
	int32_t channels = _STATE->channels;
	auto& offset = recorder.offset;


	float peak = 0.0001f;

	if (offset == 0) {
		float sum = 0;
		for (int32_t i = 0; i < bufsize; i++) {
			float smpl = (float)buf[i] * CONVMYFLT;
			sum += (smpl * smpl);
		}
		float rms = sqrt(sum / (float)bufsize);
		if (rms < 0.001) {
			_STATE->peak[0] = _STATE->peak[1] = peak;
			return 0;
		}
	}

	if (w != nullptr) {
		if (w->wrapper.write(buf, sizeof(int16_t), bufsize) != bufsize) {
			std::string message(recorder.message);
			message.append("Write Error.");
			showToast(_appState, message.c_str());
			recorder._isRecording.store(false);
		}

		_DATA->views.infopanel->bytes = w->wrapper.tell();
	}
	else
		_DATA->views.infopanel->bytes =
		recorder.filebuffer[0].size() * _STATE->channels * sizeof(short);

	int32_t processed = 0;

	{
		std::lock_guard lk(recorder.mutex);
		try {
			for (int i = 0; i < _STATE->channels; i++, offset < recorder.gap)
				recorder.filebuffer[i].reserve(offset + bufsize);
		}
		catch (std::bad_alloc& e) {
			std::string message(recorder.message);
			message.append("Out of Memory.");
			showToast(_appState, message.c_str());
			recorder._isRecording.store(false);
			return 0;
		}
	}

	for (; processed < bufsize; processed++, offset++) {
		float tmp = std::abs((float)buf[processed] * CONVMYFLT);
		peak = tmp > peak
			? tmp : peak;
		if (offset < recorder.gap) {
			for (int32_t j = 0; j < channels; j++) {
				recorder.filebuffer[j].push_back(buf[processed]);
			}
		}

		if (w == nullptr && offset >= recorder.gap) {
			std::string message = recorder.message;
			message.append("7 Minutes Max reached.");
			recorder._isRecording.store(false);
			showToast(_STATE, message.data());
			break;
		}
		else if (offset >= _STATE->sr * 60 * 60) {
			recorder._isRecording.store(false);
			std::string message = recorder.message;
			message.append("stopping after 60 Minutes");
			showToast(_STATE, message.data());
			break;
		}
	}
	_STATE->peak[0] = _STATE->peak[1] = peak;
	recorder.fileBufferSize.store(recorder.filebuffer[0].size());
	_DATA->views.infopanel->dec_offset.store(offset);
	return processed;
}


long Recorder::Record(TRACK* track, int32_t bufsize) {
	const auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 0;
	auto tindex = track->index;
	const int32_t sr = _STATE->sr;
	gap = (size_t)_STATE->sr * (7 * 60) + 3;
	_track = track;
	offset = 0;
	fileBufferSize.store(0);
	recformat = _STATE->format;
	{
		ATTACH
			jclass activityClass = tsl::android::recorderclass;
		mid = env->GetStaticMethodID(activityClass, "getFD", "()I");
		fd = (int)env->CallStaticIntMethod(activityClass, mid);
		DETACH
	}

	if (fd == -1) {
		return 0;
	}

	message = track->name;
	message.append(" Microphone Recording ");
	std::shared_ptr<tsl::Player::RecordingContext> w = nullptr;
	if (fd != 0) {
		FILE* f = nullptr;
		f = fdopen(fd, "wb");
		if (f == nullptr) {
			if (fd)close(fd);
			return -1;
		}
		w = tsl::Player::setupRecording(__STATE, f, message, 1,
			tsl::AudioFileWrapper::SampleFormat::SAMPLE_INT16);
		if (w == nullptr) {
			if (f)fclose(f);
			return -1;
		}
	}
	std::shared_ptr<AudioStream> stream;
	AudioStreamBuilder builder;
	// builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	builder.setDirection(oboe::Direction::Input);
	// The engine's rate, converted if the microphone does not run at it.
	//
	// sr is tsl::kEngineSampleRate now, not the device's rate, so on 44.1 kHz
	// hardware this is a genuine request for conversion rather than a
	// formality. Without the quality set, Oboe does no conversion of its own
	// (SampleRateConversionQuality defaults to None) and the stream comes back
	// at whatever the input device felt like -- which nothing here would
	// notice, because the file header below is written from _STATE->sr. The
	// recording would be silently off-pitch by the ratio, and so would every
	// grain read from it.
	builder.setSampleRate(sr);
	builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
	builder.setChannelCount(1);
	builder.setFormat(AudioFormat::I16);
	builder.setFramesPerDataCallback(bufsize);
	builder.setErrorCallback(&oboeRecorder);
	builder.setAudioApi(tsl::android::useAAudio ? AudioApi::AAudio : AudioApi::OpenSLES);
	int32_t micrecformat = _DATA->micrecformat;
	InputPreset inputPreset = InputPreset::Generic;
	if (micrecformat == 6)
		inputPreset = InputPreset::VoiceRecognition;
	else if (micrecformat == 9)
		inputPreset = InputPreset::Unprocessed;
	builder.setInputPreset(inputPreset);
	Result result = builder.openStream(stream);
	if (result != Result::OK) {
		message.append(convertToText(result));
		showToast(_STATE, message.c_str());
		return 0;
	}
	LOGD("Frames per burst %d", stream->getFramesPerBurst());
	LOGD("API: %s Mode: %s", stream->getAudioApi() == AudioApi::AAudio ? "AAudio" : "OpenSLES",
		stream->getSharingMode() == SharingMode::Exclusive ? "Exclusive" : "NonExclusive");

	// Refuse rather than record at the wrong pitch. The header has already been
	// written with sr, and a rate mismatch is inaudible as a fault -- it just
	// makes the recording, and every grain taken from it, play sharp or flat.
	if (stream->getSampleRate() != sr) {
		LOGE("Mic opened at %d, engine runs at %d -- refusing to record",
			stream->getSampleRate(), sr);
		stream->close();
		message.append("Microphone rate mismatch");
		showToast(_STATE, message.c_str());
		return 0;
	}

	//QADD(_DATA->queue_draw, _DATA->views.micbutton, (void(*)(void*, ...))render_mic, PERMANENT);


	result = stream->requestStart();
	if (result != Result::OK) {
		stream->close();
		message.append(convertToText(result));
		showToast(_STATE, message.c_str());
		return 0;
	}

	{
		std::lock_guard lk(mutex);   // RecorderView::render may still be reading
		for (int32_t i = 0; i < _STATE->channels; i++) {
			filebuffer[i].clear();
		}
	}

	auto l_info = _DATA->views.infopanel;
	l_info->dec_offset = 0;
	l_info->bytes = 0;
	l_info->progress = -1;
	l_info->text.store(track->name);
	l_info->text2.store(" RECORDING");
	l_info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);


	bool isplaying = false;
	{
		std::lock_guard lk(_STATE->player);
		isplaying = _STATE->player._isplaying.load() && _DATA->pauseplayback.load();
		if (isplaying)
			_STATE->player.stop();
	}

	_STATE->params[track->index][MICROPHONEButton].store(1.0);
	if (tindex == _STATE->active_track.load() && GASMAIN == SPACE_WAVEFORM)
		_STATE->parameters[MICROPHONEButton].view->redraw();

	_DATA->views.recorderView->redraw();
	_DATA->views.recorderView->addCB();


	_isRecording = true;
	std::string m1(message);
	m1.append("started.");
	showToast(_STATE, m1.c_str());
	auto buffer = _STATE->pool.acquire<short>(bufsize);

	while (_isRecording.load()) {
		oboe::ResultWithValue<int32_t> res = stream->read(buffer,
			bufsize,
			1000000000 /* timeout */);
		int32_t framesRead = 0;
		if (!res || (framesRead = res.value()) != bufsize) {
			std::string s = message;
			s.append("Error: ");
			s.append(convertToText(result));
			s.append(" ");
			s.append(std::to_string(res.value()));
			showToast(_STATE, s.c_str());
			break;
		}
		else {
			processBuf(w, buffer, bufsize);
		}
	}
	_STATE->pool.release(buffer);

	result = stream->stop();
	if (result != Result::OK) {
		std::string s = message;
		s.append("Error: ");
		s.append(convertToText(result));
		showToast(_STATE, s.c_str());
	}
	result = stream->close();
	if (result != Result::OK) {
		std::string s = message;
		s.append("Error: ");
		s.append(convertToText(result));
		showToast(_STATE, s.c_str());
	}

	_STATE->params[track->index][MICROPHONEButton].store(0.0);
	if (tindex == _STATE->active_track.load() && GASMAIN == SPACE_WAVEFORM)
		_STATE->parameters[MICROPHONEButton].view->redraw();
	_DATA->views.infopanel->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);


	_DATA->views.recorderView->deldraw();
	_DATA->views.recorderView->delCB();
	fileBufferSize.store(0);
	l_info->dec_offset = 0;
	//QDEL(_DATA->queue_draw, _DATA->views.micbutton);

	if (w != nullptr) {
		w->wrapper.close();
		ATTACH
			jclass activityClass = tsl::android::recorderclass;
		mid = env->GetStaticMethodID(activityClass, "closeFD", "(Z)V");
		env->CallStaticVoidMethod(activityClass, mid, offset == 0);
		DETACH
	}

	if (offset == 0) {
		if (isplaying)
			_STATE->player.play();
		return 0;
	}
	else if (offset > gap)
		offset = gap;


	/* Both of these mutate the buffers out from under RecorderView::render,
	   which may still run for one more frame -- deldraw() above only queues the
	   removal now. The resize reallocates, and the Recording constructor MOVES
	   (buffer[i] = std::move(data[i])), so it steals the data pointer outright. */
	std::shared_ptr<tsl::Recording> rec;
	{
		std::lock_guard lk(mutex);
		for (int32_t i = 0; i < _STATE->channels; i++) {
			filebuffer[i].resize(filebuffer[0].size(), 0);
		}
		rec = std::make_shared<tsl::Recording>(_STATE, filebuffer, _STATE->channels, "Microphone Recording");
	}
	/* Waveform first, THEN loadAudio -- the decoder's order
	   (DecoderAndroid.cpp:663 queues loadAudio and calls setup immediately).
	   loadAudio still ends in current->pushSwap(), which copies the whole
	   outgoing recording into a swap page and measured 3.5 s on device, so
	   anything sequenced after it inherits that wait. setup() needs only `rec`
	   and does not care whether the track has adopted it yet. */
	track->waveform->setup(rec);
	auto e = track->loadAudio(rec);
	if (e.poolHandle < tsl::INVALID_POOL_HANDLE)
		_DATA->snapShot.addEvent(e);
	offset = 0;

	if (isplaying)
		_STATE->player.play();
	return track->offToInfo.load();
}

#else
#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <cassert>

namespace ut {

	struct Node {

		struct Tag {
			static constexpr uintptr_t version_mask = 0x4;
			static constexpr uintptr_t ptr_mask = 0xFFFFFFFFFFFFFFF0;

			Tag() noexcept = default;

			explicit Tag(uintptr_t ptr, uintptr_t version) noexcept
				: m_ptr(ptr& ptr_mask) {
				assert(version < (1u << 4) - 1);
				m_ptr |= version;
			}

			explicit Tag(Node* ptr, uintptr_t version) noexcept
				: Tag(reinterpret_cast<uintptr_t>(ptr), version) {
			}

			bool operator==(const Tag& rhs) const noexcept {
				return m_ptr == rhs.m_ptr;
			}

			operator Node* () const noexcept {
				return reinterpret_cast<Node*>(m_ptr & ptr_mask);
			}

			uint32_t version() const {
				return m_ptr & version_mask;
			}

			Tag next_version() const noexcept {
				const auto v = version();
				assert(v < (1u << 4) - 1);

				return Tag(m_ptr, v + 1);
			}

			uintptr_t m_ptr{};
		};

		static_assert(sizeof(Tag) == 8, "Tag must be 8 bytes");

		Node() = default;

		virtual ~Node() = default;

		void init() {
			Tag null_tag{};

			m_next.store(null_tag, std::memory_order_relaxed);
			m_prev.store(null_tag, std::memory_order_relaxed);
		}

		void prefetch_next() const {
			/* Read, high temporal locality */
			if (auto next = (Node*)(m_next.load(std::memory_order_acquire))) {
	#ifdef _WIN32
				_mm_prefetch((const char*)next, _MM_HINT_T0);
#else
				__builtin_prefetch(next, 0, 3);
#endif
			}
		}

		void prefetch_prev() const {
			/* Read, high temporal locality */
			if (auto prev = (Node*)m_prev.load(std::memory_order_acquire)) {
#ifdef _WIN32
				_mm_prefetch((const char*)prev, _MM_HINT_T0);
#else
				__builtin_prefetch(prev, 0, 3);
#endif
			}
		}

		std::atomic<Tag> m_next{};
		std::atomic<Tag> m_prev{};
	};

	template <typename T>
	struct Lock_free_list {

		class iterator;
		class const_iterator;

		struct iterator {
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::bidirectional_iterator_tag;

			iterator() noexcept : m_node(nullptr) {}

			explicit iterator(Node* node, Node* prev = nullptr) noexcept
				: m_node(node), m_prev(prev) {
			}

			reference operator*() const {
				if (m_node == nullptr) {
					throw std::runtime_error("Dereferencing null iterator");
				}
				return *static_cast<T*>(m_node);
			}

			pointer operator->() const {
				if (m_node == nullptr) {
					throw std::runtime_error("Dereferencing null iterator");
				}
				return static_cast<T*>(m_node);
			}

			iterator& operator++() {
				if (m_node == nullptr) {
					throw std::runtime_error("Incrementing null iterator");
				}

				/* Load next node with acquire semantics */
				auto next = (Node*)m_node->m_next.load(std::memory_order_acquire);

				/* Verify the node hasn't been removed */
				if ((Node*)m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
					/* Node was removed, try to recover by finding next valid node */
					while (m_node != nullptr && (Node*)m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
						m_node = (Node*)m_node->m_next.load(std::memory_order_acquire);
						if (m_node != nullptr) {
							m_prev = (Node*)m_node->m_prev.load(std::memory_order_acquire);
						}
					}
				}
				else {
					m_prev = m_node;
					m_node = next;
				}

				return *this;
			}

			iterator operator++(int) {
				auto tmp = *this;

				++(*this);
				return tmp;
			}

			iterator& operator--() {
				if (m_node == nullptr) {
					throw std::runtime_error("Decrementing begin iterator");
				}

				/* Load prev node with acquire semantics */
				auto prev = (Node*)m_prev->m_prev.load(std::memory_order_acquire);

				/* Verify the node hasn't been removed */
				if (m_prev->m_next.load(std::memory_order_acquire) != m_node) {
					/* Node was removed, try to recover by finding previous valid node */
					while (m_prev != nullptr && (Node*)m_prev->m_next.load(std::memory_order_acquire) != m_node) {
						m_prev = (Node*)m_prev->m_prev.load(std::memory_order_acquire);
						if (m_prev != nullptr) {
							m_node = (Node*)m_prev->m_next.load(std::memory_order_acquire);
						}
					}
				}
				else {
					m_node = m_prev;
					m_prev = prev;
				}

				return *this;
			}

			iterator operator--(int) {
				auto tmp = *this;
				--(*this);
				return tmp;
			}

			bool operator==(const iterator& rhs) const noexcept {
				return m_node == rhs.m_node;
			}

			bool operator!=(const iterator& rhs) const noexcept {
				return !(*this == rhs);
			}

			Node* m_node;

			/* Keep track of previous node for bidirectional iteration */
			Node* m_prev;
		};

		struct const_iterator {
			using value_type = const T;
			using pointer = const T*;
			using reference = const T&;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::bidirectional_iterator_tag;

			const_iterator() noexcept
				: m_node(nullptr), m_prev(nullptr) {
			}

			const_iterator(const iterator& other) noexcept
				: m_node(other.m_node), m_prev(other.m_prev) {
			}

			explicit const_iterator(const Node* node, const Node* prev = nullptr) noexcept
				: m_node(node), m_prev(prev) {
			}

			reference operator*() const {
				if (m_node == nullptr) {
					throw std::runtime_error("Dereferencing null iterator");
				}
				return *static_cast<const T*>(m_node);
			}

			pointer operator->() const {
				if (m_node == nullptr) {
					throw std::runtime_error("Dereferencing null iterator");
				}
				return static_cast<const T*>(m_node);
			}

			const_iterator& operator++() {
				if (m_node == nullptr) {
					throw std::runtime_error("Incrementing null iterator");
				}

				auto next = (Node*)m_node->m_next.load(std::memory_order_acquire);

				if ((Node*)m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
					while (m_node != nullptr && (Node*)m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
						m_node = (Node*)m_node->m_next.load(std::memory_order_acquire);
						if (m_node != nullptr) {
							m_prev = (Node*)m_node->m_prev.load(std::memory_order_acquire);
						}
					}
				}
				else {
					m_prev = m_node;
					m_node = next;
				}

				return *this;
			}

			const_iterator operator++(int) {
				auto tmp = *this;
				++(*this);
				return tmp;
			}

			const_iterator& operator--() {
				if (m_prev == nullptr) {
					throw std::runtime_error("Decrementing begin iterator");
				}

				auto prev = (Node*)m_prev->m_prev.load(std::memory_order_acquire);

				if ((Node*)m_prev->m_next.load(std::memory_order_acquire) != m_node) {
					while (m_prev != nullptr && (Node*)m_prev->m_next.load(std::memory_order_acquire) != m_node) {
						m_prev = (Node*)m_prev->m_prev.load(std::memory_order_acquire);
						if (m_prev != nullptr) {
							m_node = (Node*)m_prev->m_next.load(std::memory_order_acquire);
						}
					}
				}
				else {
					m_node = m_prev;
					m_prev = prev;
				}

				return *this;
			}

			const_iterator operator--(int) {
				auto tmp = *this;
				--(*this);
				return tmp;
			}

			bool operator==(const const_iterator& other) const noexcept {
				return m_node == other.m_node;
			}

			bool operator!=(const const_iterator& other) const noexcept {
				return !(*this == other);
			}

			const Node* m_node;
			const Node* m_prev;
		};

		Lock_free_list() {
			Node::Tag null_tag{};

			m_head.store(null_tag, std::memory_order_relaxed);
			m_tail.store(null_tag, std::memory_order_relaxed);
		}

		~Lock_free_list() {
			clear();
		}

		/* Add a node to the front */
		void push_front(Node* node) {
			assert(node != nullptr);

			typename Node::Tag null_ptr{};

			node->init();

			for (;;) {
				auto old_head = m_head.load(std::memory_order_acquire);

				/* Setup new node's pointers */
				node->m_next.store(typename Node::Tag{ old_head.m_ptr, 0 }, std::memory_order_relaxed);

				/* Try to set as new head with incremented version */
				typename Node::Tag new_head{ node, old_head.version() + 1 };

				if (m_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed)) {
					if (old_head != nullptr) {
						/* Update old head's prev pointer */
						auto old_prev = ((Node*)old_head)->m_prev.load(std::memory_order_acquire);
						typename Node::Tag new_prev{ node, old_prev.version() + 1 };

						((Node*)old_head)->m_prev.store(new_prev, std::memory_order_release);
					}
					else {
						// Empty list case - update tail
						m_tail.store(new_head, std::memory_order_release);
					}
					return;
				}
			}
		}

		/* Remove a specific node */
		void remove(Node* node) {
			for (;;) {
				/* Load both links with their versions */
				auto prev = node->m_prev.load(std::memory_order_acquire);
				auto next = node->m_next.load(std::memory_order_acquire);

				Node* prev_ptr = prev;
				Node* next_ptr = next;

				if (prev_ptr != nullptr) {
					/* Read prev's current next pointer and version */
					auto expected = ((Node*)prev)->m_next.load(std::memory_order_acquire);

					if (expected != node) {
						/* Node already removed or list changed */
						continue;
					}

					/* Prepare new tagged pointer with incremented version */
					Node::Tag new_tag{ next_ptr, expected.version() + 1 };

					/* Try to update with new version */
					if (((Node*)prev)->m_next.compare_exchange_strong(expected, new_tag, std::memory_order_release, std::memory_order_relaxed)) {
						/* Successfully unlinked from prev side */
						if (next != nullptr) {
							auto next_expected = ((Node*)next)->m_prev.load(std::memory_order_acquire);
							/* Increment version */
							Node::Tag next_new{ prev_ptr, next_expected.version() + 1 };

							((Node*)next)->m_prev.compare_exchange_strong(next_expected, next_new, std::memory_order_release);
						}
						return;
					}
				}
				else {
					/* Handle head case */
					auto expected = m_head.load(std::memory_order_acquire);

					if (expected != node) {
						continue;
					}

					Node::Tag new_tag{ next_ptr, expected.version() + 1 };

					if (m_head.compare_exchange_strong(expected, new_tag, std::memory_order_release, std::memory_order_relaxed)) {
						if (next_ptr != nullptr) {
							auto next_expected = ((Node*)next)->m_prev.load(std::memory_order_acquire);
							Node::Tag next_new{ nullptr, next_expected.version() + 1 };

							((Node*)next)->m_prev.compare_exchange_strong(next_expected, next_new, std::memory_order_release);
						}
						return;
					}
				}
			}
		}

		/* Add a node to the back */
		void push_back(Node* node) {
			assert(node != nullptr);

			node->init();

			typename Node::Tag null_tag{};

			for (;;) {
				auto old_tail = m_tail.load(std::memory_order_acquire);

				if (old_tail == nullptr) {
					/* Empty list case - try to set both head and tail */
					auto old_head = m_head.load(std::memory_order_acquire);

					if (old_head == nullptr) {
						typename Node::Tag new_tag{ node, old_head.version() + 1 };

						if (m_head.compare_exchange_weak(old_head, new_tag, std::memory_order_release, std::memory_order_relaxed)) {
							m_tail.store(new_tag, std::memory_order_release);
							return;
						}
					}
					continue;
				}

				/* Setup new node's prev pointer */
				node->m_prev.store(typename Node::Tag{ old_tail.m_ptr, 0 }, std::memory_order_relaxed);

				/* Try to update old tail's next pointer */
				auto old_next = ((Node*)old_tail)->m_next.load(std::memory_order_acquire);

				/* Tail was incorrect */
				if (old_next != nullptr) {
					continue;
				}

				/* Prepare new tagged pointer with incremented version */
				typename Node::Tag new_next{ node, old_next.version() + 1 };

				if (((Node*)old_tail)->m_next.compare_exchange_weak(old_next, new_next, std::memory_order_release, std::memory_order_relaxed)) {
					/* Update tail pointer */
					typename Node::Tag new_tail{ node, old_tail.version() + 1 };

					m_tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release, std::memory_order_relaxed);
					return;
				}
			}
		}

		/* Insert a node after a specific node */
		bool insert_after(Node* node, Node* new_node) {
			assert(node != nullptr);
			assert(new_node != nullptr);

			typename Node::Tag null_ptr{};

			new_node->init();

			for (;;) {
				auto next_tagged = node->m_next.load(std::memory_order_acquire);

				new_node->m_prev.store(typename Node::Tag{ node, 0 }, std::memory_order_relaxed);
				new_node->m_next.store(typename Node::Tag{ next_tagged.m_ptr, 0 }, std::memory_order_relaxed);

				typename Node::Tag new_next{ new_node, next_tagged.version() + 1 };

				if (node->m_next.compare_exchange_weak(next_tagged, new_next, std::memory_order_release, std::memory_order_relaxed)) {
					if (next_tagged != nullptr) {
						auto next_prev = ((Node*)next_tagged)->m_prev.load(std::memory_order_acquire);
						typename Node::Tag new_prev{ new_node, next_prev.version() + 1 };

						((Node*)next_tagged)->m_prev.store(new_prev, std::memory_order_release);
					}
					else {
						/* New node becomes the tail */
						auto old_tail = m_tail.load(std::memory_order_acquire);

						if (old_tail == node) {
							typename Node::Tag new_tail{ new_node, old_tail.version() + 1 };

							m_tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release, std::memory_order_relaxed);
						}
					}
					return true;
				}
			}
		}

		/* Find a node with a specific value */
		template<typename Predicate>
		Node* find_if(Predicate pred) {
			for (;;) {
				auto current = m_head.load(std::memory_order_acquire);

				while (current != nullptr) {
					/* Check if current node matches predicate */
					if (pred(static_cast<T*>((Node*)current))) {
						/* Verify node is still in list by checking its links */
						auto next = ((Node*)current)->m_next.load(std::memory_order_acquire);
						auto prev = ((Node*)current)->m_prev.load(std::memory_order_acquire);

						/* If node's next->prev points back to node, it's still valid */
						if (next != nullptr) {
							if (((Node*)next)->m_prev.load(std::memory_order_acquire) != current) {
								/* Node was removed, restart search */
								break;
							}
						}
						else if (m_tail.load(std::memory_order_acquire) != current) {
							/* Node was tail but no longer is */
							break;
						}

						/* If node's prev->next points to node, it's still valid */
						if (prev != nullptr) {
							if (((Node*)prev)->m_next.load(std::memory_order_acquire) != current) {
								/* Node was removed, restart search */
								break;
							}
						}
						else if (m_head.load(std::memory_order_acquire) != current) {
							/* Node was head but no longer is */
							break;
						}

						return current;
					}
					current = ((Node*)current)->m_next.load(std::memory_order_acquire);
				}

				/* If we completed iteration without finding a match, return nullptr */
				if (current == nullptr) {
					return nullptr;
				}

				/* If we broke out of the loop due to concurrent modification, retry */
			}
		}

		/* Convenience method to find by value */
		/*
		Node* find(const typename T::value_type& value) {
			return find_if([&value](const T* node) {
				return node->m_value == value;
				});
		}
		*/
		/* Clear the list */
		void clear() {
			Node::Tag null_tag{};

			/* The containing node should be deleted by the list owne*/
			m_head.store(null_tag, std::memory_order_relaxed);
			m_tail.store(null_tag, std::memory_order_relaxed);
		}

		iterator begin() noexcept {
			return iterator(m_head.load(std::memory_order_acquire), nullptr);
		}

		const_iterator begin() const noexcept {
			return const_iterator(m_head.load(std::memory_order_acquire), nullptr);
		}

		const_iterator cbegin() const noexcept {
			return const_iterator(m_head.load(std::memory_order_acquire), nullptr);
		}

		iterator end() noexcept {
			return iterator(nullptr, m_tail.load(std::memory_order_acquire));
		}

		const_iterator end() const noexcept {
			return const_iterator(nullptr, m_tail.load(std::memory_order_acquire));
		}

		const_iterator cend() const noexcept {
			return const_iterator(nullptr, m_tail.load(std::memory_order_acquire));
		}

		/* Print the list */
		void print() {
			auto current = m_head.load(std::memory_order_acquire);

			while (current != nullptr) {
				std::cout << static_cast<T*>(current)->m_value << " ";
				current = ((Node*)current)->m_next.load(std::memory_order_acquire);
			}

			std::cout << std::endl;
		}

		std::atomic<Node::Tag> m_head{};
		std::atomic<Node::Tag> m_tail{};
	};


} // namespace ut// ---------------------------
// Fake UI Object
// ---------------------------

struct UIObject
{
	uint64_t id;
	std::atomic<uint64_t> mutation_counter{ 0 };
	std::atomic<bool> alive{ true };

	UIObject(uint64_t i) : id(i) {}

	void mutate()
	{
		mutation_counter.fetch_add(1, std::memory_order_relaxed);
	}
};

// ---------------------------
// Plug Your Queue Here
// ---------------------------

// Example:
// using TestQueue = HPQueue<UIObject*, 1024>;


// ---------------------------
// Stress Parameters
// ---------------------------

static constexpr int OBJECT_COUNT = 32;
static constexpr int THREAD_COUNT = 16;
static constexpr int TEST_SECONDS = 20;


void startrecfunc(TRACK* track) {
	auto _appState = track->_appState;
	auto& recorder = _DATA->recorder;
	recorder.Record(track, 0);
}


long Recorder::Record(TRACK* track, int) {
	const auto _appState = track->_appState;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 0;
	auto tindex = track->index;
	const int32_t sr = _STATE->sr;
	gap = (size_t)_STATE->sr * (7 * 60) + 3;
	_track = track;
	offset = 0;
	fileBufferSize.store(0);

	message = track->name;
	message.append(" Microphone Recording ");

	{
		std::lock_guard lk(mutex);   // RecorderView::render may still be reading
		for (int i = 0; i < MAX_CHANNELS; i++) {
			filebuffer[i].clear();
		}
	}

	while (auto blockOpt = queue.try_pop()) {
		auto block = blockOpt.value();
		if (block == nullptr)continue;
		else _STATE->pool.release(block);
	}

	auto l_info = _DATA->views.infopanel;
	l_info->dec_offset = 0;
	l_info->bytes = 0;
	l_info->progress = -1;
	l_info->text.store(track->name);
	l_info->text2.store(" RECORDING");
	l_info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);


	std::vector<TRACK*> silencedTracks;
	bool startedPlayer = false;
	const bool isStandalone = !_DATA->isRunningAsPlugin;
	if (isStandalone) {
		const bool pausePlayback = _DATA->micPausePlayback;
		if (pausePlayback) {
			for (auto t : _DATA->tracks) {
				if (t && _STATE->params[t->index][POWERTRACK] == 1.0)
					silencedTracks.push_back(t);
			}
		}
		if (!_STATE->player.isPlaying()) {
			for (auto t : silencedTracks)
				t->disabled = true;
			if (_STATE->player.play() != 0) {
				for (auto t : silencedTracks)
					t->disabled = false;
				return 0;
			}
			startedPlayer = true;
		} else if (!silencedTracks.empty()) {
			for (auto t : silencedTracks)
				t->gainTask.setTarget(0, -120);
			_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
			auto token = _STATE->waitNotify.begin_wait();
			_DATA->toAudioThreadQueue.try_push([_STATE, silencedTracks, token] mutable {
				for (auto t : silencedTracks)
					t->disabled = true;
				_STATE->waitNotify.complete(token);
			});
			_STATE->waitNotify.wait_for_signal(token, tsl::gainTaskFadeMs);
		}
	}

	_STATE->params[track->index][MICROPHONEButton].store(1.0);
	if (tindex == _STATE->active_track.load() && GASMAIN == SPACE_WAVEFORM)
		_STATE->parameters[MICROPHONEButton].view->redraw();

	_DATA->views.recorderView->redraw();
	_DATA->views.recorderView->addCB();

	slot = _STATE->waitNotify.acquire_slot();
	_isRecording = true;
	std::string m1(message);
	m1.append("started.");
	showToast(_STATE, m1.c_str());
	while (_isRecording.load(std::memory_order_acquire)) {
		_STATE->waitNotify.wait_for_signal();
		if (_STATE->destroyRequested.load(std::memory_order_acquire))return 0;
		while (auto blockOpt = queue.try_pop()) {
			auto block = blockOpt.value();
			if (block == nullptr)continue;
			auto nFrames = block->frames;
			auto channels = block->channels;
			float peak = 0.0001f;
			auto buf = block->data;
			if (offset == 0) {
				float sum = 0;
				for (int32_t i = 0; i < nFrames * channels; i++) {
					float smpl = (float)buf[i] * CONVMYFLT;
					sum += (smpl * smpl);
				}
				float rms = sqrt(sum / (float)(nFrames * channels));
				if (rms < 0.001) {
					_STATE->peak[0] = _STATE->peak[1] = peak;
					_STATE->pool.release(block);
					continue;
				}
			}
			_DATA->views.infopanel->bytes =
				filebuffer[0].size() * channels * sizeof(recSampleFormat);

			std::lock_guard lk(mutex);
			try {
				for (int i = 0; i < _STATE->channels; i++, offset < gap)
					filebuffer[i].reserve(offset + nFrames);
			}
			catch (std::bad_alloc& e) {
				_STATE->pool.release(block);
				message.append("Out of Memory.");
				showToast(_appState, message.c_str());
				_isRecording.store(false);
				break;
			}

			int processed = 0;
			float peaks[MAX_CHANNELS]{};
			for (; processed < nFrames; processed++, offset++) {
				for (int chan = 0; chan < channels; chan++) {
					auto smpl = block->data.at(processed * channels + chan);
					filebuffer[chan].push_back(smpl);
					auto tmp = (float)std::abs((float)smpl * CONVMYFLT);
					peaks[chan] = std::max(tmp, peaks[chan]);
				}
				if (offset >= gap) {
					message.append("7 Minutes Max reached.");
					_isRecording.store(false);
					showToast(_STATE, message.data());
					break;
				}
			}
			_STATE->peak[0] = peaks[0]; _STATE->peak[1] = peaks[1];
			fileBufferSize.store(filebuffer[0].size());
			_DATA->views.infopanel->dec_offset.store(offset);
			_STATE->pool.release(block);
			// Process recItem.data of size recItem.size
		}


	}
	_STATE->params[track->index][MICROPHONEButton].store(0.0);
	if (tindex == _STATE->active_track.load() && GASMAIN == SPACE_WAVEFORM)
		_STATE->parameters[MICROPHONEButton].view->redraw();
	_DATA->views.infopanel->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);


	_DATA->views.recorderView->deldraw();
	_DATA->views.recorderView->delCB();
	fileBufferSize.store(0);
	l_info->dec_offset = 0;
	//QDEL(_DATA->queue_draw, _DATA->views.micbutton);


	if (!silencedTracks.empty()) {
		_DATA->toAudioThreadQueue.try_push([_STATE, silencedTracks] mutable {
			for (auto t : silencedTracks)
				t->disabled = false;
		});
		for (auto t : silencedTracks)
			t->gainTask.setTarget(-120, 0);
	}
	if (startedPlayer)
		_STATE->player.stop();

	if (offset == 0)
		return 0;
	else if (offset > gap)
		offset = gap;

	std::shared_ptr<tsl::Recording> rec;
	{
		/* The Recording constructor MOVES these buffers out, and
		   RecorderView::render may still be reading them for one more frame
		   (deldraw() above only queues the removal). */
		std::lock_guard lk(mutex);
		rec = std::make_shared<tsl::Recording>(_STATE, filebuffer, _STATE->channels, "Microphone Recording");
	}
	/* Waveform first -- see the Android path above; loadAudio ends in
	   pushSwap() and anything after it waits out that copy. */
	track->waveform->setup(rec);
	auto e = track->loadAudio(rec);
	if (e.poolHandle < tsl::INVALID_POOL_HANDLE)
		_DATA->snapShot.addEvent(e);

	offset = 0;

	return rec->off;
}

#endif