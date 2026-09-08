//
// Created by pr on 29.01.22.
//

#include "TrackSettings.h"
#include "view.h"
#include "grainstorm.h"
#include "track.h"
#include <SkFont.h>
#include "button.h"
#include <RecyclerView.h>
#include <Input.h>
#include <MidiSaver.h>
#include "colours.h"
#include "app.h"
#include "Editor.h"
#include <MidiLearning.h>
#include "degradation.h"
#include "Effects/gaintask.h"
#include "TrackSettings/fxOrder.h"
#include "aigen.h"

inline constexpr const char* loadNames[8] = {
	"LOAD1", "LOAD2", "LOAD3", "LOAD4", "LOAD5", "LOAD6", "LOAD7", "LOAD8"
};
inline constexpr const char* saveNames[8] = {
	"SAVE1", "SAVE2", "SAVE3", "SAVE4", "SAVE5", "SAVE6", "SAVE7", "SAVE8"
};
inline constexpr const char* titles[] = { "TRACK1 LOOP", "TRACK2 LOOP", "TRACK3 LOOP", "TRACK4 LOOP" };


using namespace tsl::graphics;


LoopPositions::LoopPositions(tsl::AppState* appState) : FloatingView(appState) {};

int LoopPositions::cb(float xpos, float ypos, int action, int) {
	switch (action) {
	case tsl::graphics::ACTION_DOWN: {
		downX = xpos;
		downY = ypos;
		if (ypos < contentHeight && ypos >0) {
			int col = (int)(xpos / columnWidth);
			int row = (int)(ypos / rowHeight);

			int setRows = (int)ceil(8.0 / columns);
			int set = row / setRows;
			int localRow = row % setRows;
			int idx = localRow * columns + col;

			if (set > 1 || idx >= 8 || col < 0 || col >= columns || row < 0) return 0;

			if (_STATE->midilearning.load()) {
				_STATE->UiTasksQueue.add_task([this, id = set == 0 ? LOOPLOAD0 + idx : LOOPSAVE0 + idx] {MidiLearning::wait(_STATE, _STATE->active_track.load(), id); });
				hot.store(-1);
			}
			else {
				timer.reset();
				hot.store(set * 8 + idx);
			}
		}
		else(hot.store(-1));
		redraw();
		break;
	}
	case tsl::graphics::ACTION_UP: {
		auto h = hot.load();
		if (h >= 0 && h < 16) {
			tsl::parameters::Event e;
			e.setup(_STATE, _STATE->active_track.load(), h >= 8 ? LOOPSAVE0 + h - 8 : LOOPLOAD0 + h);
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


	// use idx and set
	return 0;
}

void LoopPositions::renderContent(void* c) {
	auto canvas = static_cast<SkCanvas*>(c);
	SkPaint paint;
	paint.setAntiAlias(true);
	const auto ml = _STATE->midilearning.load();
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	auto h = hot.load();
	for (int set = 0; set < 2; set++) {
		int setRows = (int)ceil(8.0 / columns);
		for (int row = 0; row < setRows; row++) {
			for (int col = 0; col < columns; col++) {
				int idx = row * columns + col;
				if (idx >= 8) goto next_set;
				auto& data = set == 0 ? loadNames[idx] : saveNames[idx];
				int renderRow = set * setRows + row;
				auto idxH = set == 0 ? idx : idx + 8;
				if (h == idxH) {
					perm = true;
					/*
					auto elapsed = timer.elapsed();
					paint.setColor(SkColorSetA(skcol::blue_transparent,
						elapsed < .5 ? 200.f * elapsed : 100.f));
					*/
					paint.setColor(skcol::blue_transparent);

					canvas->drawRect(SkRect::MakeXYWH(col * columnWidth,
						renderRow * rowHeight,
						columnWidth, rowHeight), paint);
				}
				paint.setColor(ml ? skcol::orange : skcol::fg);
				canvas->drawSimpleText(data, strlen(data), SkTextEncoding::kUTF8,
					col * columnWidth + 2 * borderSize,
					renderRow * rowHeight + y, font, paint);

			}
		}
	next_set:;
	}
	if (h == -1)perm = false;


};
void LoopPositions::computeContent(int maxWidth, int maxHeight) {
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	SkRect bounds{};
	auto xxx = font.measureText("LOAD1", 5, SkTextEncoding::kUTF8, &bounds);
	auto w = bounds.width() + 4 * borderSize;
	columns = 3;
	while (w * columns > maxWidth)columns--;
	contentWidth = w * columns;
	columnWidth = contentWidth / columns;
	x = borderSize + columnWidth * 0.5 - bounds.centerX();
	y = rowHeight * 0.5 - bounds.centerY();
	rows = (int)(ceil(8. / columns) * 2);
	contentHeight = rows * rowHeight;
};

void LoopPositions::show(tsl::AppState* _appState, int32_t t) {
	auto tmp = _DATA->showPosWindows[t].load();
	if (tmp != nullptr) {
		tmp->deldraw();
		tmp->delCB();
	}
	else {
		tmp = std::make_shared<tsl::graphics::LoopPositions>(_appState);
		_DATA->showPosWindows[t].store(tmp);
		tmp->setTitle(titles[t]);
	}
	tmp->init();
	tmp->addDraw();
	tmp->addCB();
}

void LoopPositions::midiLearningEvent(tsl::AppState* _appState) {
	for (auto& showPosWindow : _DATA->showPosWindows) {
		auto tmp = showPosWindow.load();
		if (tmp != nullptr)
			_STATE->toUiThreadQueue.try_push([tmpPtr = std::move(tmp)] {
			if (tmpPtr->visible_) {
				tmpPtr->redraw();
			}
				});
	}
}

class InputRouting : public TextViewBase<std::string> {
public:
	InputRouting(tsl::AppState* appState) : TextViewBase<std::string>(appState) {};

	void render(SkCanvas* c, int32_t index) override {
		const auto& s = _values.at(index);

		flush(c);
		TextViewBase::render(c, index);
		SkFont& font = _STATE->font_normal;
		float x, y;
		font.setSize(_STATE->textsize2 * .9f);
		centerText(font, this, s.c_str(), x, y);
		SkPaint paint;
		paint.setColor(
			_DATA->tracks[(int)_STATE->params[_STATE->active_track.load()][DISTRSOURCE].load()]->name ==
			s
			? skcol::active
			: skcol::fg);
		c->drawString(s.c_str(), _STATE->textsize2 * .5f, y, font, paint);
	}


	void computeWidth(int32_t index) override {
		const auto& s = _values.at(index);

		SkFont& font = _STATE->font_normal;
		font.setSize(_STATE->textsize2 * .9);
		SkRect bounds{};
		font.measureText(s.c_str(), s.size(),
			SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	int32_t cb(const InputEvent& e, int index) override {
		if (e.action == ACTION_UP) {
			auto tmp = indexhot.load();
			if (tmp >= 0 && tmp < _values.size()) {
				auto t = _STATE->active_track.load();
				tsl::parameters::Event e;
				e.setup(_STATE, t, DISTRSOURCE);
				e.value = tmp;
				e.apply(_STATE, tsl::parameters::FromUi);
				
			}
			else
				return 0;
		}
		TextViewBase::cb(e, index);
		return 1;
	}
};

void showRouting(TRACK* track) {
	auto _appState = track->_appState;
	std::vector<std::string> vals{ "TRACK1", "TRACK2", "TRACK3", "TRACK4" };
	std::string s("INPUT ROUTING ");

	auto tmp = _DATA->inputRouting.load();
	if (tmp != nullptr) {
		tmp->deldraw();
		tmp->delCB();
	}
	else {
		tmp = std::make_shared<tsl::graphics::RecyclerView<InputRouting, std::string>>(track->_appState,
			vals);
		_DATA->inputRouting = tmp;

	}
	s.append(track->name);
	tmp->setTitle(s);
	tmp->computeSize();
	tmp->addDraw();
	tmp->addCB();
}


struct controlitem_t {
	int32_t trackindex;
	int32_t idview;
	std::string name;
	std::atomic<MYFLOAT>* pointer;
	bool active;
};


static const int32_t items[] = { TRACK_CONTROLS_ACTIVE, BPMSYNCINPUT, GRAINSEQINPUT, LFO1CONTROLSACTIVE,
								LFO2CONTROLSACTIVE, LFO3CONTROLSACTIVE,
								DELAY_CONTROLS_ACTIVE, MDELAY1_CONTROLS_ACTIVE,
								MDELAY2_CONTROLS_ACTIVE, MDELAY3_CONTROLS_ACTIVE,
								MDELAY4_CONTROLS_ACTIVE, MDELAY5_CONTROLS_ACTIVE,
								MDELAY6_CONTROLS_ACTIVE, MDELAY7_CONTROLS_ACTIVE,
								MDELAY8_CONTROLS_ACTIVE, PP_CONTROLS_ACTIVE };

static const int32_t views[] = { TRACK_CONTROLS_ACTIVE, BPMSYNCINPUT, GRAINSEQINPUT, LFO1CONTROLSACTIVE,
								LFO1CONTROLSACTIVE, LFO1CONTROLSACTIVE,
								DELAY_CONTROLS_ACTIVE, MDELAY1_CONTROLS_ACTIVE,
								MDELAY1_CONTROLS_ACTIVE, MDELAY1_CONTROLS_ACTIVE,
								MDELAY1_CONTROLS_ACTIVE, MDELAY1_CONTROLS_ACTIVE,
								MDELAY1_CONTROLS_ACTIVE, MDELAY1_CONTROLS_ACTIVE,
								MDELAY1_CONTROLS_ACTIVE, PP_CONTROLS_ACTIVE };

static const char* itemnames[] = { "LOOP", "BPM", "SEQUENCER", "LFO1", "LFO2", "LFO3", "DELAY",
								  "MDELAY1", "MDELAY2", "MDELAY3", "MDELAY4", "MDELAY5",
								  "MDELAY6",
								  "MDELAY7", "MDELAY8", "PPDELAY" };


class Syncing : public TextViewBase<controlitem_t> {
public:
	explicit Syncing(tsl::AppState* appState) : TextViewBase<controlitem_t>(appState) {};

	void render(SkCanvas* c, int32_t index) override {
		const auto& s = _values.at(index);

		flush(c);
		TextViewBase::render(c, index);
		SkFont& font = _STATE->font_normal;
		float x, y;
		font.setSize(_STATE->textsize2 * .9f);
		//font.setEmbolden(s.find("INPUT") != std::string::npos);
		std::string ss = "TRACK";
		ss.append(std::to_string(s.trackindex + 1));
		ss.append(" ");
		ss.append(s.name);
		SkPaint paint;
		paint.setColor(s.active ? skcol::active : skcol::fg);
		centerText(font, this, ss.c_str(), x, y);
		c->drawString(ss.c_str(), _STATE->textsize2 * .5f, y, font, paint);
	}


	void computeWidth(int32_t index) override {
		const auto& s = _values.at(index);
		std::string ss = "TRACK";
		ss.append(std::to_string(s.trackindex));
		ss.append(" ");
		ss.append(s.name);
		SkFont& font = _STATE->font_normal;
		font.setSize(_STATE->textsize2 * .9);
		SkRect bounds{};
		font.measureText(ss.c_str(), ss.size(),
			SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	int32_t cb(const InputEvent& e, int index) override {
		if (e.action == ACTION_UP) {
			auto tmp = indexhot.load();
			if (tmp >= 0 && tmp < _values.size()) {
				set_controlitem(_appState, _values[tmp]);
				TextViewBase::cb(e, index);
				return 0;
			}
			else return 0;
		}
		return TextViewBase::cb(e, index);
	}

	static
		bool set_controlitem(tsl::AppState* _appState, controlitem_t& item) {
		auto pointer = item.pointer;
		*pointer = !*pointer;
		item.active = pointer->load();
		View* v = _STATE->parameters[item.idview].view;
		if (v)
			_STATE->toUiThreadQueue.try_push([v] {
			if (v->visible_)
				v->redraw();
				});
		return pointer->load();
	}

	static void get_controlitems(tsl::AppState* _appState, std::vector<controlitem_t>& vector) {
		int32_t size = _STATE->dofastrender.load() ? ARRAY_LEN(itemnames) : 6;
		for (int32_t j = 0; j < 4; j++) {
			for (int32_t i = 0; i < size; i++) {
				vector.push_back({ j, views[i], itemnames[i],
								  &_STATE->params[_DATA->tracks[j]->index][items[i]],
								  _STATE->params[_DATA->tracks[j]->index][items[i]].load() ==
								  1.0 });
			}
		}
	}

	static void showSyncing(tsl::AppState* _appState) {
		auto tmp = _DATA->showSyncing.load();
		if (tmp != nullptr) {
			tmp->deldraw();
			tmp->delCB();
		}
		else {
			tmp = std::make_shared<tsl::graphics::RecyclerView<Syncing, controlitem_t>>(_STATE);
			_DATA->showSyncing = tmp;
		}
		std::vector<controlitem_t> vals;
		get_controlitems(_appState, vals);
		tmp->setTitle("SYNCING");
		tmp->setValues(vals);
		tmp->computeSize();
		tmp->addDraw();
		tmp->addCB();
	}


};


class ImportPreset : public TextViewBase<std::shared_ptr<tsl::preset::PresetWrapper>> {
public:
	explicit ImportPreset(tsl::AppState* appState) : TextViewBase<std::shared_ptr<tsl::preset::PresetWrapper>>(appState) {};

	void render(SkCanvas* c, int32_t index) override {
		const auto& s = _values.at(index);
		flush(c);
		TextViewBase::render(c, index);

		SkFont& font = _STATE->font_normal;
		SkPaint paint;
		paint.setColor(skcol::fg);
		float x, y;
		font.setSize(_STATE->textsize2 * .9f);

		centerText(font, this, s->name.c_str(), x, y);
		c->drawString(s->name.c_str(), _STATE->textsize2 * .5f, y, font, paint);
	}


	void computeWidth(int32_t index) override {
		const auto& s = _values.at(index);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9);
		SkRect bounds{};
		font.measureText(s->name.c_str(), s->name.size(),
			SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	int32_t cb(const InputEvent& e, int index) override {
		if (e.action == ACTION_UP) {
			auto tmp = indexhot.load();
			if (tmp >= 0 && tmp < _values.size()) {
				const auto& s = _values.at(tmp);
				auto t = _DATA->tracks[_STATE->active_track.load()];
				auto res2 = _STATE->WorkerQueue.add_taskInt(tsl::preset::loadPresetThreadFunc, t, s);
				if (res2 > 0) {
					char text[100];
					snprintf(text, 100, "%s Load Preset Queued. Pos %d.", t->name,
						res2 + 1);
					showToast(_STATE, text);
				}

			}
			else return 0;
		}
		return TextViewBase::cb(e, index);

	}

	static void importPreset(TRACK* track, const bool isProject) {
		auto _appState = track->_appState;
		auto& q = isProject ? _DATA->projects : _DATA->presets;
		std::vector<std::shared_ptr<tsl::preset::PresetWrapper>> presets;
		{
			std::lock_guard lk(q);
			if (q.empty()) {
				getPresets(presets, isProject);
				for (auto& p : presets) q.push(p);
			}
			else
				for (auto& pr : q)presets.push_back(pr);
		}
		if (!presets.empty()) {
			tsl::graphics::RecyclerView < ImportPreset, std::shared_ptr<tsl::preset::PresetWrapper>> tmp(
				track->_appState);
			tmp.acquire_slot();
			std::string s(isProject ? "IMPORT PROJECT" : "IMPORT PRESET ");
			if (!isProject)
				s.append(track->name);
			tmp.setTitle(s);
			tmp.setValues(presets);
			tmp.computeSize();
			tmp.addDraw();
			tmp.addCB();
			_STATE->waitNotify.wait_for_signal(tmp.token());
			tmp.deldraw();
			tmp.delCB();

		}
		else
			showToast(track->_STATE, isProject ? "No Projects found" : "No Presets found.");
	}
};


class ImportMapping : public TextViewBase<std::shared_ptr<MIDIHEADER2>> {
public:
	explicit ImportMapping(tsl::AppState* appState) : TextViewBase<std::shared_ptr<MIDIHEADER2>>(appState) {};

	void render(SkCanvas* c, int32_t index) override {
		const auto s = _values.at(index);
		flush(c);
		TextViewBase::render(c, index);
		SkFont font(_STATE->font_normal);
		SkPaint paint;
		paint.setColor(fg);
		float x, y;
		font.setSize(_STATE->textsize2 * .9f);

		centerText(font, this, s->h.name, x, y);
		c->drawString(s->h.name, _STATE->textsize2 * .5f, y, font, paint);
	}


	void computeWidth(int32_t index) override {
		const auto s = _values.at(index);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9);
		SkRect bounds{};
		font.measureText(s->h.name, strlen(s->h.name),
			SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	int32_t cb(const InputEvent& e, int index) override {
		if (e.action == ACTION_UP) {
			auto tmp = indexhot.load();
			if (tmp >= 0 && tmp < _values.size()) {
				auto& s = _values.at(tmp);
				auto res2 = _STATE->WorkerQueue.add_taskInt(loadMidiMapping, _STATE, s);
				if (res2 > 0) {
					char text[100];
					snprintf(text, 100, "Load MidiMapping Queued. Pos %d.",
						res2 + 1);
					showToast(_STATE, text);
				}
			}
			else return 0;
		}
		return TextViewBase::cb(e, index);
	}

	static void importMapping(tsl::AppState* _appState) {
		auto& q = _STATE->mappings;
		std::vector<std::shared_ptr<MIDIHEADER2>> presets;
		{
			std::lock_guard lk(q);
			if (q.empty()) {
				getMappings(presets);
				for (auto& p : presets) q.push(p);
			}
			else
				for (auto& pr : q)presets.push_back(pr);
		}
		if (!presets.empty()) {
			tsl::graphics::RecyclerView<ImportMapping, std::shared_ptr<MIDIHEADER2>>tmp(
				_appState);
			tmp.acquire_slot();
			tmp.setTitle("IMPORT MIDImapping");
			tmp.setValues(presets);
			tmp.computeSize();
			tmp.addDraw();
			tmp.addCB();
			_STATE->waitNotify.wait_for_signal(tmp.token());
			tmp.deldraw();
			tmp.delCB();

		}
		else
			showToast(_STATE, "No Mappings found.");
	}

};


using OT2 = std::shared_ptr<MIDIHEADER2>;
#include "MidiSaver.h"

static void saveMapping(tsl::AppState* _appState) {
	tsl::app::deleteOverwriteFunc<OT2>(
		_STATE,
		_STATE->mappings,
		"Save MIDImapping",
		std::function<std::vector<OT2>()>(
			[] {
				std::vector<OT2> presets;

				getMappings(presets);
				return presets;
			}),
		[](tsl::AppState* _appState, std::string& name) {
			return saveMappingGotFileName(_appState, name) == 0;
		});

	return;
}

static void showLogs(tsl::AppState* _appState) {
	auto tmp = _appState->logView.load();
	if (tmp == nullptr) {
		tmp = std::make_shared<tsl::graphics::LogView>(_appState);
		_appState->logView.store(tmp);
	}
	tmp->init();
	tmp->addDraw();
	tmp->addCB();
}

class TrackSettings : public TextViewBase<std::string> {
public:
	explicit TrackSettings(tsl::AppState* appState) : TextViewBase<std::string>(appState) {};

	void render(SkCanvas* c, int32_t index) override {
		const auto& s = _values.at(index);
		flush(c);
		TextViewBase::render(c, index);
		SkFont font(_STATE->font_normal);
		SkPaint paint;
		paint.setColor(s == "MULTITHREADED" && _DATA->isMultithreaded.load() ? skcol::active : skcol::fg);
		float x, y;
		font.setSize(_STATE->textsize2 * .9f);
		centerText(font, this, s.c_str(), x, y);
		c->drawString(s.c_str(), textOffset, y, font, paint);
	}


	void computeWidth(int32_t index) override {
		const auto& s = _values.at(index);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9);
		SkRect bounds{};
		font.measureText(s.c_str(), s.size(),
			SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	int32_t cb(const InputEvent& e, int index) override {
		if (e.action == ACTION_UP) {
			auto tmp = indexhot.load();
			if (tmp >= 0 && tmp < _values.size()) {
				const auto& s = _values.at(tmp);
				if (s == "HISTORY") {
					_STATE->toUiThreadQueue.try_push([this] {
						auto& h = _DATA->snapShot;

						if (!h.visible_) {
							h.init();
							h.addDraw();
							h.addCB();

						}
						});

				}
				else if (s == "RESET TRACK") {
					TRACK* t = _DATA->tracks[_STATE->active_track.load()];
					const auto tindex = t->index;
					auto _appState = t->_appState;

					auto res2 = _DATA->snapShot.add_taskInt([_STATE, t] {
						std::lock_guard lk(_DATA->snapShot);

						auto doGainTask = _STATE->player.isPlaying() && _STATE->params[t->index][POWERTRACK].load() == 1.0;
						if (doGainTask) {
							t->gainTask.setTargetAutoWait(tsl::gaintask::GainDown);
							auto token = _STATE->waitNotify.begin_wait();
							_DATA->toAudioThreadQueue.try_push([t, _STATE, token]() {
								t->disabled = true;
								_STATE->waitNotify.complete(token);
								});
							_STATE->waitNotify.wait_for_signal(token);
						}
						while (auto e = _DATA->snapShot.queue.try_pop()) {
							e->flags &= ~tsl::parameters::Event::History;
							_DATA->snapShot.addEvent(std::move(*e));
						}
						auto oldSnap = _DATA->snapShot.get();
						std::vector<tsl::parameters::Event> fileEvents;
						fileEvents.push_back(std::move(t->currentAudioToEvent()));
						t->setToInitState();
						_DATA->snapShot.clear(t->index);
						fileEvents.push_back(std::move(t->currentAudioToEvent()));
						auto newSnap = _DATA->snapShot.get();
						_DATA->snapShot.OnPresetLoaded(oldSnap, newSnap, fileEvents);
						if (doGainTask) {
							auto token = _STATE->waitNotify.begin_wait();
							_DATA->toAudioThreadQueue.try_push([t, _STATE, token]() {
								t->gainTask.setTarget(0, 0);
								t->disabled = false;
								_STATE->waitNotify.complete(token);
								});
							_STATE->waitNotify.wait_for_signal(token);
						}


						_STATE->toUiThreadQueue.try_push([_STATE, t] {if (
							_DATA->tracks[_STATE->active_track.load()] == t) TrackButton::func(_STATE, t->index);
							});
						});
					if (res2 > 0) {
						char text[100];
						snprintf(text, 100, "RESET TRACK Queued. Pos %d.",
							res2 + 1);
						showToast(_STATE, text);
					}
					indexhot = -1;
					return 0;
				}
				else if (s == "RESET ALL") {
					auto res2 = _DATA->snapShot.add_taskInt([_STATE = _DATA->track1._appState] {
						std::lock_guard lk(_DATA->snapShot);

						std::vector<TRACK*> tracks{};
						if (_STATE->player.isPlaying()) {
							for (auto t : _DATA->tracks) {
								if (_STATE->params[t->index][POWERTRACK].load() == 1.0)
									tracks.push_back(t);
							}
							if (!tracks.empty())for (auto t : tracks) {
								t->gainTask.setTarget(0, -120);
								_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
								auto token = _STATE->waitNotify.begin_wait();
								_DATA->toAudioThreadQueue.try_push([tracks, _STATE, token]() {
									for (auto t : tracks)t->disabled = true;
									_STATE->waitNotify.complete(token);
									});
								_STATE->waitNotify.wait_for_signal(token);
							}
						}
						while (auto e = _DATA->snapShot.queue.try_pop()) {
							e->flags &= ~tsl::parameters::Event::History;
							_DATA->snapShot.addEvent(std::move(*e));
						}
						auto oldSnap = _DATA->snapShot.get();
						std::vector<tsl::parameters::Event> fileEvents;
						for (auto t : _DATA->tracks) {
							fileEvents.push_back(std::move(t->currentAudioToEvent()));
							_DATA->snapShot.clear(t->index);
							t->setToInitState();
							fileEvents.push_back(std::move(t->currentAudioToEvent()));
							_STATE->params[t->index][POWERTRACK].store(0.0);
						}
						_STATE->params[0][POWERTRACK].store(1.0);
						auto newSnap = _DATA->snapShot.get();
						_DATA->snapShot.OnPresetLoaded(oldSnap, newSnap, fileEvents);
						if (!tracks.empty())for (auto t : tracks) {
							auto token = _STATE->waitNotify.begin_wait();
							_DATA->toAudioThreadQueue.try_push([tracks, _STATE, token]() {
								for (auto t : tracks) {
									t->gainTask.setTargetAutoWait(tsl::gaintask::GainUp);
									t->disabled = false;
								}
								_STATE->waitNotify.complete(token);
								});
							_STATE->waitNotify.wait_for_signal(token);
						}
						_STATE->toUiThreadQueue.try_push([_STATE] {
							TrackButton::func(_STATE, _STATE->active_track.load());
							if (_STATE->parameters[POWERTRACK].view->visible_)
								_STATE->parameters[POWERTRACK].view->redraw();
							});
						});
					if (res2 > 0) {
						char text[100];
						snprintf(text, 100, "RESET ALL Queued. Pos %d.",
							res2 + 1);
						showToast(_STATE, text);
					}
					indexhot = -1;
					return 0;
				}
				else if (s == "ACTIVE EFFECTS") {
					auto res2 = _DATA->snapShot.add_taskInt([_STATE = _DATA->track1._appState] {
						auto t = _DATA->tracks[_STATE->active_track.load()];
						tsl::graphics::EffectOrderView::activate(t);
						});
					if (res2 > 0) {
						char text[100];
						snprintf(text, 100, "ACTIVE EFFECTS Queued. Pos %d.",
							res2 + 1);
						showToast(_STATE, text);
					}
					else {
						indexhot = -1;
						return 0;
					}
				}
				else if (s == "LOOP POS") {
					_STATE->UiTasksQueue.add_task(LoopPositions::show, _appState,
						_STATE->active_track.load());

				}
				else if (s == "EDITOR") {
					_STATE->UiTasksQueue.add_task(ListView::show, _appState, &_DATA->editorView,
						EDITORCOPY, EDITORMAXIMIZE);
				}
				else if (s == "RECORD") {
					if (_STATE->dofastrender.load())
						_STATE->UiTasksQueue.add_task(ListView::show, _appState, &_DATA->recordView,
							EDITORLOOP1,
							EDITORSAVE);
					else
						_STATE->UiTasksQueue.add_task(ListView::show, _appState, &_DATA->recordView,
							EDITORLOOP1,
							EDITORREC4);
				}
#ifdef GS_AIGEN
				else if (s == "RECORD AI->TRACK") {
					tsl::aigen::recordToTrack(_appState,
						_DATA->tracks[_STATE->active_track.load()]);
				}
#endif
				else if (s == "INPUT ROUTING") {
					_STATE->UiTasksQueue.add_task(showRouting,
						_DATA->tracks[_STATE->active_track.load()]);
				}
				else if (s == "SYNCING") {
					_STATE->UiTasksQueue.add_task(Syncing::showSyncing, _appState);
				}
				else if (s == "IMPORT PRESET") {
					_STATE->UiTasksQueue.add_task(ImportPreset::importPreset,
						_DATA->tracks[_STATE->active_track.load()],
						false);
				}
				else if (s == "IMPORT PROJECT") {
					_STATE->UiTasksQueue.add_task(ImportPreset::importPreset,
						_DATA->tracks[_STATE->active_track.load()],
						true);
				}
				else if (s == "SAVE PRESET") {
					TRACK* t = _DATA->tracks[_STATE->active_track.load()];
					auto res2 = _DATA->snapShot.add_taskInt([_STATE = _DATA->track1._appState, t] {
						tsl::preset::save_preset(t, false);
						});
					if (res2 < 0)
						showToast(_STATE, "SAVE PRESET: queue full, try again.");
					else if (res2 > 0) {
						char text[100];
						snprintf(text, 100, "SAVE PRESET Queued. Pos %d.",
							(int)res2 + 1);
						showToast(_STATE, text);
					}

				}
				else if (s == "SAVE PROJECT") {
					TRACK* t = _DATA->tracks[_STATE->active_track.load()];
					auto res2 = _DATA->snapShot.add_taskInt([_STATE = _DATA->track1._appState, t] {
						tsl::preset::save_preset(t, true);
						});
					if (res2 < 0)
						showToast(_STATE, "SAVE PROJECT: queue full, try again.");
					else if (res2 > 0) {
						char text[100];
						snprintf(text, 100, "SAVE PROJECT Queued. Pos %d.",
							(int)res2 + 1);
						showToast(_STATE, text);
					}
				}
				else if (s == "IMPORT MIDI MAPPING") {
					// Init-statement, not `auto res = ... > 0`: that parses as
					// `auto res = (add_taskInt(...) > 0)`, so res was the bool and
					// the position was thrown away (always printed "Pos 1").
					if (auto res = _STATE->UiTasksQueue.add_taskInt(ImportMapping::importMapping, _appState); res < 0)
						showToast(_STATE, "Import Midi Mapping: queue full, try again.");
					else if (res > 0) {
						char text[100];
						snprintf(text, 100, "Import Midi Mapping Queued. Pos %d.",
							(int)res + 1);
						showToast(_STATE, text);
					};
				}
				else if (s == "SAVE MIDI MAPPING") {
					// Same precedence fix. The threshold was `> 1` here and `> 0` at
					// the five sibling sites; normalised to `> 0`.
					if (auto res = _STATE->UiTasksQueue.add_taskInt(saveMapping, _appState); res < 0)
						showToast(_STATE, "Save Midi Mapping: queue full, try again.");
					else if (res > 0) {
						char text[100];
						snprintf(text, 100, "Save Midi Mapping Queued. Pos %d.",
							(int)res + 1);
						showToast(_STATE, text);

					}
				}
				else if (s == "LOGS") {
					_STATE->UiTasksQueue.add_task(showLogs, _appState);
				}
				else if (s == "MULTITHREADED") {
					auto x = _DATA->isMultithreaded.load();
					auto des = x == true ? false : true;
					while (!_DATA->isMultithreaded.compare_exchange_weak(x, des,
						std::memory_order_release,
						std::memory_order_relaxed));
					indexhot = -1;
					return 0;

				}
				else if (s == "SAVE SS") {
					currentCS.store(_DATA->snapShot.get());
					showToast(_STATE, "Saved.");
					indexhot = -1;
					return 0;

				}
				else if (s == "LOAD SS") {
					auto cs = currentCS.load();
					if (cs) {
						_DATA->snapShot.add_task([this, cs] {
							std::vector < tsl::parameters::Event > events_;
							std::string names_[4];
							tsl::parameters::Snapshot::deserialize(cs->mem, cs->size, events_, names_);
							tsl::parameters::Snapshot::apply(_STATE, cs, events_, names_, false);
							showToast(_STATE, "Ok");
							});
					}
					else {
						showToast(_STATE, "Empty.");
					}
					indexhot = -1;
					return 0;

				}
			}
			else return 0;
		}
		return TextViewBase::cb(e, index);
	}
	tsl::AtomicSharedPtr<tsl::parameters::CurrentSnapShot> currentCS{};
};



#include "FloatingScrollView.h"

void showTrackSettings(TRACK* t) {
	auto _appState = t->_appState;
	auto tmp = _DATA->trackSettings.load();
	if (tmp != nullptr) {
		tmp->deldraw();
		tmp->delCB();
	}
	else {
		std::vector<std::string> effect = { "ACTIVE EFFECTS", "LOOP POS",
									   "INPUT ROUTING", "SYNCING",
									   "EDITOR", "RECORD",
#ifdef GS_AIGEN
									   "RECORD AI->TRACK",
#endif
									   "RESET TRACK", "RESET ALL", "MULTITHREADED" };
		std::vector<std::string> effect2 = { "ACTIVE EFFECTS", "LOOP POS",
											"INPUT ROUTING",
											"SYNCING", "EDITOR", "RECORD",
#ifdef GS_AIGEN
											"RECORD AI->TRACK",
#endif
											"IMPORT PRESET", "SAVE PRESET", "IMPORT PROJECT",
											"SAVE PROJECT", "IMPORT MIDI MAPPING",
											"SAVE MIDI MAPPING", "RESET TRACK", "RESET ALL", "MULTITHREADED"
#if defined (RELEASEBUILD)
											,"LOGS", "SAVE SS", "LOAD SS"
#endif
		};
		tmp = std::make_shared<tsl::graphics::FloatingScrollView<TrackSettings, std::string>>(
			t->_appState,
			_STATE->dofastrender ? effect2 : effect);
		_DATA->trackSettings = tmp;
	}
	tmp->setTitle(t->name);
	tmp->init();
	tmp->addDraw();
	tmp->addCB();

}

void showSettings(tsl::AppState* _appState) {

	auto tmp = _DATA->generalSettings.load();
	if (tmp != nullptr) {
		tmp->deldraw();
		tmp->delCB();
	}
	else {
		std::vector<std::string> effect = { "UPGRADE", "INSTRUCTIONS",
									   "CHANGELOG", "EULA",
									   "OPEN SOURCE SOFTWARE" };
		std::vector<std::string> effect2 = { "UPGRADE", "INSTRUCTIONS",
									   "CHANGELOG", "EULA",
									   "OPEN SOURCE SOFTWARE" };
		tmp = std::make_shared<tsl::graphics::RecyclerView<TrackSettings, std::string>>(
			_appState,
			_STATE->dofastrender ? effect2 : effect);
		_DATA->generalSettings = tmp;
	}
	tmp->setTitle("Settings");
	tmp->computeSize();
	tmp->addDraw();
	tmp->addCB();

}



void midiLearningEvent(tsl::AppState* _appState) {
	LoopPositions::midiLearningEvent(_appState);
	ListView::midiLearningEvent(_DATA->editorView);
	ListView::midiLearningEvent(_DATA->recordView);
}
