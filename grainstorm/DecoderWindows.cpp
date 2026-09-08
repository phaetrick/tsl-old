
// Created by pr on 15.11.17.


#include "logger.h"
#include <utility>
#include "DecoderWindows.h"
#include "DecoderView.h"
#include "infopanel.h"
#include "grainstorm.h"
#include "track.h"
#include "waveform.h"
#include "audio/Recording.h"
#include <cstddef>
#include "app.h"
#include "WindowsMediaDecoder.h"
#include <filesystem>
#include <algorithm>
#include <string>


int handleFrame(tsl::Decdata& decData, void* buf, uint64_t processed) {
	if (processed == 0)
		return 0;
	int channels = decData.channels;
	auto _appState = decData._appState;
	auto offset = decData.offset.load();
	auto info = _DATA->views.infopanel;
	uint64_t frame = 0;
	auto buf2 = static_cast<int16_t*>(buf);
	{
		/* One lock over resize + fill + the offset store, matching the Android
		   backend. The fill used to sit outside it: DecoderView::render could
		   not lose the pointer that way (only the resize reallocates, and that
		   was locked) but it could read half-written frames, and it is a data
		   race on decData.buffer either way.

		   Publishing decData.offset inside the lock also keeps the invariant
		   the reader relies on -- offset is never ahead of what is written. */
		std::lock_guard lk(decData.mtx);
		for (int i = 0; i < channels; i++) {
			try { decData.buffer[i].resize(offset + processed, 0); }
			catch (const std::bad_alloc&) {
				LOGE("Out of memory while reserving buffer for decoded audio.");
				return 1;
			}
		}

		for (; frame < processed && frame + offset < decData.off; frame++)
			for (int i = 0; i < channels; i++)
				decData.buffer[i][offset + frame] = buf2[frame * channels + i];

		decData.offset.store(offset + frame);
	}

	auto offsetAfter = offset + frame;
	info->progress.store(offsetAfter / (double)decData.off);
	info->dec_offset.store(static_cast<long>(offsetAfter));
	info->bytes.store(static_cast<long>(offsetAfter) * channels * sizeof(int16_t));
	return 0;
}



std::shared_ptr<tsl::Recording> dec(TRACK* track, std::string name, long gap) {
	auto _appState = track->_appState;
	if (name.empty()) {
		return nullptr;
	}
	auto decViewS = _DATA->views.decoderView;
	if (!decViewS) {
		showToast(_STATE, "Decoder View not initialized.");
		return nullptr;
	}
	auto decoderView = dynamic_cast<tsl::graphics::DecoderView*>(decViewS.get());
	auto& decData = decoderView->decData;
	auto channels = _STATE->channels;
	decData.channels = channels;
	decData.fileName = name;
	decData.sr = _STATE->sr;
	decData.off = gap;
	decData.offset.store(0);
	_DATA->decodingStop = false;

	auto info = _DATA->views.infopanel;

	info->text.store(track->name);
	info->text2.store(" DECODING");
	info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);

	decoderView->addCB();
	decoderView->addDraw();

	std::string message = track->name;
	message.append(" Audio Import ");



	// Update the lambda to match the expected signature of std::function<int(void*, uint64_t)>  
	auto partiallyBound = [](tsl::Decdata& recording, void* buf, uint64_t processed) -> int {
		return handleFrame(recording, buf, processed);
		};
	int ret = tsl::decoding::decodeWithMediaFoundation(decData, partiallyBound);


	_DATA->views.decoderView->delCB();
	_DATA->views.decoderView->deldraw();

	info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);


	if (ret == 1)
		message.append("Fail: Could not setup decoder.");
	else if (ret == 2)
		message.append("Fail: Unknown audio format.");
	else if (!decData.buffer[0].empty()) {
		std::shared_ptr<tsl::Recording> recording;
		{
			/* The Recording constructor MOVES these buffers out, and
			   DecoderView::render may still be reading them for one more frame
			   -- deldraw() above only queues the removal now. */
			std::lock_guard lk(decData.mtx);
			recording = std::make_shared<tsl::Recording>(_STATE, decData.buffer, _STATE->channels, name);
		}
		recording->sr = _STATE->sr;
		if (recording->off >= gap)
			message.append("(7 Minutes Max Reached.) ");
		message.append("Finished.");
		recording->statusMessage = std::move(message);
		return  recording;
	}
	else
		message.append("No samples decoded.");
	if (ret == 3)message.append(" (Out of memory.)");
	showToast(_STATE, message.c_str());
	{
		std::lock_guard lk(decData.mtx);   // DecoderView::render may still be reading
		for (auto& buf : decData.buffer) {
			buf.clear();
		}
	}
	return nullptr;
}


#include <thread>
#include <chrono>

inline void usleep(long long usec) {
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

int32_t filebrowsercallback(tsl::AppState* _appState, const std::string& filename1) {
	const std::string& _filename = filename1;
	if (_filename.empty())
		return -1;

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	int32_t s;
	if ((s = _STATE->WorkerQueue.add_taskInt([_STATE, track, _filename]() {
		while (!_STATE->initdone.load())usleep(10 * 1000);
		auto rec = dec(track, _filename, _STATE->sr * 420);
		if (rec && rec->off > 0) {
			/*
			auto slot = _STATE->waitNotify.acquire_slot();
			track->gainTask.setTargetAutoWait(tsl::gaintask::GainDown);
			_STATE->waitNotify.wait_for_signal(tsl::gainTaskFadeMs);
			track->gainTask.setTargetAutoWait(tsl::gaintask::GainUp);
			*/
			// No one waits on this task; the old wake_thread here only poisoned the slot.
			_DATA->snapShot.add_task([_STATE, track, rec] mutable {
				auto e = track->loadAudio(rec);
				if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
					_DATA->snapShot.addEvent(e);
				}
				/* Reported here, not when dec() returns: the import is not
				   finished until loadAudio has archived whatever the track was
				   holding, which is the slow part. dec() sets statusMessage on
				   success and deliberately does NOT toast it, so without this
				   the "Finished." message was never shown at all on this
				   platform. */
				if (!rec->statusMessage.empty())
					showToast(_STATE, rec->statusMessage.c_str());
				});
				track->waveform->setup(rec);

		}
		/* No failure toast here: dec() is the single reporter for its own
		   failures. Every reachable failure path in it toasts the specific
		   reason and THEN returns nullptr -- the decode error, out-of-memory,
		   "no samples decoded", "Decoder View not initialized" -- so `rec` is
		   null by the time we get here and the old
		   "else if (rec && !statusMessage.empty())" was unreachable. Its `else`
		   therefore always fired, adding a generic "Failed to decode audio."
		   on top of the specific message: two toasts per failure.
		   (dec()'s one silent return is name.empty(), which cannot happen --
		   the caller rejects an empty filename before getting here.) */

		})) > 0) {
		char text[100];
		snprintf(text, 100, "%s Import Audio Queued. Pos %d .", track->name, s + 1);
		showToast(_STATE, text);
	}
	return 0;
}