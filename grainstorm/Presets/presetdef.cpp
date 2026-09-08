//
// Created by pr on 23.07.25.
//

#include "presetdef.h"
#include "track.h"
#include "gui/gui.h"
#include "grainstorm.h"
#include "params.h"
using namespace tsl::parameters;
using namespace tsl::preset;
auto findFxType = [](uint16_t fxNum, bool power = true) {
	for (auto& e : grainmod2values)   if ((float)fxNum == e) return power ? powerGrainFx : bypassGrainFx;
	for (auto& e : fxtypes2values)    if ((float)fxNum == e) return power ? powerFx : bypassFx;
	for (auto& e : reverbtypevalues)  if ((float)fxNum == e) return power ? powerStereoFx : bypassStereoFx;
	return EventSubtype{ UINT8_MAX };
	};

const char* checklfodest(const char* input) {
	if (!strcmp(input, "GAIN"))
		return "PREGAIN";
	else if (!strcmp(input, "BANDPASS CENTER") || !strcmp(input, "BANDPASS CF"))
		return "RESON CF";
	else if (!strcmp(input, "BANDPASS WIDTH") || !strcmp(input, "BANDPASS Q"))
		return "RESON Q";
	else if (!strcmp(input, "PHASER IV SPACING"))
		return "PHASER IV DISTANCE";
	else if (!strcmp(input, "AWIN CYCLES"))
		return "ENV CYCLES";
	else if (!strcmp(input, "ST PHASER II"))
		return "PHASER II";
	else if (!strcmp(input, "FILTER"))
		return "CEPSTRUM";
	else if (!strcmp(input, "FILTER WHITE"))
		return "CEPSTRUM WHITE";
	else if (!strcmp(input, "ROBOTIZATION"))
		return "ZERO PHASE";
	else if (!strcmp(input, "RND WRITE"))
		return "DEVIATION";
	else
		return input;
}

void setenvouter(TRACK* track, const char* fieldname) {
	auto _appState = track->_appState;
	auto index = findIndexChar(envelopesnames, ARRAY_LEN(envelopesnames), fieldname);
	auto old = _STATE->params[track->index][ENVOUTER].exchange(index);
}

void setenvinner(TRACK* track, const char* fieldname) {
	auto _appState = track->_appState;
	auto index = findIndexChar(envelopesnames, ARRAY_LEN(envelopesnames), fieldname);
	_STATE->params[track->index][ENVINNER].store(index);
}

void load_preset17(TRACK* t, Preset17* pr) {
	auto _appState = t->_appState;
	auto rec = t->filebuffer.load();

	std::vector<Event> ev{};

	for (int32_t i = 0; i < pr->numparameters; i++) {
		if (!(_STATE->parameters[i].flags & Param::NoAssignment)) {
			_STATE->params[t->index][i].store(pr->fxvalues[i]);
			_STATE->controls[t->index][i].lfo_min.store(pr->lfomin[i]);
			_STATE->controls[t->index][i].lfo_max.store(pr->lfomax[i]);

			if (_STATE->parameters[i].initvalue != pr->fxvalues[i]) {
				ev.push_back(Event::createEvent(t->index, paramUpdate, i, pr->fxvalues[i], 0, 0, 0));

			}
			if (_STATE->parameters[i].initvalue != pr->lfomin[i]) {
				ev.push_back(Event::createEvent(t->index, lfoMin, i, pr->lfomin[i], 0, 0, 0));

			}
			if (_STATE->parameters[i].initvalue != pr->lfomax[i]) {
				ev.push_back(Event::createEvent(t->index, lfoMax, i, pr->lfomax[i], 0, 0, 0));
			}
		}
	}

	_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;


	t->lfo1.store(LFORECOMPUTE, 1.0);
	t->lfo1.store(LFOREDRAW, 1.0);
	t->lfo2.store(LFORECOMPUTE, 1.0);
	t->lfo2.store(LFOREDRAW, 1.0);
	t->lfo3.store(LFORECOMPUTE, 1.0);
	t->lfo3.store(LFOREDRAW, 1.0);


	t->setLfoDest(0, (int)pr->fxvalues[LFO1DEST]);
	t->setLfoDest(1, (int)pr->fxvalues[LFO2DEST]);
	t->setLfoDest(2, (int)pr->fxvalues[LFO3DEST]);

	t->setEnvfDest(0, (int)pr->fxvalues[ENVF1_DEST]);
	t->setEnvfDest(1, (int)pr->fxvalues[ENVF2_DEST]);
	t->setEnvfDest(2, (int)pr->fxvalues[ENVF3_DEST]);


	std::vector<std::pair<int, int>> vecgrain, vecfx, vecstfx;

	for (int32_t i = 0; i < NUM_PARAMSPACES; i++) {
		if (_DATA->callbacks_fx_power[i] != nullptr) {
			if (pr->fxpower[i]) {
				//LOGE("YAZ %d", i);
				if (findIndexFloatWithLen(grainmod2values, i, ARRAY_LEN(grainmods2)) != -1)
					vecgrain.emplace_back(i, pr->q_pos[i]);
				else if (findIndexFloatWithLen(reverbtypevalues, i, ARRAY_LEN(reverbtypes)) !=
					-1)
					vecstfx.emplace_back(i, pr->q_pos[i]);
				else if (findIndexFloatWithLen(fxtypes2values, i, ARRAY_LEN(fxtypes2)) != -1)
					vecfx.emplace_back(i, pr->q_pos[i]);
			}
		}
		t->bypass[i].store(pr->bypass[i]);
		if (pr->bypass[i] == 1.) {
			auto e = Event::createEvent(t->index, Power, i, 1.0, findFxType(i, false), 0, 0);
			if (e.subType != UINT8_MAX)ev.push_back(e);
		}
	}

	std::sort(vecgrain.begin(), vecgrain.end(), sortbysec);
	std::sort(vecfx.begin(), vecfx.end(), sortbysec);
	std::sort(vecstfx.begin(), vecstfx.end(), sortbysec);

	int grainFx = 0, fxx = 0, stfx = 0;
	for (auto& fx : vecgrain) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ grainFx, 1 }), powerGrainFx, 0, 0));
		grainFx++;
	}
	for (auto& fx : vecfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ fxx, 1 }), powerFx, 0, 0));
		fxx++;
	}
	for (auto& fx : vecstfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ stfx, 1 }), powerStereoFx, 0, 0));
		stfx++;
	}

	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}

	for (auto& e : ev)_DATA->snapShot.addEvent(std::move(e));
}

void load_preset18(TRACK* t, PresetWrapper& ptl) {
	auto _appState = t->_appState;
	auto& bypass = ptl.bypass, & fxpower = ptl.fxpower, & q_pos = ptl.q_pos;
	auto& pr = *ptl.get<Preset18>();

	std::vector<Event> ev{};

	for (auto& par : ptl.params) {
		if (!(_STATE->parameters[par.id].flags & Param::NoAssignment)) {
			if (par.type == NormalParam) {
				ev.push_back(Event::createEvent(t->index, paramUpdate, par.id, par.val, 0, 0, 0));
				_STATE->params[t->index][par.id].store(par.val);

			}
			else if (par.type == LFOmin) {
				ev.push_back(Event::createEvent(t->index, lfoMin, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_min.store(par.val);

			}
			else if (par.type == LFOmax) {
				ev.push_back(Event::createEvent(t->index, lfoMax, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_max.store(par.val);
			}
		}
	}

	_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;


	t->lfo1.store(LFORECOMPUTE, 1.0);
	t->lfo1.store(LFOREDRAW, 1.0);
	t->lfo2.store(LFORECOMPUTE, 1.0);
	t->lfo2.store(LFOREDRAW, 1.0);
	t->lfo3.store(LFORECOMPUTE, 1.0);
	t->lfo3.store(LFOREDRAW, 1.0);


	t->setLfoDest(0, (int)ptl.params[LFO1DEST].val);
	t->setLfoDest(1, (int)ptl.params[LFO2DEST].val);
	t->setLfoDest(2, (int)ptl.params[LFO3DEST].val);

	t->setEnvfDest(0, (int)ptl.params[ENVF1_DEST].val);
	t->setEnvfDest(1, (int)ptl.params[ENVF2_DEST].val);
	t->setEnvfDest(2, (int)ptl.params[ENVF3_DEST].val);

	std::vector<std::pair<int, int>> vecgrain, vecfx, vecstfx;

	for (int32_t i = 0; i < pr.numfx; i++) {
		if (_DATA->callbacks_fx_power[i] != nullptr) {
			if (fxpower[i] == 1.0) {
				//LOGE("YAZ %d", i);
				if (findIndexFloatWithLen(grainmod2values, i, ARRAY_LEN(grainmods2)) != -1)
					vecgrain.emplace_back(i, q_pos[i]);
				else if (findIndexFloatWithLen(reverbtypevalues, i, ARRAY_LEN(reverbtypes)) !=
					-1)
					vecstfx.emplace_back(i, q_pos[i]);
				else if (findIndexFloatWithLen(fxtypes2values, i, ARRAY_LEN(fxtypes2)) != -1)
					vecfx.emplace_back(i, q_pos[i]);
			}
		}
		t->bypass[i].store(bypass[i]);
		if (bypass[i] == 1.) {
			auto e = Event::createEvent(t->index, Power, i, 1.0, findFxType(i, false), 0, 0);
			if (e.subType != UINT8_MAX)ev.push_back(e);
		}
	}

	std::sort(vecgrain.begin(), vecgrain.end(), sortbysec);
	std::sort(vecfx.begin(), vecfx.end(), sortbysec);
	std::sort(vecstfx.begin(), vecstfx.end(), sortbysec);

	int grainFx = 0, fxx = 0, stfx = 0;
	for (auto& fx : vecgrain) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ grainFx, 1 }), powerGrainFx, 0, 0));
		grainFx++;
	}
	for (auto& fx : vecfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ fxx, 1 }), powerFx, 0, 0));
		fxx++;
	}
	for (auto& fx : vecstfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ stfx, 1 }), powerStereoFx, 0, 0));
		stfx++;
	}

	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}

	if (!strcmp(pr.header, "GPR")) {
		ev.push_back(Event::createEvent(t->index, Power, 0, ptl.params[POWERTRACK].val, powerTrack, 0, 0));
		ev.push_back(Event::createEvent(t->index, paramUpdate, DISTRSOURCE, ptl.params[DISTRSOURCE].val, 0, 0, 0));

		_STATE->params[t->index][POWERTRACK].store(ptl.params[POWERTRACK].val);
		_STATE->params[t->index][DISTRSOURCE].store(ptl.params[DISTRSOURCE].val);
	}

	for (auto& e : ev)_DATA->snapShot.addEvent(std::move(e));
}

void load_preset19(TRACK* t, PresetWrapper& ptl) {
	auto _appState = t->_appState;
	auto& bypass = ptl.bypass, & fxpower = ptl.fxpower, & q_pos = ptl.q_pos;
	auto& pr = *ptl.get<Preset19>();

	std::vector<Event> ev{};

	for (auto& par : ptl.params) {
		if (!(_STATE->parameters[par.id].flags & Param::NoAssignment)) {
			if (par.type == NormalParam) {
				ev.push_back(Event::createEvent(t->index, paramUpdate, par.id, par.val, 0, 0, 0));
				_STATE->params[t->index][par.id].store(par.val);

			}
			else if (par.type == LFOmin) {
				ev.push_back(Event::createEvent(t->index, lfoMin, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_min.store(par.val);

			}
			else if (par.type == LFOmax) {
				ev.push_back(Event::createEvent(t->index, lfoMax, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_max.store(par.val);
			}
		}
	}

	_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;


	t->lfo1.store(LFORECOMPUTE, 1.0);
	t->lfo1.store(LFOREDRAW, 1.0);
	t->lfo2.store(LFORECOMPUTE, 1.0);
	t->lfo2.store(LFOREDRAW, 1.0);
	t->lfo3.store(LFORECOMPUTE, 1.0);
	t->lfo3.store(LFOREDRAW, 1.0);

	t->setEnvfDest(0, (int)ptl.params[ENVF1_DEST].val);
	t->setEnvfDest(1, (int)ptl.params[ENVF2_DEST].val);
	t->setEnvfDest(2, (int)ptl.params[ENVF3_DEST].val);

	std::vector<std::pair<int, int>> vecgrain, vecfx, vecstfx;

	for (int32_t i = 0; i < pr.numfx; i++) {
		if (_DATA->callbacks_fx_power[i] != nullptr) {
			if (fxpower[i] == 1.0 && i != SPACE_PITCH && i != SPACE_ARP) {
				//LOGE("YAZ %d", i);
				if (findIndexFloatWithLen(grainmod2values, i, ARRAY_LEN(grainmods2)) != -1)
					vecgrain.emplace_back(i, q_pos[i]);
				else if (findIndexFloatWithLen(reverbtypevalues, i, ARRAY_LEN(reverbtypes)) !=
					-1)
					vecstfx.emplace_back(i, q_pos[i]);
				else if (findIndexFloatWithLen(fxtypes2values, i, ARRAY_LEN(fxtypes2)) != -1)
					vecfx.emplace_back(i, q_pos[i]);
			}
		}
		t->bypass[i].store(bypass[i]);
		if (bypass[i] == 1.) {
			auto e = Event::createEvent(t->index, Power, i, 1.0, findFxType(i, false), 0, 0);
			if (e.subType != UINT8_MAX)ev.push_back(e);
		}
	}

	std::sort(vecgrain.begin(), vecgrain.end(), sortbysec);
	std::sort(vecfx.begin(), vecfx.end(), sortbysec);
	std::sort(vecstfx.begin(), vecstfx.end(), sortbysec);

	int grainFx = 0, fxx = 0, stfx = 0;
	for (auto& fx : vecgrain) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ grainFx, 1 }), powerGrainFx, 0, 0));
		grainFx++;
	}
	for (auto& fx : vecfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ fxx, 1 }), powerFx, 0, 0));
		fxx++;
	}
	for (auto& fx : vecstfx) {
		_DATA->callbacks_fx_power[fx.first](t, true);
		ev.push_back(Event::createEvent(t->index, Power, fx.first, std::bit_cast<double>(PowerState{ stfx, 1 }), powerStereoFx, 0, 0));
		stfx++;
	}

	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}

	if (!strcmp(pr.header, "GPR")) {
		ev.push_back(Event::createEvent(t->index, Power, 0, ptl.params[POWERTRACK].val, powerTrack, 0, 0));
		ev.push_back(Event::createEvent(t->index, paramUpdate, DISTRSOURCE, ptl.params[DISTRSOURCE].val, 0, 0, 0));
		_STATE->params[t->index][POWERTRACK].store(ptl.params[POWERTRACK].val);
		_STATE->params[t->index][DISTRSOURCE].store(ptl.params[DISTRSOURCE].val);
	}

	for (auto& lfo1 : ptl.lfo1) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo1, 1.0, 0, 0, 0));
		t->lfo[(int)lfo1].store(t->lfos[0]);
	}
	for (auto& lfo2 : ptl.lfo2) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo2, 1.0, 1, 0, 0));

		t->lfo[(int)lfo2].store(t->lfos[1]);
	}
	for (auto& lfo3 : ptl.lfo3) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo3, 1.0, 2, 0, 0));

		t->lfo[(int)lfo3].store(t->lfos[2]);
	}
	SequencerState s{};
	s.arpcycledir = _STATE->params[t->index][ARP_CYCLEDIR];
	s.arpstep = _STATE->params[t->index][ARP_CYCLESTEP];
	s.silencecount = _STATE->params[t->index][SILENCECOUNT];
	s.grainscount = _STATE->params[t->index][GRAINSCOUNT];
	s.currentnote = 0;
	ev.push_back(Event::createEvent(t->index, paramUpdate, 0, std::bit_cast<double>(s), sequencerState, 0, 0));

	t->grainsequencer.setState(_STATE->params[t->index][ARP_CYCLESTEP],
		_STATE->params[t->index][ARP_CYCLEDIR],
		_STATE->params[t->index][SILENCECOUNT],
		_STATE->params[t->index][GRAINSCOUNT]);

	for (auto& e : ev)_DATA->snapShot.addEvent(std::move(e));
}

void load_preset20(TRACK* t, PresetWrapper& ptl) {
	auto _appState = t->_appState;
	auto& bypass = ptl.bypass, & fxpower = ptl.fxpower;
	auto& pr = *ptl.get<Preset20>();

	std::vector<Event> ev{};

	for (auto& par : ptl.params) {
		if (!(_STATE->parameters[par.id].flags & Param::NoAssignment)) {
			if (par.type == NormalParam) {
				ev.push_back(Event::createEvent(t->index, paramUpdate, par.id, par.val, 0, 0, 0));
				_STATE->params[t->index][par.id].store(par.val);

			}
			else if (par.type == LFOmin) {
				ev.push_back(Event::createEvent(t->index, lfoMin, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_min.store(par.val);

			}
			else if (par.type == LFOmax) {
				ev.push_back(Event::createEvent(t->index, lfoMax, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_max.store(par.val);
			}
		}
	}


	_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;


	t->lfo1.store(LFORECOMPUTE, 1.0);
	t->lfo1.store(LFOREDRAW, 1.0);
	t->lfo2.store(LFORECOMPUTE, 1.0);
	t->lfo2.store(LFOREDRAW, 1.0);
	t->lfo3.store(LFORECOMPUTE, 1.0);
	t->lfo3.store(LFOREDRAW, 1.0);

	t->setEnvfDest(0, pr.env1);
	t->setEnvfDest(1, pr.env2);
	t->setEnvfDest(2, pr.env3);

	for (auto bp : ptl.bypass) {
		auto e = Event::createEvent(t->index, Power, bp, 1.0, findFxType(bp, false), 0, 0);
		if (e.subType != UINT8_MAX)ev.push_back(e);
		t->bypass[bp].store(1.0);
	}

	int gfx = 0, fx = 0, stfx = 0;
	for (auto fx : ptl.fxpower) {
		auto type = findFxType(fx, true);
		if (type != UINT8_MAX && _DATA->callbacks_fx_power[fx] != nullptr) {
			_DATA->callbacks_fx_power[fx](t, true);

			if (type == powerGrainFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ gfx, 1 }), type, 0, 0);
				ev.push_back(e);
				gfx++;
			}
			else if (type == powerFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ fx, 1 }), type, 0, 0);
				ev.push_back(e);
				fx++;
			}
			else if (type == powerStereoFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ stfx, 1 }), type, 0, 0);
				ev.push_back(e);
				stfx++;
			}
		}
	}


	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}

	if (ptl.isProject) {
		ev.push_back(Event::createEvent(t->index, Power, 0, pr.powertrack, powerTrack, 0, 0));
		ev.push_back(Event::createEvent(t->index, paramUpdate, DISTRSOURCE, pr.distr, 0, 0, 0));
		_STATE->params[t->index][POWERTRACK].store(pr.powertrack);
		_STATE->params[t->index][DISTRSOURCE].store(pr.distr);
	}
	for (auto& lfo1 : ptl.lfo1) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo1, 1.0, 0, 0, 0));
		t->lfo[(int)lfo1].store(t->lfos[0]);
	}
	for (auto& lfo2 : ptl.lfo2) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo2, 1.0, 1, 0, 0));

		t->lfo[(int)lfo2].store(t->lfos[1]);
	}
	for (auto& lfo3 : ptl.lfo3) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo3, 1.0, 2, 0, 0));

		t->lfo[(int)lfo3].store(t->lfos[2]);
	}
	SequencerState s{};
	s.arpcycledir = _STATE->params[t->index][ARP_CYCLEDIR];
	s.arpstep = _STATE->params[t->index][ARP_CYCLESTEP];
	s.silencecount = _STATE->params[t->index][SILENCECOUNT];
	s.grainscount = _STATE->params[t->index][GRAINSCOUNT];
	s.currentnote = 0;
	ev.push_back(Event::createEvent(t->index, paramUpdate, 0, std::bit_cast<double>(s), sequencerState, 0, 0));

	t->grainsequencer.setState(_STATE->params[t->index][ARP_CYCLESTEP],
		_STATE->params[t->index][ARP_CYCLEDIR],
		_STATE->params[t->index][SILENCECOUNT],
		_STATE->params[t->index][GRAINSCOUNT]);

	for (auto& e : ev)_DATA->snapShot.addEvent(std::move(e));

}
void load_preset21(TRACK* t, PresetWrapper& ptl) {
	std::vector<Event> ev{};
	auto _appState = t->_appState;


	auto& bypass = ptl.bypass, & fxpower = ptl.fxpower;
	auto& pr = *ptl.get<Preset21>();

	for (auto& par : ptl.params) {
		if (!(_STATE->parameters[par.id].flags & Param::NoAssignment)) {
			if (par.type == NormalParam) {
				ev.push_back(Event::createEvent(t->index, paramUpdate, par.id, par.val, 0, 0, 0));
				_STATE->params[t->index][par.id].store(par.val);

			}
			else if (par.type == LFOmin) {
				ev.push_back(Event::createEvent(t->index, lfoMin, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_min.store(par.val);

			}
			else if (par.type == LFOmax) {
				ev.push_back(Event::createEvent(t->index, lfoMax, par.id, par.val, 0, 0, 0));
				_STATE->controls[t->index][par.id].lfo_max.store(par.val);
			}
		}
	}

	_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;


	t->lfo1.store(LFORECOMPUTE, 1.0);
	t->lfo1.store(LFOREDRAW, 1.0);
	t->lfo2.store(LFORECOMPUTE, 1.0);
	t->lfo2.store(LFOREDRAW, 1.0);
	t->lfo3.store(LFORECOMPUTE, 1.0);
	t->lfo3.store(LFOREDRAW, 1.0);

	for (auto& fol : ptl.followerParams) {
		// Presets before 22 stored follower att/rel as raw milliseconds. They now live
		// on the FOLLOWERATT/FOLLOWERREL Log10 curve (20*log10(ms)), so convert here --
		// clamped to the param range first, since old files can hold 0 (log10(0) = -inf).
		if (ptl.version < 22) {
			const auto& pa = _STATE->parameters[FOLLOWERATT];
			const auto& pr_ = _STATE->parameters[FOLLOWERREL];
			fol.att = LOG10D20F(std::max<MYFLOAT>(fol.att, 0.25));
			fol.rel = LOG10D20F(std::max<MYFLOAT>(fol.rel, 0.25));
			CLAMP(fol.att, pa.min, pa.max); // CLAMP assigns to its first argument
			CLAMP(fol.rel, pr_.min, pr_.max);
		}
		ev.push_back(Event::createEvent(t->index, Eventtype::Power, fol.id, fol.envpower, followerPower, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.att, followerAtt, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.rel, followerDec, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.gain, followerGain, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.min, followerMin, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.max, followerMax, 0, 0));
		ev.push_back(Event::createEvent(t->index, Eventtype::Follower, fol.id, fol.source, followerSidechain, 0, 0));
		auto& dest = _STATE->followerMap[t->index].at(fol.id);

		dest.envpower.store(fol.envpower);
		dest.env[0] = fol.env[0];
		dest.env[1] = fol.env[1];
		dest.att.store(fol.att);
		dest.rel.store(fol.rel);
		dest.gain.store(fol.gain);
		dest.min.store(fol.min);
		dest.max.store(fol.max);
		dest.source.store(fol.source);
	}

	for (auto bp : ptl.bypass) {
		auto e = Event::createEvent(t->index, Power, bp, 1.0, findFxType(bp, false), 0, 0);
		if (e.subType != UINT8_MAX)ev.push_back(e);
		t->bypass[bp].store(true);
	}

	int gfx = 0, fx = 0, stfx = 0;
	for (auto fx : ptl.fxpower) {
		auto type = findFxType(fx, true);
		if (type != UINT8_MAX && _DATA->callbacks_fx_power[fx] != nullptr) {
			_DATA->callbacks_fx_power[fx](t, true);

			if (type == powerGrainFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ gfx, 1 }), type, 0, 0);
				ev.push_back(e);
				gfx++;
			}
			else if (type == powerFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ fx, 1 }), type, 0, 0);
				ev.push_back(e);
				fx++;
			}
			else if (type == powerStereoFx) {
				auto e = Event::createEvent(t->index, Power, fx, std::bit_cast<double>(PowerState{ stfx, 1 }), type, 0, 0);
				ev.push_back(e);
				stfx++;
			}
		}
	}

	for (int32_t i = 0; i < _STATE->channels; i++) {
		_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
		_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
	}

	if (ptl.isProject) {
		ev.push_back(Event::createEvent(t->index, Power, 0, pr.powertrack, powerTrack, 0, 0));
		ev.push_back(Event::createEvent(t->index, paramUpdate, DISTRSOURCE, pr.distr, 0, 0, 0));
		_STATE->params[t->index][POWERTRACK].store(pr.powertrack);
		_STATE->params[t->index][DISTRSOURCE].store(pr.distr);
	}
	for (auto& lfo1 : ptl.lfo1) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo1, 1.0, 0, 0, 0));
		t->lfo[(int)lfo1].store(t->lfos[0]);
	}
	for (auto& lfo2 : ptl.lfo2) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo2, 1.0, 1, 0, 0));

		t->lfo[(int)lfo2].store(t->lfos[1]);
	}
	for (auto& lfo3 : ptl.lfo3) {
		ev.push_back(Event::createEvent(t->index, lfoDest, lfo3, 1.0, 2, 0, 0));

		t->lfo[(int)lfo3].store(t->lfos[2]);
	}
	SequencerState s{};
	s.arpcycledir = _STATE->params[t->index][ARP_CYCLEDIR];
	s.arpstep = _STATE->params[t->index][ARP_CYCLESTEP];
	s.silencecount = _STATE->params[t->index][SILENCECOUNT];
	s.grainscount = _STATE->params[t->index][GRAINSCOUNT];
	s.currentnote = 0;
	ev.push_back(Event::createEvent(t->index, paramUpdate, 0, std::bit_cast<double>(s), sequencerState, 0, 0));

	t->grainsequencer.setState(_STATE->params[t->index][ARP_CYCLESTEP],
		_STATE->params[t->index][ARP_CYCLEDIR],
		_STATE->params[t->index][SILENCECOUNT],
		_STATE->params[t->index][GRAINSCOUNT]);

	for (auto& e : ev)_DATA->snapShot.addEvent(std::move(e));

}
