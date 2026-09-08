//
// Created by pr on 27.12.22.
//

#include "graingenerator.h"
#include "grainstorm.h"
#include "defines.h"
#include "track.h"
#include "app.h"

// The figuration algorithms (CHIMES / FIGURE) speak major pentatonic -- the
// classic wind-chime tuning, and the one set that stays consonant over
// arbitrary source material. Degree -> semitones above the base pitch.
static inline MYFLOAT pentaSemis(int32_t deg) {
	static constexpr int32_t penta[5] = { 0, 2, 4, 7, 9 };
	if (deg < 0) deg = 0;
	return (MYFLOAT)(12 * (deg / 5) + penta[deg % 5]);
}

// How many pentatonic degrees fit inside a span of semitones (>= 1).
static inline int32_t pentaDegreesIn(MYFLOAT spanSemis) {
	int32_t n = 1;
	while (n < 64 && pentaSemis(n) <= spanSemis) n++;
	return n;
}

graingenerator::graingenerator(TRACK* t, tsl::AppState *appState) : follower(appState, t->index, GRAINGENFOLDENSA, 0, 0, 0, 0, 0) {
	_track = t;
    follower.setKrate(true);
    follower.envpower.store(true);
}

int graingenerator::tick(int chan, int smpl, MYFLOAT &seqgain, MYFLOAT &seqpitch, MYFLOAT &seqsize) {
	auto fire = buf[chan][smpl];
	if (fire == 1) {
		_track->grainsequencer.tick(chan, seqgain, seqpitch, seqsize);
		seqgain *= gainBuf[chan][smpl];
		seqpitch *= pitchBuf[chan][smpl];
		seqsize *= sizeBuf[chan][smpl];
	}
	return fire;
}

void graingenerator::reset() {
	auto _appState = _track->_appState;

	for (auto chan = 0; chan < _STATE->channels; chan++)
		_count[chan] = 0;
	_DATA->snapShot.queue.try_push(getState());
}

void graingenerator::tickPriv(int chan, int smpl) {
	auto _appState = _track->_appState;
	if (_dodenslfo) {
		auto graindens = dens_min + dens_lfo->buf[smpl] * dens_range;
		setFreq(graindens, chan);
		//step_length_file = step_length * playbackspeed;
	}

	if (_dodeviationlfo) {
		setDev(deviation_a + deviation_lfo->buf[smpl] * devation_range, chan);
	}

	if (_alg == 3 && ++kcount[chan] == 64) {
		kcount[chan] = 0;
		nextcount[chan] = (int)spline[chan].tick();
	}
	else if (_alg == 4 && ++kcount[chan] == 64) {
		kcount[chan] = 0;
		const MYFLOAT dens = chan == 0 ? follower.detectL(buf[chan][smpl] * CONVMYFLT)
			: follower.detectR(buf[chan][smpl] * CONVMYFLT);
		nextcount[chan] = clampPeriod(_STATE->sr / (dens > 0.001 ? dens : 0.001));
    }
	else if (_alg == 5 && ++kcount[chan] == 64) {
		kcount[chan] = 0;
		auto& c = _chime[chan];
		// The spline is smooth 0..1 weather; the WIND knob bends its
		// distribution (low = mostly calm with rare gusts, high = mostly
		// blowing). Splines can overshoot their control points, so clamp first.
		MYFLOAT raw = spline[chan].tick();
		if (raw < 0.) raw = 0.; else if (raw > 1.) raw = 1.;
		const MYFLOAT w = _STATE->params[_track->index][GRAINGENCHIMEWIND].load();
		c.wind = (MYFLOAT)pow((double)raw, exp((0.5 - w) * 3.));
		// A gust must be able to interrupt a lull: shorten the pending count
		// when the wind now implies a shorter period. Never lengthen -- the
		// sparseness after a gust arrives by itself with the next draw.
		const double rate = _prvfreq[chan] * pow((double)(c.wind < 0.02 ? 0.02 : c.wind), 3.);
		const int32_t implied = clampPeriod(_STATE->sr / rate);
		if (implied < _count[chan]) _count[chan] = implied;
	}
	

	if (--_count[chan] <= 0) {
		double period = _minPeriod;
		// All three are multiplied into the grain, so 1 is neutral. Written
		// before the switch so the figuration algorithms can overwrite them.
		sizeBuf[chan][smpl] = 1.f;
		gainBuf[chan][smpl] = 1.f;
		pitchBuf[chan][smpl] = 1.f;
		switch (_alg) {
		case 4:
		case 3: {
			period = nextcount[chan];
			break;
		}
		case 0: {
			auto nextsamps = (int32_t)(_STATE->sr / _prvfreq[chan]);
			period = (_minPeriod + DISTANCE(nextsamps, _minPeriod) * (1 - BiRandGab1 * _dev[chan])) * _swingval[swingstep[chan]];
			break;
		}
		case 1: {
			auto r1 = randGab1;
			auto r2 = randGab1;
			if (r1 < 1e-9) r1 = 1e-9;    // log(0) is -inf and poisons the period
			auto nextcount = sqrt(-2.0 * log(r1)) * sin(r2 * TWOPI_P);
			if (nextcount < -1.0) {
				auto diff = -1.0 - nextcount;
				nextcount = (1.0 < -1.0 + diff ? 1.0 : -1.0 + diff);
			}
			else if (nextcount > 1.0) {
				auto diff = nextcount - 1.0;
				nextcount = (-1.0 > 1.0 - diff ? -1.0 : 1.0 - diff);
			}
			auto nextsamps = (int32_t)(_STATE->sr / _prvfreq[chan]);
			period = nextsamps + nextcount * _dev[chan] * nextsamps;
			break;
		}
		case 2: {
			auto& h = _height[chan];
			auto& rev = _rising[chan];
			auto a = LOG2NORMAL(
				_STATE->params[_track->index][GRAINGENHEIGHTA].load()), b = LOG2NORMAL(
					_STATE->params[_track->index][GRAINGENHEIGHTB].load()), vel = LOG2NORMAL(
						_STATE->params[_track->index][GRAINGENVEL].load());
			if (b > a)std::swap(a, b);
			if (vel < 0.01) vel = 0.01;
			if (rev) {
				if (h >= a) {
					rev = false;
					h *= .75;
				}
				else {
					h /= .75;
				}
				period = (2. * h / vel + 0.02) * _STATE->sr;
			}
			else {
				if (h <= b) {
					// HOLD wins over LOOP: an explicit "stay at the floor" beats
					// the loop's bounce-back — with the old precedence the
					// checkbox was inaudible from any looping patch. Unchecking
					// HOLD mid-roll releases the ball back into LOOP or restart.
					const auto hold = _STATE->params[_track->index][GRAINGENREST] == 1.;
					if (hold) {
						h = b + tsl::random::randomfloat(0, 0.02);
						period = (2. * h / vel + 0.02) * _STATE->sr;
					}
					else if (_STATE->params[_track->index][GRAINGENLOOP].load() == 1.) {
						rev = true;
						h /= .75;
						period = (2. * h / vel + 0.02) * _STATE->sr;
					}
					else {
						h = a;
						period = h / vel * _STATE->sr;
					}
				}
				else {
					h *= .75;
					period = (2. * h / vel + 0.02) * _STATE->sr;
				}
			}
			break;
		}
		case 5: {
			// CHIMES -- one wind field drives density, energy, register and
			// ring together; the correlation is what reads as weather rather
			// than dice.
			auto& c = _chime[chan];
			const int32_t ntubes = pentaDegreesIn(
				_STATE->params[_track->index][GRAINGENCHIMESPAN].load());
			// Light air walks the clapper between neighbouring tubes; a gust
			// throws it anywhere. The walk is what makes lulls read as melody.
			if (randGab1 < c.wind) {
				c.tube = (int32_t)(randGab1 * ntubes);
			}
			else {
				const auto r = randGab1;
				c.tube += r < .33 ? -1 : r < .66 ? 0 : 1;
			}
			if (c.tube < 0) c.tube = 0;
			else if (c.tube >= ntubes) c.tube = ntubes - 1;
			// Strike energy rides the wind, so lull strikes are rare AND soft.
			MYFLOAT energy = c.wind * tsl::random::randomfloat(0.55, 1.0);
			if (c.wind > 0.6 && randGab1 < 0.08) energy = 1.;   // gust accent
			energy = 0.10 + 0.90 * energy;
			pitchBuf[chan][smpl] = (MYFLOAT)pow(2., pentaSemis(c.tube) / 12.);
			gainBuf[chan][smpl] = (MYFLOAT)pow((double)energy, 0.7);
			// Soft strikes ring shorter. Never above 1: seqsize > 1 would read
			// past the grain buffer (the sequencer's own SIZE stops at 1 too).
			sizeBuf[chan][smpl] = 0.6 + 0.4 * energy;
			const double rate = _prvfreq[chan] *
				pow((double)(c.wind < 0.02 ? 0.02 : c.wind), 3.);
			// Log-uniform jitter keeps the strikes uneven at any rate.
			period = _STATE->sr / rate * exp(tsl::random::randomfloat(-0.9, 0.6));
			break;
		}
		case 6: {
			// FIGURE -- whole pentatonic gestures: runs, arps, zigzag lace,
			// cascades. Contour, length and curve are decided when the figure
			// starts, then it plays out; DENSITY is the note rate.
			auto& f = _fig[chan];
			const int32_t maxdeg = pentaDegreesIn(
				_STATE->params[_track->index][GRAINGENFIGRANGE].load()) - 1;
			if (f.left <= 0) {
				int32_t shape = (int32_t)_STATE->params[_track->index][GRAINGENFIGSHAPE].load();
				if (shape == 0) shape = 1 + (int32_t)(randGab1 * 4.);   // ANY
				if (shape > 4) shape = 4;
				f.shape = shape;
				f.left = (int32_t)(_STATE->params[_track->index][GRAINGENFIGNOTES].load() *
					tsl::random::randomfloat(0.75, 1.3) + 0.5);
				if (shape == 4) f.left = (int32_t)(f.left * 1.4);  // cascades fall longer
				if (f.left < 2) f.left = 2;
				f.u = 0.;
				f.du = 1. / (MYFLOAT)(f.left - 1);
				f.stepmul = 1.;
				// Accel straddles 1 -- a constant step is an arpeggiator.
				// Cascades always slow as they fall.
				f.accel = shape == 4 ? tsl::random::randomfloat(1.03, 1.16)
					: tsl::random::randomfloat(0.94, 1.10);
				f.dir = randGab1 < 0.25 ? f.dir : -f.dir;
				if (shape == 4) f.dir = -1;
				f.deg = f.dir > 0 ? (int32_t)(randGab1 * (maxdeg / 3 + 1))
					: maxdeg - (int32_t)(randGab1 * (maxdeg / 3 + 1));
				if (shape == 3) {
					f.deg = maxdeg / 2;
					f.upA = 1 + (int32_t)(randGab1 * 3.);
					do { f.dnB = 1 + (int32_t)(randGab1 * 3.); } while (f.dnB == f.upA);
				}
			}
			if (f.deg < 0) f.deg = 0;
			else if (f.deg > maxdeg) f.deg = maxdeg;   // RANGE may have moved
			pitchBuf[chan][smpl] = (MYFLOAT)pow(2., pentaSemis(f.deg) / 12.);
			// Runs, arps and cascades fade out so they dissolve rather than
			// stop; the zigzag lace arches.
			const MYFLOAT ashape = f.shape == 3 ? 0.55 + 0.45 * sin(PI_P * f.u)
				: 1. - 0.72 * f.u * f.u;
			gainBuf[chan][smpl] = ashape * tsl::random::randomfloat(0.85, 1.08);
			switch (f.shape) {
			case 1: f.deg += f.dir; break;                                  // RUN
			case 2: f.deg += f.dir * (randGab1 < 0.3 ? 3 : 2); break;       // ARP
			case 3: f.deg += (f.left & 1) ? f.upA : -f.dnB; break;          // ZIGZAG
			default: f.deg += f.dir; break;                                 // CASCADE
			}
			// Fold the contour back into the range instead of ending it there.
			if (f.deg > maxdeg) { f.deg = maxdeg - (f.deg - maxdeg); f.dir = -f.dir; }
			else if (f.deg < 0) { f.deg = -f.deg; f.dir = -f.dir; }
			if (--f.left > 0) {
				period = _STATE->sr / _prvfreq[chan] * f.stepmul *
					tsl::random::randomfloat(0.85, 1.18);
				f.stepmul *= f.accel;
				f.u += f.du;
			}
			else {
				period = LOG2NORMAL(_STATE->params[_track->index][GRAINGENFIGREST].load()) *
					tsl::random::randomfloat(0.6, 1.6) * _STATE->sr;
			}
			break;
		}
		case 7: {
			// GLISS -- phase-continuous sweeps across grains. The grain stream
			// runs at DENSITY; TIME is the sweep, and gain is coupled to the
			// phase so the wrap modes stay seamless.
			auto& g = _gliss[chan];
			const int32_t mode = (int32_t)_STATE->params[_track->index][GRAINGENGLSMODE].load();
			const MYFLOAT range = _STATE->params[_track->index][GRAINGENGLSRANGE].load();
			const MYFLOAT T = LOG2NORMAL(_STATE->params[_track->index][GRAINGENGLSTIME].load());
			if (g.resting) { g.resting = false; g.phase = 0.; }
			const MYFLOAT x = g.phase;
			MYFLOAT semis, amp = 1.;
			switch (mode) {
			case 0: {  // WANDER: smoothstep between chained targets, no rests
				const MYFLOAT s = x * x * (3. - 2. * x);
				semis = g.from + (g.to - g.from) * s;
				amp = tsl::random::randomfloat(0.92, 1.0);
				break;
			}
			case 1: semis = (x - 0.5) * range; break;   // UP, centred on the
			case 2: semis = (0.5 - x) * range; break;   // played pitch
			case 3: semis = 0.5 * range * sin(PI_P * x); break;          // ARCH
			case 4: semis = 0.5 * range * exp(-4.6 * x); break;          // FALL
			case 5: semis = (x - 0.5) * range; break;                    // RISSET UP
			default: semis = (0.5 - x) * range; break;                   // RISSET DN
			}
			if (mode >= 1 && mode <= 4) {
				// Fade the edges so the jump back reads as a phrase, not a click.
				MYFLOAT e = x < 0.12 ? x / 0.12 : x > 0.88 ? (1. - x) / 0.12 : 1.;
				if (e < 0.) e = 0.;
				amp = 0.15 + 0.85 * (0.5 - 0.5 * cos(PI_P * e));
			}
			else if (mode >= 5) {
				// The Risset loudness bell: silent exactly where the octave
				// wraps, which is what hides the seam of the endless glide.
				const MYFLOAT s = sin(PI_P * x);
				amp = s * s;
			}
			pitchBuf[chan][smpl] = (MYFLOAT)pow(2., semis / 12.);
			gainBuf[chan][smpl] = amp;
			period = _STATE->sr / _prvfreq[chan] * tsl::random::randomfloat(0.95, 1.06);
			g.phase += (MYFLOAT)(period / (T * _STATE->sr));
			if (g.phase >= 1.) {
				if (mode == 0) {
					g.from = g.to;
					g.to = tsl::random::randomfloat(-0.5, 0.5) * range;
					g.phase = 0.;
				}
				else if (mode >= 5) {
					// TIME can be shorter than one grain period, so the phase
					// may overshoot by whole wraps -- drop them all, or it
					// grows without bound and pins at the engine's pitch clamp.
					g.phase -= (MYFLOAT)(int32_t)g.phase;
				}
				else {
					period = LOG2NORMAL(_STATE->params[_track->index][GRAINGENGLSREST].load()) *
						tsl::random::randomfloat(0.6, 1.5) * _STATE->sr;
					g.resting = true;
				}
			}
			break;
		}
		default:
			break;
		}
		_count[chan] = clampPeriod(period);
		if (++swingstep[chan] > 1)
			swingstep[chan] = 0;
		buf[chan][smpl] = 1;
	}
	else {
		buf[chan][smpl] = 0;
		sizeBuf[chan][smpl] = 1.f;
		gainBuf[chan][smpl] = 1.f;
		pitchBuf[chan][smpl] = 1.f;
	}
}

void graingenerator::prepare() {
	auto _appState = _track->_appState;
	_alg = _STATE->params[_track->index][ASGRAINGEN].load();
	dens_lfo = _track->lfo[DENSITY];
	_dodenslfo = dens_lfo && dens_lfo->power();
	const bool mono = _STATE->params[_track->index][GRAINGENMONO] == 1.0;
	auto swing = _STATE->params[_track->index][ARP_SWING].load();
	_swingval[0] = 1 - (1 - 1 / 1.5) * swing;
	_swingval[1] = 1 + swing;
	_minPeriod = (int32_t)(_STATE->sr / _STATE->parameters[DENSITY].max);
	if (_minPeriod < 1)
		_minPeriod = 1;
	// Long enough for the slowest density any algorithm can legitimately ask for,
	// short enough that a poisoned period cannot park the generator for good.
	_maxPeriod = (int32_t)(_STATE->sr * 60.);
	if (_oldalg != _alg) {
		if (_alg == 3) {
			for (int32_t i = 0; i < _STATE->channels; i++) {
				spline[i].reset();
				kcount[i] = 63;
			}
		}
		else if (_alg == 4) {
    		for (int32_t i = 0; i < _STATE->channels; i++) {
				kcount[i] = 63;
			}
		}
		else if (_alg == 5) {
			// CHIMES borrows the spline as its wind field; the algorithms are
			// exclusive, so SPLINE's own use of it can never be interleaved.
			for (int32_t i = 0; i < _STATE->channels; i++) {
				spline[i].reset();
				kcount[i] = 63;
				_chime[i] = ChimeState{};
			}
		}
		else if (_alg == 6) {
			for (int32_t i = 0; i < _STATE->channels; i++)
				_fig[i] = FigureState{};
		}
		else if (_alg == 7) {
			for (int32_t i = 0; i < _STATE->channels; i++)
				_gliss[i] = GlissState{};
		}
		// The previous algorithm's countdown can be parked for up to 60 s (a
		// GLISS rest, a CHIMES lull) — without this, the new algorithm
		// inherits the wait and a channel sits silent (seen GLISS -> FOLLOW).
		for (int32_t i = 0; i < _STATE->channels; i++)
			_count[i] = 0;
		_oldalg = _alg;
	}


	if (_dodenslfo) {
        auto a = _STATE->controls[_track->index][DENSITY].lfo_min.load(), b =
        _STATE->controls[_track->index][DENSITY].lfo_max.load();
		dens_range = DISTANCEF(a,b);

		dens_min = std::min(a,b);
	}

	deviation_lfo = _track->lfo[DENSDEV];
	_dodeviationlfo = deviation_lfo && deviation_lfo->power();
	if (_dodeviationlfo) {
		MYFLOAT a = _STATE->controls[_track->index][DENSDEV].lfo_min.load();
		MYFLOAT b = _STATE->controls[_track->index][DENSDEV].lfo_max.load();
		deviation_a = std::min(a, b);
		devation_range = DISTANCEF(a, b);
	}

	if (!_dodeviationlfo && !_dodenslfo) {
        auto dev = _STATE->params[_track->index][DENSDEV].load();
        auto dens = _STATE->params[_track->index][DENSITY].load();
		for (int32_t chan = 0; chan < _STATE->channels; chan++) {
			setDev2(dev, chan);
			setFreq2(dens, chan);
		}
	}

	if (_oldalg == 3) {
		auto densa = _STATE->sr / LOG2NORMAL(
			_STATE->params[_track->index][GRAINGENSPLINEDENSA].load()), densb =
			_STATE->sr / LOG2NORMAL(
				_STATE->params[_track->index][GRAINGENSPLINEDENSB].load());
		auto cpsa = LOG2NORMAL(
			_STATE->params[_track->index][GRAINGENSPLINECPSA].load()), cpsb = LOG2NORMAL(
				_STATE->params[_track->index][GRAINGENSPLINECPSB].load());
		for (int32_t i = 0; i < _STATE->channels; i++) {
			spline[i].setUp(cpsa, cpsb, densa, densb);
		}
	}
	else if (_oldalg == 5) {
		// The wind field: 0..1 weather changing around the GUST rate. The cps
		// spread keeps the two channels' weather related but not identical.
		const auto gust = LOG2NORMAL(
			_STATE->params[_track->index][GRAINGENCHIMEGUST].load());
		for (int32_t i = 0; i < _STATE->channels; i++) {
			spline[i].setUp(gust * 0.6, gust * 1.7, 0., 1.);
		}
	}
	else if (_oldalg == 4) {
        follower.att = _STATE->params[_track->index][GRAINGENFOLATTACK].load();
        follower.rel = _STATE->params[_track->index][GRAINGENFOLDECAY].load();
        follower.min = _STATE->params[_track->index][GRAINGENFOLDENSA].load();
        follower.max = _STATE->params[_track->index][GRAINGENFOLDENSB].load();
        follower.gain = _STATE->params[_track->index][GRAINGENFOLGAIN].load();
        follower.prepare();
		auto src = _DATA->tracks[(int)_STATE->params[_track->index][DISTRSOURCE].load()];
		auto fb = src->filebuffer.load();
		auto state = fb ? src->currentState : nullptr;
		if(state == nullptr) {
			for (int32_t chan = 0; chan < _STATE->channels; chan++) {
				for (int32_t smpl = 0; smpl < _STATE->currentBufSize; smpl++) {
					buf[chan][smpl] = 0.;
				}
			}
		}
		else {
			long offset = state->offset.load();
			const long off = fb->off;
			if (!mono) {
				for (int32_t chan = 0; chan < _STATE->channels; chan++) {
					for (int32_t smpl = 0; smpl < _STATE->currentBufSize; smpl++) {
						long offf = offset + smpl * src->playbackDirTmp;
						buf[chan][smpl] =
							offf >= 0 && offf < off ? fb->buffer[chan][offf] : 0.;
					}
				}
			}
			else {
				for (int32_t smpl = 0; smpl < _STATE->currentBufSize; smpl++) {
					long offf = offset + smpl * src->playbackDirTmp;
					buf[0][smpl] =
						offf >= 0 && offf < off ? fb->buffer[0][offf] : 0.;
				}
				if (_STATE->channels == 2) {
					for (int32_t smpl = 0; smpl < _STATE->currentBufSize; smpl++) {
						buf[1][smpl] = buf[0][smpl];
					}
				}
			}
		}
		
	}

	for (int32_t smpl = 0; smpl < _STATE->currentBufSize; smpl++) {
		tickPriv(0, smpl);
		if (mono) {
			for (int32_t i = 1; i < _STATE->channels; i++){
				buf[i][smpl] = buf[0][smpl];
			    pitchBuf[i][smpl] = pitchBuf[0][smpl];
			    gainBuf[i][smpl] = gainBuf[0][smpl];
			    sizeBuf[i][smpl] = sizeBuf[0][smpl];
			}
		}
		else {
			for (int32_t i = 1; i < _STATE->channels; i++)
				 tickPriv(i, smpl);
		}
	}
	_STATE->params[_track->index][GRAINGENCOUNT0] = _count[0];
	_STATE->params[_track->index][GRAINGENCOUNT1] = _STATE->channels == 2 ? _count[1] : _count[0];
	_DATA->snapShot.queue.try_push(getState());
}

void graingenerator::onSampleRateChanged() {
	auto _appState = _track->_appState;
	spline[0].setSampleRate(_STATE->ksr);
	spline[1].setSampleRate(_STATE->ksr);
}
using namespace tsl::parameters;
tsl::parameters::Event graingenerator::getState() {
	GraingenState s{};
	s.count[0] = _count[0];
	s.count[1] = _track->_appState->channels == 2 ? _count[1] : _count[0];
	Event e;
	e.trackIndex = _track->index;
	e.eventType = Eventtype::paramUpdate;
	e.subType = EventSubtype::grainGenState;
	e.graingenState = s;
	e.flags |= Event::ToAudioThread;
	return e;
};

void graingenerator::setState(tsl::parameters::Event&e) {
	auto& s = e.graingenState;
	_count[0] = s.count[0];
	_count[1] = s.count[1];
};
