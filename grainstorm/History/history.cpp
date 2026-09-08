#include "history.h"
#include "app.h"
#include "grainstorm.h"


using namespace tsl::parameters;

static void showErrorToast(tsl::AppState* _appState, Event& e, const char* error) {
	char buffer[200]{};
	int pos = 0;
	pos = snprintf(buffer, 200 - pos, error);
	e.toString(_appState, pos, buffer, 200, false);
	if (pos > 0) {
		showToast(_STATE, buffer);
		LOGE("%s", buffer);
	}
}

void Snapshot::addRecording(int trackIndex, std::shared_ptr<tsl::Recording> rec) {
	std::lock_guard lk(*this);

	if (rec && rec->off > 0) {
		names_[trackIndex] = rec->fileName;
		addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, rec->off, EventSubtype::off, 0, 0));
		auto state = rec->state.load();

		if (state) {
			//addEvent(Event::createEvent(trackIndex, Eventtype::Recordin g, rec->poolHandle, state->off_start, EventSubtype::offStart, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, state->off_start, EventSubtype::offStart, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, state->off_stop, EventSubtype::offStop, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, state->offset, EventSubtype::offset, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, state->bounceType, EventSubtype::bounceType, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, state->playbackDir, EventSubtype::playbackDir, 0, 0));
			addEvent(Event::createEvent(trackIndex, Eventtype::Recording, rec->poolHandle, std::bit_cast<double>(state->waveformState.load()), EventSubtype::waveformView, 0, 0));
		}
	}
	else {
		names_[trackIndex].clear();
		for (auto it = events_.begin(); it != events_.end();) {
			if (it->trackIndex == trackIndex && it->eventType == Eventtype::Recording)
				it = events_.erase(it);
			else
				++it;
		}
	}
#if defined PLUGIN_MODE
	// RequestHistory just raises an atomic flag consumed by the plugin's OnIdle,
	// so it is safe to call from here and works while the editor is closed.
	if (_STATE->RequestHistory)
		_STATE->RequestHistory();
#endif
}


void Snapshot::addEvent(const Event& e) {
	std::lock_guard lk(*this);

	auto addToHistory = [this](const Event& e) {
		auto& h = history[e.trackIndex];
		auto& r = redoEvents[e.trackIndex];
		auto& pos = undoredoPos[e.trackIndex];

		auto shouldSkipUndo = [this](const Event& e) {
			if (historyTimer.elapsedReplace() >= 0.3) return false;
			bool bothNoGroup = (lastEvent.groupId == 0 && e.groupId == 0);
			bool bothHaveGroup = (lastEvent.groupId != 0 && e.groupId != 0);
			if (bothNoGroup)   return lastEvent == e;
			// Skip only duplicates within the SAME group; an event starting a new
			// group must never be dropped, or grouped undo loses its first member.
			if (bothHaveGroup) return lastEvent.groupId == e.groupId && lastEvent == e;
			return false;
			};

		if (shouldSkipUndo(e)) return;

		lastEvent = e;

		r.resize(pos);
		r.push_back(e);
		r.back().setFlag(Event::History, false);
		r.back().setFlag(Event::FromDaw, false);
		if (r.back().flags & Event::NoInfo) r.back().flags &= ~Event::Info;
		else r.back().flags |= Event::Info;
		if (e.flags & Event::ToAudioThread || e.flags & Event::ToWorkerThread)
			redoEvents[e.trackIndex].back().value = e.getCurrentValue(_STATE);

		h.resize(pos);
		h.push_back(e);
		h.back().setFlag(Event::History, false);
		h.back().setFlag(Event::FromDaw, false);
		if (h.back().flags & Event::NoInfo) h.back().flags &= ~Event::Info;
		else h.back().flags |= Event::Info;

		++pos;
		bool doRedraw = hasRedos[e.trackIndex].exchange(false);
		bool doRedraw2 = !hasUndos[e.trackIndex].exchange(true);
		if (doRedraw || doRedraw2) {
			_STATE->toUiThreadQueue.try_push([_STATE = this->_appState, tindex = e.trackIndex] {
				if (_STATE->active_track.load() == tindex && _STATE->parameters[REDOBUTTON].view->visible_) {
					_STATE->parameters[REDOBUTTON].view->redraw();
					_STATE->parameters[UNDOBUTTON].view->redraw();

				}
				});
		}
		};

	// Helper: is this a Power event and is it in the "off" state?
	auto isPowerOff = [&]() -> bool {
		if (e.eventType != Eventtype::Power) return false;
		if (e.subType != EventSubtype::powerFx &&
			e.subType != EventSubtype::powerStereoFx &&
			e.subType != EventSubtype::powerGrainFx) return false;
		return std::bit_cast<PowerState>((double)e.value).pow == 0;
		};

	if (e.flags & Event::History && (
		(e.eventType == Eventtype::Recording && e.subType <= recordingChange)
		|| e.eventType == Eventtype::SpecialAction
		|| e.eventType == Eventtype::FxOrder
		|| e.eventType == Eventtype::TextEvent
		|| e.eventType == Eventtype::Rerender
		|| e.eventType == Eventtype::Preset
		|| (e.eventType == Eventtype::paramUpdate && (_STATE->parameters[e.paramIndex].flags & Param::NoValue))
		)) {
		if (!(e.flags & Event::FromDaw)) addToHistory(e);
		return;
	}

	auto initValue = e.getDefaultValue(_STATE);

	for (auto it = events_.begin(); it != events_.end(); ++it) {
		if (*it == e) {
			if (!(e.flags & Event::FromDaw) && e.flags & Event::History && it->value != e.value) {
				auto newEvent = *it;
				newEvent.flags = e.flags;
				newEvent.groupId = e.groupId;
				addToHistory(newEvent); // captures real pos in value ✓
			}
			// erase if default OR if power is off regardless of pos
			if (e.value == initValue || isPowerOff()) {
				events_.erase(it);
			}
			else {
				it->value = e.value;
			}
			return;
		}
	}

	// Don't append if power is off — treat as default
	if (e.value != initValue && !isPowerOff()) {
		events_.push_back(e);
		events_.back().setFlag(Event::FromDaw, false);
		if (!(e.flags & Event::FromDaw) && e.flags & Event::History)
			addToHistory(tsl::parameters::Event::createEvent(
				e.trackIndex, e.eventType, e.paramIndex,
				initValue, e.subType, e.groupId, e.flags));
	}
	/*
		int i = 1;
		for (auto& e : events_) {
			char buf[100]{};
			int pos = 0;
			pos = snprintf(buf + pos, 100 - pos, "%d: ", i++);
			e.toString(_STATE, pos, buf, 100, true);
			LOGE("%s", buf);
		}
		*/
}

bool Snapshot::undo(int tindex) {
	std::lock_guard lk(*this);

	auto& pos = undoredoPos[tindex];
	auto& h = history[tindex];
	if (pos == 0) {
		return false;
	}
	try {

		int end = pos - 1;
		int start = end;
		if (h.at(end).groupId != 0) {
			while (start > 0 &&
				h.at(start - 1).groupId == h.at(end).groupId)
				--start;
		}

		if (!_STATE->player.isPlaying()) {
			for (int i = start; i < end + 1; i++) {
				auto& ev = h.at(i);;
				if (ev.flags & Event::EventFlags::ToAudioThread) {
					showErrorToast(_STATE, ev, "Synth has to be running to undo: ");
					return false;
				}
			}
		}
		std::vector<Event> audioThreadEvents{};
		for (int i = start; i < pos; i++) {

			auto& ev = h.at(i);

			if (ev.flags & Event::EventFlags::History) {
				showErrorToast(_STATE, ev, "Err. History Flag in Undo: ");
				return false;
			}

			if ((ev.flags & Event::EventFlags::ToAudioThread)) {
				audioThreadEvents.push_back(ev);
			}
			else {
				redoEvents[tindex].at(i) = ev.apply(_STATE, tsl::parameters::FromHistory);
			}
		}

		if (!audioThreadEvents.empty()) {
			_DATA->toAudioThreadQueue.try_push([this, ev = std::move(audioThreadEvents)] () mutable {
				for (auto& e : ev) {

					e.apply(_STATE, tsl::parameters::None);
				}
				});
		}


		pos = start;

		bool doRedraw = false;
		if (pos == 0) {
			if (hasUndos[tindex].exchange(false));
			doRedraw = true;
		}
		if (!hasRedos[tindex].exchange(true))
			doRedraw = true;

		if (doRedraw) {
			_STATE->toUiThreadQueue.try_push([_STATE = this->_appState, tindex] {
				if (_STATE->active_track.load() == tindex && _STATE->parameters[REDOBUTTON].view->visible_) {
					_STATE->parameters[REDOBUTTON].view->redraw();
					_STATE->parameters[UNDOBUTTON].view->redraw();

				}
				});
		}

	}
	catch (std::exception& e) {
		showToast(_STATE, e.what());
		return false;
	}
	return true;
}

bool Snapshot::redo(int tindex) {
	std::lock_guard lk(*this);
	auto& pos = undoredoPos[tindex];
	auto& r = redoEvents[tindex];
	if (pos >= r.size())
		return false;

	try {
		if (!_STATE->player.isPlaying()) {
			auto tmpPos = pos;
			while (true)
			{
				auto& ev = r.at(tmpPos);
				if (ev.flags & Event::EventFlags::ToAudioThread) {
					showErrorToast(_STATE, ev, "Synth has to be running to redo: ");
					return false;
				}
				++tmpPos;
				if (tmpPos >= r.size()) break;
				if (ev.groupId != 0 && r.at(tmpPos).groupId == ev.groupId)
					continue;
				break;
			}
		}

		std::vector<Event> audioThreadEvents;

		while (true)
		{
			auto& ev = r.at(pos);

			if (ev.flags & Event::EventFlags::History) {
				ev.flags &= ~Event::EventFlags::History;
				showErrorToast(_STATE, ev, "Err. History Flag in Redo: ");
				//return false;
			}
			if ((ev.flags & Event::EventFlags::ToAudioThread)) {
				audioThreadEvents.push_back(ev);
			}
			else {
				ev.apply(_STATE, tsl::parameters::FromHistory);
			}
			++pos;
			if (pos >= r.size()) break;
			if (ev.groupId != 0 && r.at(pos).groupId == ev.groupId)
				continue;
			break;
		}

		if (!audioThreadEvents.empty()) {
			_DATA->toAudioThreadQueue.try_push([this, ev = std::move(audioThreadEvents)] mutable {
				for (auto& e : ev) {
					e.apply(_STATE, tsl::parameters::None);
				}
				});
		}

		bool doRedraw = false;
		if (pos >= r.size()) {
			if (hasRedos[tindex].exchange(false));
			doRedraw = true;
		}
		if (!hasUndos[tindex].exchange(true))
			doRedraw = true;

		if (doRedraw) {
			_STATE->toUiThreadQueue.try_push([_STATE = this->_appState, tindex] {
				if (_STATE->active_track.load() == tindex && _STATE->parameters[REDOBUTTON].view->visible_) {
					_STATE->parameters[REDOBUTTON].view->redraw();
					_STATE->parameters[UNDOBUTTON].view->redraw();

				}
				});
		}

	}
	catch (std::exception& e) {
		showToast(_STATE, e.what());
		return false;
	}
	return true;

}

int Snapshot::cb(float xpos, float ypos, int action, int) {
	switch (action) {
	case tsl::graphics::ACTION_DOWN: {
		auto current = currentTindex.load();
		auto& undos = hasUndos[current], & redos = hasRedos[current];
		if (ypos < rowHeight) {
			hot.store(std::min(3, (int)floor(xpos * 4 / contentWidth)));
		}
		else if (ypos < contentHeight) {
			auto which = (int)floor(xpos * 2 / contentWidth);
			if (which == 0 && !undos.load()) {
				hot.store(-1);
			}
			else if (which == 1 && !redos.load()) {
				hot.store(-1);
			}
			else
				hot.store(std::min(5, 4 + which));
		}
		else(hot.store(-1));
		redraw();
		break;
	}
	case tsl::graphics::ACTION_UP: {
		auto current = currentTindex.load();
		auto& undos = hasUndos[current], & redos = hasRedos[current];

		if (hot >= 0 && hot <= 3) {
			currentTindex.store(hot);

		}
		else if (hot == 4 && undos.load()) {
			add_task([this, current] {undo(current); });

		}
		else if (hot == 5 && redos.load()) {
			add_task([this, current] {redo(current); });
		}
		hot.store(-1);
		redraw();

		break;
	}
	case tsl::graphics::ACTION_MOVE: {
		hot.store(-1);
		break;
	}
	default:break;
	}

	return 0;
};
const char* ti[] = { "1", "2", "3", "4" };
const char* ur[] = { "UNDO", "REDO" };

void Snapshot::renderContent(void* canv) {
	auto c = static_cast<SkCanvas*>(canv);
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::fg);
	const int ht = hot.load(), tt = currentTindex.load();
	for (int i = 0; i < 4; i++) {
		if (ht == i || tt == i) {
			paint.setColor(skcol::blue_transparent);
			c->drawRect(SkRect::MakeXYWH(i * contentWidth / 4., 0, contentWidth / 4., rowHeight), paint);
			paint.setColor(skcol::fg);

		}
		c->drawSimpleText(ti[i], 1, SkTextEncoding::kUTF8, xopsTrackNumber[i], yposTrackNumber[i], font, paint);
	}
	if (ht == 4 || ht == 5) {
		paint.setColor(skcol::blue_transparent);
		c->drawRect(SkRect::MakeXYWH(ht == 4 ? 0 : contentWidth / 2., rowHeight + borderSize, contentWidth / 2., rowHeight), paint);
		paint.setColor(skcol::fg);
	}
	auto current = currentTindex.load();
	auto undos = hasUndos[current].load(), redos = hasRedos[current].load();

	SkPaint paintRedo(paint);

	if (!undos) paint.setColor(skcol::grey);
	c->drawSimpleText(ur[0], 4, SkTextEncoding::kUTF8, xposUndo, yposUndo, font, paint);


	if (!redos) paintRedo.setColor(skcol::grey);
	c->drawSimpleText(ur[1], 4, SkTextEncoding::kUTF8, xposRedo, yposRedo, font, paintRedo);

}

void Snapshot::computeContent(int maxWidth, int maxHeight) {
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	SkRect bounds{};
	float cw = 4 * rowHeight;
	font.measureText(ur[0], 4, SkTextEncoding::kUTF8, &bounds);
	cw = std::max(cw, 2 * bounds.width());
	font.measureText(ur[1], 4, SkTextEncoding::kUTF8, &bounds);
	cw = std::max(cw, 2 * bounds.width());
	cw += borderSize;
	contentWidth = cw;
	centerText(font, cw / 2.f, rowHeight, ur[0], xposUndo, yposUndo);
	centerText(font, cw / 2.f, rowHeight, ur[1], xposRedo, yposRedo);
	xposRedo += cw / 2.f;
	yposUndo += rowHeight + borderSize;
	yposRedo += rowHeight + borderSize;

	for (int i = 0; i < 4; i++) {
		centerText(font, cw / 4.f, rowHeight, ti[i], xopsTrackNumber[i], yposTrackNumber[i]);
		xopsTrackNumber[i] += cw / 4.f * i;
	}
	contentHeight = 2 * rowHeight + borderSize;
}

uint16_t Snapshot::nextGroupId() {
	uint16_t current = groupIdCounter.load(std::memory_order_relaxed);
	uint16_t next;
	do {
		next = (current % 65535) + 1; // gives 1..65535, skips 0
	} while (!groupIdCounter.compare_exchange_weak(current, next,
		std::memory_order_acq_rel, std::memory_order_relaxed));
	return next;
}