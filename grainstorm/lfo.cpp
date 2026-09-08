//
// Created by pr on 10.12.19.
//

#include "lfo.h"
#include "track.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "ControlItem.h"
#include "app.h"
#include <include/core/SkPath.h>

bool LFO::redrawExchange() {
	return _STATE->params[track->index][LFO1CPS + LFOREDRAW + index * LFONUMPARAMS].exchange(0.0, std::memory_order_acq_rel) == 1.0;
};


MYFLOAT LFO::gp(int32_t n) const {
	return _STATE->params[track->index][LFO1CPS + n + index * LFONUMPARAMS].load(std::memory_order_acquire);
}

MYFLOAT LFO::freq() const {
	return _STATE->params[track->index][LFO1CPS + index * LFONUMPARAMS].load();
}


MYFLOAT LFO::clockmulti() const {
	return _STATE->params[track->index][LFO1CLOCKMULTI + index * LFONUMPARAMS].load();
}

bool LFO::power() const {
	return _STATE->params[track->index][LFO1POWER + index * LFONUMPARAMS].load() == 1.0;
}


bool LFO::joinends() const {
	return _STATE->params[track->index][LFO1JOIN + index * LFONUMPARAMS].load() == 1.0;
}

MYFLOAT LFO::phs() const {
	return _STATE->params[track->index][LFO1PHS + index * LFONUMPARAMS].load();
}


MYFLOAT LFO::pos() const {
	return _STATE->params[track->index][LFO1POS + index * LFONUMPARAMS].load();
}

MYFLOAT LFO::zoom() const {
	return _STATE->params[track->index][LFO1ZOOM + index * LFONUMPARAMS].load();
}

void LFO::phsforw() {
	_STATE->params[track->index][LFO1PHS + index * LFONUMPARAMS].store(1.0);
}

void LFO::phsback() {
	_STATE->params[track->index][LFO1PHS + index * LFONUMPARAMS].store(0);
}

MYFLOAT LFO::quant() const {
	return _STATE->params[track->index][LFO1QUANT + index * LFONUMPARAMS].load();
}

int32_t LFO::func() const {
	return _STATE->params[track->index][LFO1CURVE + index * LFONUMPARAMS].load();
}

/*
MYFLOAT LFO::syncFact() const{
	return _STATE->params[track->index][LFO1SYNCFACT + index * LFONUMPARAMS].load();
};
*/
bool LFO::stopped() const {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	if (_DATA->isRunningAsPlugin) {
		const bool sync_to_host = _STATE->params[track->index][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0;
		return sync_to_host ? _DATA->hostTimeSnapshot.running == false : _STATE->params[track->index][LFO1STOPPED + index * LFONUMPARAMS].load() == 1.0;
	}
#endif
	return _STATE->params[track->index][LFO1STOPPED + index * LFONUMPARAMS].load() == 1.0;
}

bool LFO::syncmidi() const {
	return _STATE->params[track->index][LFO1SYNCMIDI + index * LFONUMPARAMS].load() == 1.0;
};


int32_t LFO::segments() const {
	return _STATE->params[track->index][LFO1NSEGS + index * LFONUMPARAMS].load();
};


int32_t LFO::editfunc() const {
	return _STATE->params[track->index][LFO1EDITFUNC + index * LFONUMPARAMS].load();
};

/*
bool LFO::syncing() const{
	return _STATE->params[track->index][LFO1CONTROLSACTIVE + index * LFONUMPARAMS].load() == 1.f;
}
*/

int32_t LFO::dest() const {
	return _STATE->params[track->index][LFO1DEST + index * LFONUMPARAMS].load();
};

MYFLOAT LFO::min(int32_t target) const {
	return _STATE->controls[track->index][target].lfo_min.load();
}


MYFLOAT LFO::max(int32_t target) const {
	return _STATE->controls[track->index][target].lfo_max.load();
}

void LFO::store(int32_t params, MYFLOAT value) const {
	auto parNum = LFO1CPS + params + index * LFONUMPARAMS;
	_STATE->params[track->index][parNum].store(value, std::memory_order_release);

}

std::atomic<MYFLOAT>* LFO::getpos0() const {
	return &_STATE->params[track->index][LFO1ENVX0 + index * LFONUMPARAMS];
};

std::atomic<MYFLOAT>* LFO::getval0() const {
	return &_STATE->params[track->index][LFO1ENVY0 + index * LFONUMPARAMS];
};

void LFO::reCompute() {
	if (_STATE->params[track->index][LFO1CPS + LFORECOMPUTE + index * LFONUMPARAMS].exchange(0.0) == 1.0) {
		const int32_t nsegs = segments();
		MYFLOAT x[32], y[32];

		for (int32_t i = 0; i <= nsegs; i++) {
			x[i] = *(getpos0() + i);
			y[i] = *(getval0() + i);
		}
		tsl::envelope::compute<MYFLOAT>[editfunc()](computeTable, WINDOW_SIZE, x,
			y, nsegs, true);
	}
}

void LFO::stop() const {
	_STATE->params[track->index][LFO1STOPPED + index * LFONUMPARAMS].store(1.0);
}

void LFO::start() const {
	_STATE->params[track->index][LFO1STOPPED + index * LFONUMPARAMS].store(0.0);
}


MYFLOAT LFO::inc() const {
	return (stopped() ||
		_STATE->params[track->index][LFO1SYNCMIDI + index * LFONUMPARAMS].load()) ? 0.0 :
		dir() *
		pow(10., _STATE->params[track->index][LFO1CPS + index * LFONUMPARAMS].load() * .05) * _STATE->onedsr;
}


double LFO::getSamples() {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	if (_DATA->isRunningAsPlugin && _STATE->params[track->index][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0) {
		// DAW-synced: the effective period comes from the host tempo, not LFO1CPS
		double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
		double periodBeats = _DATA->hostTimeSnapshot.beatsPerBar / CyclesPerBar();
		double samples = periodBeats * (60.0 / bpm) * _STATE->sr;
		return samples > 0 ? samples : 0;
	}
#endif
	return 1. / LOG2NORMALF(freq()) * _STATE->sr;
}


void LFO::rev() const {
	auto& param = _STATE->params[track->index][LFO1DIR + index * LFONUMPARAMS];

	double old = param.load(std::memory_order_relaxed);
	double desired;

	do
	{
		desired = (old == 0.0) ? 1.0 : 0.0;
	} while (!param.compare_exchange_weak(
		old,
		desired,
		std::memory_order_acq_rel,
		std::memory_order_relaxed));

}

MYFLOAT LFO::dir() const {
	return _STATE->params[track->index][LFO1DIR + index * LFONUMPARAMS].load() == 1.0 ? 1.0 : -1.0;
};

using namespace tsl::Syncing;
using namespace tsl::parameters;

SyncResult LFO::syncBySamples(double samples, std::vector<Event>& events) {
	auto tindex = track->index;
	SyncResult ret = 0;
	MYFLOAT syncfactor = syncFact();
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	if (_DATA->isRunningAsPlugin && _STATE->params[tindex][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0) {
		if (samples <= 0) {
			ret |= SyncFlags::LoopTooShort;
			return ret;
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

		// Clamp to parameter range
		while (loopsPerBar > 16.0)
			loopsPerBar /= syncfactor;
		while (loopsPerBar < 0.0625)
			loopsPerBar *= syncfactor;// Convert to normalized parameter (octave curve inverse)
		double loopsPerBarNormalized = (std::log2(loopsPerBar) + 4.0) / 8.0;
		loopsPerBarNormalized = std::clamp(loopsPerBarNormalized, 0.0, 1.0);

		auto timingId = LFO1SYNCDAWTIMING + index * 2;
		if (_STATE->params[tindex][timingId].exchange(loopsPerBarNormalized) != loopsPerBarNormalized) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = timingId;
			e.value = loopsPerBarNormalized;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else ret |= SyncFlags::UnChanged;

		_STATE->toUiThreadQueue.try_push([this, index = track->index]() {
			if (_STATE->active_track.load() == index && GETView(LFO1SYNCDAWTIMING)->visible_) {
				GETView(LFO1SYNCDAWTIMING)->redraw();
			}	});
		
	}
	else {
#endif
		double val = _STATE->sr / samples;

		while (val > LFO_FREQ_MAX)
			val /= syncfactor;
		while (val < LFO_FREQ_MIN)
			val *= syncfactor;
		// LFO1CPS is stored log-scaled (20*log10(Hz)), see inc()/control()
		auto des = LOG10D20F(val);
		auto paramIndex = LFO1CPS + index * LFONUMPARAMS;
		if (_STATE->params[tindex][paramIndex].exchange(des) != des) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = paramIndex;
			e.value = des;
			events.push_back(e);
			_STATE->toUiThreadQueue.try_push([this, tindex] {
				if (_STATE->active_track.load() == tindex &&
					GASMAIN == SPACE_LFOS &&
					GASLFO == index &&
					_STATE->parameters[LFO1CPS].view->visible_) {
					_STATE->parameters[LFO1CPS].view->redraw();
				}});
				ret |= SyncFlags::Ok;
				return ret;
		}
		else ret |= SyncFlags::UnChanged;


#if defined(PLUGIN_MODE) || defined(OS_IOS)
	}
#endif
	return ret;
}


SyncResult LFO::control(uint16_t todo, std::vector<Event>& events) {
	
	SyncResult ret{};
	
	auto tindex = track->index;
	
	MYFLOAT syncfactor = syncFact();

	switch (todo) {
	case SLOWButton:
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (_DATA->isRunningAsPlugin && _STATE->params[tindex][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0) {
			// DAW sync: divide loopsPerBar by syncfactor
			double loopsPerBarNorm = _STATE->params[tindex][LFO1SYNCDAWTIMING + index * 2].load();
			double loopsPerBar = _STATE->parameters[LFO1SYNCDAWTIMING + index * 2].toDisplay(_STATE->sr, loopsPerBarNorm);

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
			auto timingId = LFO1SYNCDAWTIMING + index * 2;
			double newNorm = std::clamp(_STATE->parameters[timingId].fromDisplay(_STATE->sr, loopsPerBar), 0.0, 1.0);
			if (_STATE->params[tindex][timingId].exchange(newNorm) != newNorm) {
				Event e;
				e.trackIndex = tindex;
				e.eventType = Eventtype::paramUpdate;
				e.paramIndex = timingId;
				e.value = newNorm;
				events.push_back(e);
				ret |= SyncFlags::Ok;
			}
			else ret |= SyncFlags::UnChanged;
			_STATE->toUiThreadQueue.try_push([this, index = track->index]() {
				if (_STATE->active_track.load() == index &&
					GETView(LFO1SYNCDAWTIMING)->visible_) {
					GETView(LFO1SYNCDAWTIMING)->redraw();
				}});
			return ret;
		}
		else 
#endif
		{
			MYFLOAT val = pow(10, freq() * .05) / syncfactor;
			if (val > LFO_FREQ_MAX) {
				ret |= SyncFlags::MaxReached;
				return ret;
			}
			else if (val < LFO_FREQ_MIN) {
				ret |= SyncFlags::MinReached;
				return ret;
			}
			else {
				auto paramIndex = LFO1CPS + index * LFONUMPARAMS;
				auto des = LOG10D20(val);
				if (_STATE->params[tindex][paramIndex].exchange(des) != des) {
					Event e;
					e.trackIndex = tindex;
					e.eventType = Eventtype::paramUpdate;
					e.paramIndex = paramIndex;
					e.value = des;
					events.push_back(e);
					_STATE->toUiThreadQueue.try_push([this, tindex] {
						const bool visible = _STATE->active_track.load() == tindex &&
							GASMAIN == SPACE_LFOS &&
							GASLFO == index && _STATE->parameters[LFO1CPS].view->visible_;
						if (visible) {
							_STATE->parameters[LFO1CPS].view->redraw();;
						}
						});
					ret |= SyncFlags::Ok;
					return ret;
				}

			}
		}

		break;
	case FASTButton:
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (_DATA->isRunningAsPlugin && _STATE->params[tindex][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0) {
			// DAW sync: multiply loopsPerBar by syncfactor
			double loopsPerBarNorm = _STATE->params[tindex][LFO1SYNCDAWTIMING + index * 2].load();
			double loopsPerBar = _STATE->parameters[LFO1SYNCDAWTIMING + index * 2].toDisplay(_STATE->sr, loopsPerBarNorm);

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
			auto timingId = LFO1SYNCDAWTIMING + index * 2;
			double newNorm = std::clamp(_STATE->parameters[timingId].fromDisplay(_STATE->sr, loopsPerBar), 0.0, 1.0);
			if (_STATE->params[tindex][timingId].exchange(newNorm) != newNorm) {
				Event e;
				e.trackIndex = tindex;
				e.eventType = Eventtype::paramUpdate;
				e.paramIndex = timingId;
				e.value = newNorm;
				events.push_back(e);
				ret |= SyncFlags::Ok;
			}
			else ret |= SyncFlags::UnChanged;
			_STATE->toUiThreadQueue.try_push([this, index = track->index]() {
				if (_STATE->active_track.load() == index &&
					GETView(LFO1SYNCDAWTIMING)->visible_) {
					GETView(LFO1SYNCDAWTIMING)->redraw();
				}});
			return ret;
		}
		else 
#endif
		{
			auto val = pow(10., freq() * .05) * syncfactor;
			if (val > LFO_FREQ_MAX) {
				ret |= SyncFlags::MaxReached;
				return ret;
			}
			else if (val < LFO_FREQ_MIN) {
				ret |= SyncFlags::MinReached;
				return  ret;
			}
			else {
				auto paramIndex = LFO1CPS + index * LFONUMPARAMS;
				auto des = LOG10D20(val);
				if (_STATE->params[tindex][paramIndex].exchange(des) != des) {
					Event e;
					e.trackIndex = tindex;
					e.eventType = Eventtype::paramUpdate;
					e.paramIndex = paramIndex;
					e.value = des;
					events.push_back(e);
					_STATE->toUiThreadQueue.try_push([this, tindex] {
						const bool visible = _STATE->active_track.load() == tindex &&
							GASMAIN == SPACE_LFOS &&
							GASLFO == index && _STATE->parameters[LFO1CPS].view->visible_;
						if (visible) {
							_STATE->parameters[LFO1CPS].view->redraw();;
						}
						});
					ret |= SyncFlags::Ok;
					return ret;
				}

			}
		}
		break;
	case DIRButton:{
		auto id = LFO1DIR + index * LFONUMPARAMS;
		auto& param = _STATE->params[track->index][LFO1DIR + index * LFONUMPARAMS];

		double old = param.load(std::memory_order_relaxed);
		double desired;

		do
		{
			desired = (old == 0.0) ? 1.0 : 0.0;
		} while (!param.compare_exchange_weak(
			old,
			desired,
			std::memory_order_acq_rel,
			std::memory_order_relaxed));
		Event e;
		e.trackIndex = tindex;
		e.eventType = Eventtype::paramUpdate;
		e.paramIndex = id;
		e.value = desired;
		events.push_back(e);
		ret |= SyncFlags::Ok;
		return ret;
	}
	
	case STEPBACK: {
		auto id = LFO1PHS + index * LFONUMPARAMS;
		if (_STATE->params[track->index][id].exchange(0.0) != 0.0) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = id;
			e.value = 0.0;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else ret |= SyncFlags::UnChanged;
		return ret;
	}
	case STOPButton: {
		auto id = LFO1STOPPED + index * LFONUMPARAMS;
		if (_STATE->params[track->index][id].exchange(1.0) != 1.0) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = id;
			e.value = 1.0;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else ret |= SyncFlags::UnChanged;
		return ret;
	}
	
	case PLAYButton:{
		auto id = LFO1STOPPED + index * LFONUMPARAMS;
		if (_STATE->params[track->index][id].exchange(0.0) != 0.0) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = id;
			e.value = 0.0;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else ret |= SyncFlags::UnChanged;
		return ret;
	}
	case STEPFORW: {
		auto id = LFO1PHS + index * LFONUMPARAMS;
		if (_STATE->params[track->index][id].exchange(1.0) != 1.0) {
			Event e;
			e.trackIndex = tindex;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = id;
			e.value = 1.0;
			events.push_back(e);
			ret |= SyncFlags::Ok;
		}
		else ret |= SyncFlags::UnChanged;
		return ret;
	}
	default:
		break;
	}
	return ret;
}


MYFLOAT lfo_user(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return lfo->computeTable[PHS2INT(phase)] * range;
}


MYFLOAT lfo_sine_add(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return lfo->env[0][PHS2INT(phase)] * range;
}

MYFLOAT lfo_sine_sub(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return (.5f - .5f * cosf(TWOPI_F_P * phase)) * -range;
}

MYFLOAT lfo_sine_direct(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return (.5f - .5f * cos(TWOPI_F_P * phase)) * range;
}

MYFLOAT lfo_ramp_add(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return phase * range;
}

MYFLOAT lfo_ramp_sub(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return -phase * range;
}

// x = m - abs(i % (2*m) - m)


MYFLOAT lfo_triangle_add(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return lfo->env[1][PHS2INT(phase)] * range;
}


MYFLOAT lfo_triangle_sub(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return phase < .5f ? phase * -2 * range : -2 * (1.f - phase) * range;
}

MYFLOAT lfo_pulse_add(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return phase < 0.5f ? 0.0f : 1.0f * range;
}

MYFLOAT lfo_pulse_sub(LFO* lfo, MYFLOAT phase, MYFLOAT range) {
	return phase < 0.5f ? 0.0f : -1.0f * range;
}

MYFLOAT LFO::phinc() const {
	return _STATE->params[track->index][LFO1RND + index].load() == 1.f ? _randomPhaseInc : inc();
}

void LFO::getNextFreq() {
	MYFLOAT fa = _prevRndCpsA;
	MYFLOAT fb = _prevRndCpsB;
	MYFLOAT min = fa < fb ? fa : fb;
	MYFLOAT dist = DISTANCEF(fa, fb);
	MYFLOAT val = min + randGab * dist;
	_randomPhaseInc = LOG2NORMALF(val) * _STATE->onedsr;
}

void
LFO::getNextRndTarget(const int32_t type, MYFLOAT& a, MYFLOAT& b, const MYFLOAT r, MYFLOAT& vel, MYFLOAT& accel) {
	switch (type) {
	case 0:
		a = b;
		b = b == 0.f ? 1.f : 0.f;
		break;
	case 1:
		a = b;
		b = _DATA->random.betarand(1.f, LOG2NORMALF(_STATE->params[track->index][LFO1RNDALPHA + index].load()), LOG2NORMALF(_STATE->params[track->index][LFO1RNDBETA + index].load()));
		break;
		/*
	case 2:
		if (a < b) {
			a = b;
			b *= _DATA->random.betarand(1.f, _STATE->params[track->index][LFO1RNDALPHA+index].load(), _STATE->params[track->index][LFO1RNDBETA+index].load());
		} else {
			a = b;
			b += (1.f - b) * _DATA->random.betarand(1.f, _STATE->params[track->index][LFO1RNDALPHA+index].load(), _STATE->params[track->index][LFO1RNDBETA+index].load());
		}
		break;*/
	case 2: {
		MYFLOAT val = b + r * BiRandGab;
		if (val > 1.0) val -= (val - 1.f);
		else if (val < 0)
			val *= -1.f;
		a = b;
		b = val;
	}
		  break;
	case 3: {
		vel += r * BiRandGab;
		vel *= (1.f - _randomPhaseInc * 500);
		MYFLOAT val = b + vel;
		if (val > 1.f) {
			val -= (val - 1.f);
			vel *= -1.f;
		}
		else if (val < 0) {
			val *= -1.f;
			vel *= -1.f;
		}
		a = b;
		b = val;
	}
		  break;
	case 4: {
		accel += r * BiRandGab;
		vel += accel;
		vel *= (1.f - _randomPhaseInc * 500);
		MYFLOAT val = b + vel;
		if (val > 1.f) {
			val -= (val - 1.f);
			vel *= -1.f;
			accel = 0.f;//*= -1.f;
		}
		else if (val < 0) {
			val *= -1.f;
			vel *= -1.f;
			accel = 0.f;//*= -1.f;
		}
		a = b;
		b = val;
	}
	default:;
	};
	//LOGE("%f %f %f %f", b, vel, accel, r);
}


void LFO::getNextRndTarget() {
	const MYFLOAT r = LOG2NORMALF(_STATE->params[track->index][LFO1RNDDEPTH + index].load());
	if (_oldR != r) {
		_oldR = r;
		_vel = _accel = 0;
	}
	const int32_t rndtype = (int)_STATE->params[track->index][LFO1RNDTYPE + index];

	getNextRndTarget(rndtype, _a, _b, r, _vel, _accel);
	_prevRndCurve = _STATE->params[track->index][LFO1RNDCURVE + index].load();
}

#if defined(PLUGIN_MODE) || defined(OS_IOS)
MYFLOAT LFO::CyclesPerBar() const {
	return _STATE->parameters[LFO1SYNCDAWTIMING + index * 2].toDSP(_STATE->sr, _STATE->params[track->index][LFO1SYNCDAWTIMING + index * 2].load());
};

bool LFO::DetectLfoDiscontinuity()
{
	const double expectedAdvance =
		(double)_STATE->currentBufSize *
		(_DATA->hostTimeSnapshot.bpm / 60.0) /
		_STATE->sr;

	const double actualAdvance =
		_DATA->hostTimeSnapshot.ppqNow -
		_DATA->hostTimeSnapshot.lastPpqPos;

	const double epsilon = expectedAdvance * 0.25;

	if (_DATA->hostTimeSnapshot.ppqNow <
		_DATA->hostTimeSnapshot.lastPpqPos)
		return true; // loop / rewind

	if (_DATA->hostTimeSnapshot.running &&
		!_DATA->hostTimeSnapshot.wasRunning)
		return true; // transport start

	if (fabs(actualAdvance - expectedAdvance) > epsilon)
		return true; // seek / tempo jump

	return false;
}

#endif

void LFO::setUpBuffer() {
	const int32_t size = _STATE->currentBufSize;
	powertmp = power();
	if (powertmp) {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		auto dawSync = _DATA->isRunningAsPlugin && _STATE->params[track->index][LFO1SYNCDAWTRANSPORT + index * 2].load() == 1.0 && _DATA->hostTimeSnapshot.running;
		double periodBeats = 1.0;
#endif

		const int32_t function = func();
		auto phaseId = LFO1PHS + index * LFONUMPARAMS;
		MYFLOAT phase = _STATE->params[track->index][phaseId].load();
		//const bool rnd = _STATE->params[track->index][LFO1RND + index].load() == 1.f;
		const bool sync =
#if defined(PLUGIN_MODE) || defined(OS_IOS)
			!dawSync &&
#endif
			syncmidi()
			;
		const bool stoptmp = stopped();
		MYFLOAT phinc = 0;
		if (function == LFO_RND) {
			const MYFLOAT cpsa = _STATE->params[track->index][LFO1CPSMIN + index];
			const MYFLOAT cpsb = _STATE->params[track->index][LFO1CPSMAX + index];
			if (_prevRndCpsA != cpsa || _prevRndCpsB != cpsb) {
				_prevRndCpsA = cpsa;
				_prevRndCpsB = cpsb;
				getNextFreq();
			}
			if (_prevFunc != LFO_RND) {
				_a = _b = functions[_prevFunc](this, phase, 1.f);
				phase = 0;
				getNextRndTarget();
				getNextFreq();
				init = true;
			}
			/*
			else{
				MYFLOAT cpsA = _STATE->params[track->index][LFO1CPSMIN+index].load();
				MYFLOAT cpsB = _STATE->params[track->index][LFO1CPSMAX+index].load();
				MYFLOAT curve = _STATE->params[track->index][LFO1RNDCURVE+index].load();
				MYFLOAT rndType = _STATE->params[track->index][LFO1RNDTYPE+index].load();
				//MYFLOAT cpsA = _STATE->params[track->index][LFO1CPSMIN].load();
				if(_prevRndCPSA != cpsA || _prevRNDCPSB != cpsB || _prevRndCurve != curve || _prevRndDistr != rndType){
					_prevRndCPSA = cpsA;
					_prevRNDCPSB = cpsB;
					_prevRndCurve = curve;
					_prevRndDistr = rndType;
				}
			}*/

			int32_t i = 0;
			while (i < size) {
				phinc = (stoptmp || sync ? 0.f : _randomPhaseInc);
				if (_prevRndCurve == 0) {
					MYFLOAT inflect = (_a + _b) * .5f;
					MYFLOAT extreme;
					MYFLOAT diffd2;
					if (phase < .5f) {
						extreme = _a;
						diffd2 = (inflect - extreme) * .5f;

						for (; i < size; i++) {
							MYFLOAT y = phase * 2.f;
							MYFLOAT val = ((3.0f) - y) * y * y * diffd2 + extreme;
							if (val < 0)
								val = 0;
							else if (val > 1.f)
								val = 1.f;
							buf[i] = val;
							phase += phinc;
							if (phase >= .5f)
								break;
						}
					}
					if (i < size && phase >= .5f) {
						extreme = _b;
						diffd2 = (inflect - extreme) * (0.5f);
						for (; i < size; i++) {
							const MYFLOAT y = 1.f - (phase - .5f) * 2.f;
							MYFLOAT val = ((3.0f) - y) * y * y * diffd2 + extreme;
							if (val < 0)
								val = 0;
							else if (val > 1.f)
								val = 1.f;
							buf[i] = val;
							phase += phinc;
							if (phase >= 1.f) {
								getNextRndTarget();
								getNextFreq();
								phase -= 1.f;
								break;
							}
						}
					}
				}
				else if (_prevRndCurve == 1) {
					const MYFLOAT dist = DISTANCEF(_b, _a);
					const MYFLOAT multi = _a > _b ? -1 : 1;
					for (; i < size; i++) {
						buf[i] = _a + multi * dist * phase;
						phase += phinc;
						if (phase >= 1.f) {
							getNextRndTarget();
							getNextFreq();
							phase -= 1.f;
							break;
						}
					}
				}
				else if (_prevRndCurve == 2) {
					for (; i < size; i++) {
						buf[i] = _b;
						phase += phinc;
						if (phase >= 1.f) {
							getNextRndTarget();
							getNextFreq();
							phinc = stoptmp || sync ? 0.f : _randomPhaseInc;
							phase -= 1.f;
						}
					}
				}
				else {
					for (; i < size; i++) {
						MYFLOAT f0 = num0;
						if (init) {
							init = false;
							goto next;
						}

						phase += phinc;
						if (phase >= 1.f) {
							getNextFreq();
							getNextRndTarget();
							phase -= 1.f;
							phinc = stoptmp || sync ? 0.f : _randomPhaseInc;
						next:
							f0 = num0 = num1;
							MYFLOAT f1 = num1 = num2;
							MYFLOAT f2 = num2 = (2 * _b) - 1.f;
							df0 = df1;
							df1 = (f2 - f0) * 0.5f;
							MYFLOAT slope = f1 - f0;
							MYFLOAT resd0 = df0 - slope;
							MYFLOAT resd1 = df1 - slope;
							c3 = resd0 + resd1;
							c2 = -(resd1 + 2.0f * resd0);
						}

						MYFLOAT x = phase;
						MYFLOAT ret = (((c3 * x + c2) * x + df0) * x + f0) * 1.f;
						if (ret > 1.f)
							ret = 1.f;
						else if (ret < 0)
							ret = 0;
						buf[i] = ret;
					}
				}
			}
		}
		else {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
			if (dawSync)
			{
				double bpm = _DATA->hostTimeSnapshot.bpm;
				double sr = _STATE->sr;

				double beatsPerSample = (bpm / 60.0) / sr;
				auto cyclesPerBar = CyclesPerBar();
				periodBeats = _DATA->hostTimeSnapshot.beatsPerBar / cyclesPerBar;
				phase = std::fmod(_DATA->hostTimeSnapshot.ppqNow / periodBeats, 1.0);
				// authoritative phase from host
				auto d = dir();
				if (d < 0)
					phase = 1.0 - phase;
				// per-sample phase increment (derived, not accumulated)
				phinc = beatsPerSample / periodBeats;
				phinc *= d;
			}
			else
			{
				phinc = inc();
			}
#else
			phinc = inc();
#endif // PLUGIN_MODE || OS_IOS


			if (_prevFunc != function) {
				MYFLOAT val = buf[size - 1];
				if (function == 0) {
					MYFLOAT tmp = acos(-2 * val + 1) / (TWOPI_F_P);
					phase = phase > .5f ? 1.f - tmp : tmp;
				}
				else if (function == 1) {
					MYFLOAT tmp = val * .5f;
					phase = phase > .5f ? 1.f - tmp : tmp;
				}
				else if (function == 2) {
					phase = val;
				}
				else if (function == 3) {
					;
				}
				else if (function == 4) {
					MYFLOAT phasetmp = 0;
					MYFLOAT dist = DISTANCEF(val, computeTable[0]);
					for (int32_t i = 1; i < WINDOW_SIZE; i++) {
						MYFLOAT disttmp = DISTANCEF(val, computeTable[i]);
						if (disttmp < dist || (disttmp == dist && DISTANCEF((MYFLOAT)i / (MYFLOAT)WINDOW_SIZE, phase) < DISTANCEF(phasetmp, phase))) {
							phasetmp = (MYFLOAT)i / (MYFLOAT)WINDOW_SIZE;
							dist = disttmp;
						}
					}
					phase = phasetmp;
				}
				// LOGE("%f %f %f", val, phase, functions[function](this, phase, 1.f));
			}
			LFO* lfocps = track->lfo[LFO1CPS + index * LFONUMPARAMS].load();
			if (
#if defined(PLUGIN_MODE) || defined(OS_IOS)
				!dawSync &&
#endif
				phinc != 0 && lfocps != nullptr && lfocps->power()) {
				MYFLOAT a = LOG2NORMALF(
					_STATE->controls[track->index][LFO1CPS + index * LFONUMPARAMS].lfo_min.load());
				MYFLOAT b = LOG2NORMALF(
					_STATE->controls[track->index][LFO1CPS + index * LFONUMPARAMS].lfo_max.load());
				MYFLOAT range = DISTANCEF(a, b);
				MYFLOAT start = std::min(a, b);
				for (int32_t i = 0; i < size; i++) {
					buf[i] = functions[function](this, phase, 1.f);
					phase += ((start + range * lfocps->buf[i]) * _STATE->onedsr);
					if (phase >= 1.0) {
						phase -= 1.0;
					}
					else if (phase < 0) {
						phase += 1.0;
					}
				}
			}
			else {
				for (int32_t i = 0; i < size; i++) {
					buf[i] = functions[function](this, phase, 1.f);
					phase += phinc;
					if (phase >= 1.0) {
						phase -= 1.0;
					}
					else if (phase < 0) {
						phase += 1.0;
					}
				}
			}
		}
		auto tmpPhase = phase;
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (dawSync)
		    phase = std::fmod(_DATA->hostTimeSnapshot.ppqNow / periodBeats, 1.0);
#endif
		if (_STATE->params[track->index][phaseId].exchange(tmpPhase) != tmpPhase) {
			_DATA->snapShot.queue.try_push(Event::createEvent(track->index, Eventtype::paramUpdate, phaseId, tmpPhase));
		}
		
		_prevFunc = function;

		const int32_t samples = (int)((MYFLOAT)size * .02f);
		const MYFLOAT readinc = (MYFLOAT)size / (MYFLOAT)samples;
		for (int32_t i = 0; i < samples; i++) {
			int32_t readpos = (int)(i * readinc);
			if (readpos >= size)
				readpos = size - 1;
			_internalDrawBuf[_writeoffset] = buf[readpos];
			_writeoffset++;
			if (_writeoffset >= LFO_TBL_SIZE)
				_writeoffset = 0;
		}
		MYFLOAT* buf = nullptr;
		if (updateRenderThread.load() && drawBuf.load(std::memory_order_acquire) == nullptr && (buf = _STATE->pool.acquire<MYFLOAT>(LFO_TBL_SIZE)) != nullptr) {
			auto tmpoff = _writeoffset - LFO_TBL_SIZE;
			if (tmpoff < 0)
				tmpoff += LFO_TBL_SIZE;
			for (int32_t i = 0; i < LFO_TBL_SIZE; i++) {
				buf[i] = _internalDrawBuf[(tmpoff++) & (LFO_TBL_SIZE - 1)];
			}
			drawBuf.store(buf, std::memory_order_release);
		}
	}
}