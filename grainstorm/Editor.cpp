//
// Created by pr on 19.03.24.
//

#include "Editor.h"
#include "grainstorm.h"
#include <skia.h>
#include <MidiLearning.h>
#include "waveform.h"
#include "BackView.h"
#include "infopanel.h"
#include <AnimatedSpinner.h>

#include "tools/PlatformPaths.h"

using namespace tsl::graphics;




struct EnvelopeSpec
{
	tsl::envelope::WINDOWTYPE type;
	double alpha; // 0.0 unless explicitly set
};

static constexpr EnvelopeSpec kEnvelopeOrder[] =
{
	{ tsl::envelope::Rectangular,   0.0 },
	{ tsl::envelope::SINE,          0.0 },
	{ tsl::envelope::SAW,           0.0 },
	{ tsl::envelope::FULL_SAW,      0.0 },
	{ tsl::envelope::wtTRAPEZOID,   0.0 },
	{ tsl::envelope::TRIANGLE_FULL, 0.0 },

	{ tsl::envelope::RECTPULS,      0.0 },
	{ tsl::envelope::FULL_RECTPULS, 0.0 },

	{ tsl::envelope::wtCOSINE,      0.0 },
	{ tsl::envelope::wtHANNING,     0.0 },

	{ tsl::envelope::SINE_FULL,     0.0 },
	{ tsl::envelope::COSINE_FULL,   0.0 },

	{ tsl::envelope::wtBLACKMAN,    0.0 },
	{ tsl::envelope::wtKAISER,      5.0 },

	{ tsl::envelope::wtHANNING,     0.25 },
	{ tsl::envelope::wtHANNING,     0.5  },
	{ tsl::envelope::wtHANNING,     0.75 },

	{ tsl::envelope::wtFLATTOP,     0.0 },
	{ tsl::envelope::wtFLATTOP2,    0.0 }
};


void scanTable(const float* table, float* output, int n, int s) {
	const uint64_t Capacity = 16384;
	const uint64_t MASK = Capacity - 1;

	// 1. Calculate the exact increment in fixed-point (32.32 format)
	// We shift by 32 to move the calculation into the "fractional" space
	uint64_t totalSteps = (uint64_t)n * Capacity - 1;
	uint64_t increment = (totalSteps << 32) / (s - 1);

	uint64_t phase = 0;

	for (int i = 0; i < s; ++i) {
		// Extract the integer part for the current index
		uint64_t fullIndex = (phase >> 32);
		size_t i0 = static_cast<size_t>(fullIndex & MASK);
		size_t i1 = static_cast<size_t>((i0 + 1) & MASK);

		// Extract the fractional part (lower 32 bits) 
		// and convert to a 0.0 - 1.0 float
		float frac = static_cast<float>(phase & 0xFFFFFFFF) / 4294967296.0f;

		// 2. Linear Interpolation: y = a + frac * (b - a)
		float a = table[i0];
		float b = table[i1];
		output[i] = a + frac * (b - a);

		phase += increment;
	}
}


void fadeinout(tsl::AppState* _appState, int32_t trackid, ParameterNum alg) {
	
	struct data_t {
		bool valid{};
		std::shared_ptr<tsl::Recording> rec{};
		TRACK* track;
		tsl::RecordingDiff diff;
	};

	auto track = _DATA->tracks[trackid];
	if (alg >= EDITORLOOP1 && alg <= EDITORLOOP4) {
		return;
	}
	else if (alg == EDITORUNDO) {
		auto rec = track->filebufferold.load();
		auto e = track->loadAudio(rec);
		track->waveform->setup(rec);

		return;
	}
	auto current = track->getAudioCopy();
	if (!current)return;
	auto off = current->off;

	if (alg == EDITORGRAINENV) {
		auto state = current->state.load();
		if (!state)return;
		auto offStart = state->off_start.load();
		int offStop = state->off_stop.load();
		int offset = state->offset.load();
		auto dist = offStop - offStart;
		if (dist <= 0) return;
		auto info = _DATA->views.infopanel;
		std::string s = track->name;
		s += " APPLYING ENVELOPE...";
		info->text.store(s.c_str());
		info->setRenderFunc(tsl::graphics::InfoPanel::RenderText);
		const auto outer_env1 = _DATA->eq[std::string(envelopesnames[(int)_STATE->params[track->index][ENVOUTER].load()])];
		const auto inner_env1 = _DATA->eq[std::string(envelopesnames[(int)_STATE->params[track->index][ENVINNER].load()])];
		int32_t    outer_cycles1 = (int)_STATE->params[track->index][AOUTERCYCLES].load();
		auto       outer_depth1 = _STATE->params[track->index][AOUTERDEPTH].load();
		const auto outer_env2 = _DATA->eq[std::string(envelopesnames[(int)_STATE->params[track->index][ENVOUTER2].load()])];
		const auto inner_env2 = _DATA->eq[std::string(envelopesnames[(int)_STATE->params[track->index][ENVINNER2].load()])];
		int32_t    outer_cycles2 = (int)_STATE->params[track->index][AOUTERCYCLES2].load();
		auto       outer_depth2 = _STATE->params[track->index][AOUTERDEPTH2].load();

		int32_t nsegs = _STATE->params[track->index][GRAINNSEGS].load();


		const auto   curve = (int)_STATE->params[track->index][GRAINCURVE].load();
		const auto   envamp2 = _STATE->params[track->index][GRAINENVINTERPOL].load();
		const auto   envamp1 = 1.f - envamp2;
		const double cycles1 = _STATE->params[track->index][AWINCYLCES].load();

		const double inv_dist = 1.0 / (dist - 1);
		const double dp_inner = cycles1 * inv_dist;
		const double dp3 = inv_dist;
		// 1. Move vector allocation OUTSIDE the hot loop (use a stack array if nsegs is small)
		MYFLOAT xx[32], yy[32]; // Example fixed size
		for (int32_t i = 0; i <= nsegs; i++) {
			xx[i] = _STATE->params[track->index][GRAINENVX0 + i].load();
			yy[i] = _STATE->params[track->index][GRAINENVY0 + i].load();
		}

		double phase_in = 0.0;
		double phase3 = 0.0;
		int32_t ss = offStart;
		while (ss < offStop && ss < off) {
			// Keep phase strictly [0..1]
			double p = phase_in;

			// Calculate outer phases
			auto po1 = p * outer_cycles1;
			while (po1 >= 1.0)po1 -= 1.0;

			auto po2 = p * outer_cycles2;
			while (po2 >= 1.0)po2 -= 1.0;

			// Generate windows
			auto i1 = tsl::envelope::WindowFunc(p, inner_env1.type, inner_env1.alpha, inner_env1.beta);
			auto i2 = tsl::envelope::WindowFunc(p, inner_env2.type, inner_env2.alpha, inner_env2.beta);
			auto o1 = tsl::envelope::WindowFunc(po1, outer_env1.type, outer_env1.alpha, outer_env1.beta);
			auto o2 = tsl::envelope::WindowFunc(po2, outer_env2.type, outer_env2.alpha, outer_env2.beta);

			// Apply outer modulation
			double sig1 = i1 * (1.0 - o1 * outer_depth1);
			double sig2 = i2 * (1.0 - o2 * outer_depth2);

			// v2 segment envelope
			double v2 = tsl::envelope::genEnvPhase<MYFLOAT>[curve](phase3, xx, yy, nsegs, true);

			// Final Gain
			double final_env = (envamp1 * sig1 + envamp2 * sig2) * v2;

			for (int channel = 0; channel < current->channels; channel++) {
				current->buffer[channel][ss] *= final_env;
			}

			// Increment and wrap
			phase_in += dp_inner;
			if (phase_in >= 1.0) phase_in -= 1.0;

			phase3 += inv_dist;
			if (phase3 > 1.0) phase3 = 1.0; // Cap segment phase at 1.0
			ss++;
		}
		info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
		current->numEdits++;
		current->lastEdit = alg;
		tsl::RecordingDiff diff;
		diff.type = tsl::RecordingDiff::Modification;
		diff.oldSize = diff.newSize = current->off;
		diff.point = offStart;
		diff.region = ss - offStart;
		auto data = std::make_shared<data_t>();
		data->track = track;
		data->rec = current;
		data->diff = diff;
		auto token = _STATE->waitNotify.begin_wait();
		_DATA->snapShot.add_task([=] mutable {
			auto e = track->loadAudio(data->rec, data->diff);
			if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
				_DATA->snapShot.addEvent(e);
				data->valid = true;;
			}
			_STATE->waitNotify.complete(token);
			});
		_STATE->waitNotify.wait_for_signal(token);
		track->waveform->setup(current);
	}
	else {
		tsl::RecordingDiff diff;
		if (current->edit(alg, diff)) {
			current->numEdits++;
			current->lastEdit = alg;
			auto afterSize = current->off;
			auto token = _STATE->waitNotify.begin_wait();
			if (afterSize > 0) {
				
				auto data = std::make_shared<data_t>();
				data->track = track;
				data->rec = current;
				data->diff = diff;

				
				_DATA->snapShot.add_task([=] mutable {
					auto e = track->loadAudio(data->rec, data->diff);
					if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
						_DATA->snapShot.addEvent(e);
						data->valid = true;;
					}
					_STATE->waitNotify.complete(token);
					});
				_STATE->waitNotify.wait_for_signal(token);
				track->waveform->setup(current);


			}
			else {
				_DATA->snapShot.add_task([=] mutable {
					std::shared_ptr<tsl::Recording> dummy{};
					auto e = track->loadAudio(dummy);
					track->waveform->setup(dummy);
					if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
						_DATA->snapShot.addEvent(e);
					}
					});
			}
		}
	}

}

#ifdef __ANDROID__
int32_t java_save_loop(JNIEnv* emv, jobject obj, jint fd, jlong trackid) {
	auto _STATE = __STATE;
	auto track = (TRACK*)trackid;
#else
int32_t save_loop(tsl::AppState * _appState, TRACK * track) {
#endif
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_acquire) == false) {
#ifdef __ANDROID__
		if (fd)close(fd);
#endif
		return -1;
	}

	std::string message = track->name;
	message.append(" Save Loop: ");
	const int chans = _STATE->channels;
	auto files = track->filebuffer.load();
	auto state = files ? files->state.load() : nullptr;
	auto off = state == nullptr ? 0 : files->off;
	if (off == 0) {
		message.append("Empty Track.");
		showToast(_STATE, message.c_str());
#ifdef __ANDROID__
		if (fd)close(fd);
#endif
		return -1;
	}

	int posStart = state->off_start.load(), posStop = state->off_stop.load();
	if (posStart > off)posStart = off;
	if (posStop > off)posStop = off;
	if (posStop - posStart <= 0) {
		message.append("Empty Loop.");
		showToast(_STATE, message.c_str());
#ifdef __ANDROID__
		if (fd)close(fd);
#endif
		return -1;
	}
#ifdef __ANDROID__
	FILE* f = fdopen(fd, "wb");
#else
	auto folder = tsl::app::getStoragePath("Grainstorm/Recordings");
	if (folder.empty()) {
		showToast(_STATE, "Failed to get storage path for recordings.");
		return -1;
	}
	auto recName = tsl::generateTimestampedName("grainstorm-track");
	std::string extension = _STATE->getAudioFormat(); // e.g., ".flac" or ".raw"
	std::string fullPath = folder + "/" + recName + extension;
	FILE* f = std::fopen(fullPath.c_str(), "wb");
#endif
	if (!f) {
		showToast(_STATE, "Failed to open file for writing.");
#ifdef __ANDROID__
		if (fd)close(fd);
#endif
		return -1;
	}
	auto w = tsl::Player::setupRecording(_STATE, f, "Track Recording ", _STATE->channels, tsl::AudioFileWrapper::SAMPLE_INT16);
	if (w != nullptr) {
		const int bufsize = 1024;
		int count = 0;
		std::vector<int16_t> writebuf;
		writebuf.resize(bufsize * chans, 0);
		auto buf = (int16_t*)writebuf.data();
		auto info = _DATA->views.infopanel;

		info->progress = 0.0;
		info->dec_offset = 0.0;
		info->bytes = 0;
		info->text.store(track->name);
		info->text2.store(" RENDERING");
		info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
		std::atomic_bool abort = false;
		tsl::graphics::BackView bv(_STATE, [&abort]() {abort = true; });

		for (int smpl = posStart; smpl < posStop; smpl++) {
			try
			{
				buf[count * chans] = files->buffer[0].at(smpl);
				if (chans == 2)
					buf[count * chans + 1] = files->buffer[1].at(smpl);

			}
			catch (const std::exception& e)
			{
				message.append("Write Error.");
				info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
				showToast(_STATE, message.c_str());
				return -1;
			}
			if (++count >= bufsize) {
				if (abort.load())break;
				count = 0;
				if (w->wrapper.write(buf, sizeof(int16_t), writebuf.size()) != writebuf.size()) {
					message.append("Write Error.");
					info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
					showToast(_STATE, message.c_str());
					return -1;
				};
				info->dec_offset.fetch_add(static_cast<long>(bufsize));
				info->progress = smpl / (double)(posStop - posStart);
				info->bytes = w->wrapper.tell();
			}
		}

		if (count > 0 && w->wrapper.write(buf, sizeof(int16_t), count * _STATE->channels) != count * _STATE->channels) {
			message.append("Write Error.");
			info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
			showToast(_STATE, message.c_str());
			return -1;
		}
		w->wrapper.close();
		w->finished.store(true, std::memory_order_release);
		info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
#ifndef __ANDROID__
		if (_STATE->askName) {
			tsl::graphics::AlphaPopUp kbd(_STATE);
			kbd.setTitle("Save As:");
			kbd.setToken(_STATE->waitNotify.begin_wait());
			tsl::graphics::AlphaPopUp::InputResult result{};
			kbd.onCompleteCallback = [&result](const tsl::graphics::AlphaPopUp::InputResult& r) {
				if (r.confirmed) {
					result = r;
					return 1;
				}
				return 0;
				};
			kbd.setText(recName);
			kbd.init();
			kbd.addDraw();
			kbd.addCB();
			_STATE->waitNotify.wait_for_signal(kbd.token());
			kbd.deldraw();
			kbd.delCB();

			if (result.confirmed) {
				std::string trimmed = tsl::trimToValidFilename(result.text); // trim whitespace if needed
				if (!trimmed.empty()) {
					std::string newPath = folder + "/" + trimmed + extension;


					if (std::rename(fullPath.c_str(), newPath.c_str()) == 0) {
						std::string message = "Recording saved as: " + newPath;
						showToast(_STATE, message.c_str());
					}
					else {
						showToast(_STATE, "Rename failed.");
					}
				}
				else {
					std::remove(fullPath.c_str());
					showToast(_STATE, "Recording name empty. File deleted.");
				}
			}
			else {
				std::remove(fullPath.c_str());
				showToast(_STATE, "Recording cancelled. File deleted.");
			}
		}
		else {
			std::string message = "Recording saved as: " + fullPath;
			showToast(_STATE, message.c_str());
		}
#endif
	}
	else {
#ifdef __ANDROID__
		if (f)fclose(f);
#endif
		showToast(_STATE, "Failed to start recording.");
		return -1;
	}
	return 0;
}


int ListView::cb(float xpos, float ypos, int action, int) {
	switch (action) {
	case tsl::graphics::ACTION_DOWN: {
		downX = xpos;
		downY = ypos;
		if (ypos < contentHeight && ypos >0) {
			auto act = (int)(ypos / rowHeight);
			if (_STATE->midilearning.load()) {
				_STATE->UiTasksQueue.add_task([this, id = min_ + act] {MidiLearning::wait(_STATE, _STATE->active_track.load(), id); });
				hot.store(-1);
			}
			else {
				timer.reset();
				hot.store(act);
			}
		}
		else(hot.store(-1));
		redraw();
		break;
	}
	case tsl::graphics::ACTION_UP: {
		auto h = hot.load();
		if (h >= 0 && h < max_ - min_) {
			auto e = tsl::parameters::Event::createEvent(_STATE->active_track.load(), tsl::parameters::Eventtype::paramUpdate, min_ + h);
			e.applyFromExt(_STATE, tsl::parameters::FromUi);
		}
		hot.store(-1);
		redraw();

		break;
	}
	case tsl::graphics::ACTION_MOVE: {
		// Only cancel once the finger leaves the touch slop; sub-slop jitter
		// is forwarded by FloatingView on every move and must not kill the tap
		if (spacing(xpos, downX, ypos, downY) > _STATE->touchSlop()) {
			hot.store(-1);
			redraw();
		}
		break;
	}
	default:break;
	}

	return 0;
};

void ListView::renderContent(void* canv) {
	auto c = static_cast<SkCanvas*>(canv);
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	SkPaint paint;
	paint.setAntiAlias(true);
	const auto ml = _STATE->midilearning.load();
	paint.setColor(ml ? skcol::orange : skcol::fg);
	const int ht = hot.load();
	for (int i = 0; i < max_ - min_; i++) {
		if (ht == i) {
			perm = true;
			auto elapsed = timer.elapsed();
			SkPaint paint;
			paint.setColor(skcol::blue_transparent);
			/*
			paint.setColor(SkColorSetA(skcol::blue_transparent,
				elapsed < .5 ? 200.f * elapsed : 100.f));
			*/c->drawRect(SkRect::MakeXYWH(-borderSize, i * rowHeight, contentWidth + 2 * borderSize, rowHeight), paint);
			paint.setColor(ml ? skcol::orange : skcol::fg);

		}
		c->drawSimpleText(_STATE->parameters[min_ + i].name, strlen(_STATE->parameters[min_ + i].name), SkTextEncoding::kUTF8, 0, i * rowHeight + ypos, font, paint);
	}
	if (ht == -1)
		perm = false;
}

void ListView::computeContent(int maxWidth, int maxHeight) {
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	SkRect bounds{};
	float maxW = font.measureText("XXXXXXXXXXXX", 12, SkTextEncoding::kUTF8, &bounds);
	for (int i = 0; i < max_ - min_; i++) {
		font.measureText(_STATE->parameters[min_ + i].name, strlen(_STATE->parameters[min_ + i].name), SkTextEncoding::kUTF8, &bounds);
		maxW = std::max(maxW, bounds.width());
		ypos = SkFloatToScalar(rowHeight * .5f - bounds.centerY());
	}

	contentWidth = maxW;
	contentHeight = (max_ - min_) * rowHeight;
}

ListView::ListView(tsl::AppState * appState, int min, int max) : FloatingView(appState) {
	setUp(min, max);
}

void ListView::setUp(int min, int max) {
	min_ = min;
	max_ = max + 1;
	hot.store(-1);
	FloatingView::init();
}

void ListView::show(tsl::AppState * appState, tsl::AtomicSharedPtr<ListView>*ptr, int min, int max) {
	auto view = ptr->load();

	if (view != nullptr) {
		view->delDrawCB();
		view->setUp(min, max);
	}
	else {
		view = std::make_shared<ListView>(appState, min, max);
		*ptr = view;
	}
	view->addDraw();
	view->addCB();
}

void ListView::midiLearningEvent(tsl::AtomicSharedPtr<ListView>&ptr) {
	auto le = ptr.load();
	if (le != nullptr) {
		auto _appState = le->_appState;
		_STATE->toUiThreadQueue.try_push([_STATE, leptr = std::move(le)] {
			if (leptr->visible_)
				leptr->redraw();
			});
	}
}




