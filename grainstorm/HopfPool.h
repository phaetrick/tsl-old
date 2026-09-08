#pragma once
//
// Adaptive-oscillator sustain pool for ModalReverb MODE 13..15 - the "literal
// Adaptiverb" route: a bank of adaptive-frequency oscillators (the
// Righetti/Buchli/Ijspeert Hopf adaptation, reduced to banded per-oscillator
// PLLs) tracks the input's partials SAMPLE-ACCURATELY - no FFT, no frame
// quantization - and the tail is the oscillators themselves ringing out.
// Glides and vibrato are followed continuously instead of stair-stepping the
// way frame-based peak tracking does.
//
// Three deliberate deviations from the textbook pool, each fixing a failure
// mode found in the design phase:
//
//  1. BANDED adaptation. Free-roaming adaptive oscillators cluster onto the
//     loudest component. Every oscillator here owns one band of a log-spaced
//     grid and its frequency is clamped to that band - coverage is guaranteed
//     and clustering is impossible, at the cost of one oscillator per band.
//
//  2. ASYMMETRIC amplitude follower. The demodulated amplitude |c| is tracked
//     UP at attack speed but released ONLY at the RT60 rate. A plain
//     estimator would erase a vanished partial at learning speed - i.e. no
//     tail at all on note changes. The asymmetry IS the reverb.
//
//  3. PRESENCE gating. Frequency/phase adaptation and amplitude learning are
//     scaled by g = env/(env+knee). When the input stops, the drive is
//     "silence": ungated adaptation would detune ringing oscillators and
//     unlearn the tail. With g at 0 the pool coasts: phases advance, alphas
//     decay at RT60, nothing adapts. HOLD forces g=0 and d=1 - a drone.
//
// Per-oscillator state is one complex tracking phasor (PLL), one complex
// output phasor (advanced at omega*pitch, so PITCH transposes the tail while
// tracking stays on the input), the adapted frequency, the complex demodulator
// and the followed amplitude. Rotators are recomputed once per block (omega
// moves slowly); the per-sample cost is ~30 flops per oscillator.
//
// Worker-sliced by construction: processSlice(w, ...) touches only oscillators
// [w*perWorker, (w+1)*perWorker) plus that slice's own envelope copy, so the 4
// ModalReverb workers can run their slices concurrently with no shared writes.
// Each slice derives the same envelope from the same input independently -
// 4 redundant one-poles instead of one shared atomic.
//

#ifndef GRAINSTORM_HOPFPOOL_H
#define GRAINSTORM_HOPFPOOL_H

#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef MYFLOAT
#define MYFLOAT double
#endif

#ifndef TWOPI_P
#define TWOPI_P (6.283185307179586476925286766559005768394)
#endif

namespace tsl {

class HopfPool {
public:
	// sr: the rate processSlice runs at. n rounded down to a multiple of workers.
	void setUp(MYFLOAT sr, int32_t n, int32_t workers, MYFLOAT fLo, MYFLOAT fHi) {
		_sr = sr;
		_workers = workers > 0 ? workers : 1;
		_n = (n / _workers) * _workers;
		_per = _n / _workers;
		if (fHi > sr * (MYFLOAT).45) fHi = sr * (MYFLOAT).45;

		_phR.assign(_n, (MYFLOAT)1.); _phI.assign(_n, (MYFLOAT)0.);
		_psR.assign(_n, (MYFLOAT)1.); _psI.assign(_n, (MYFLOAT)0.);
		_cR.assign(_n, (MYFLOAT)0.);  _cI.assign(_n, (MYFLOAT)0.);
		_c1R.assign(_n, (MYFLOAT)0.); _c1I.assign(_n, (MYFLOAT)0.);
		_uR.assign(_n, (MYFLOAT)1.);  _uI.assign(_n, (MYFLOAT)0.);
		_cPR.assign(_n, (MYFLOAT)0.); _cPI.assign(_n, (MYFLOAT)0.);
		_coh.assign(_n, (MYFLOAT)0.);
		for (int32_t q = 0; q < 2; q++) {
			_wPub[q].assign(_n, (MYFLOAT)0.);
			_aPub[q].assign(_n, (MYFLOAT)0.);
		}
		_own.assign(_n, (MYFLOAT)1.);   // start owning: losers are muted within
		                                // one ownership time constant
		_blk.assign(_workers, 0);
		_alpha.assign(_n, (MYFLOAT)0.);
		_w.resize(_n); _wLo.resize(_n); _wHi.resize(_n); _wHome.resize(_n);
		_env.assign(_workers, (MYFLOAT)0.);
		_envF.assign(_workers, (MYFLOAT)0.);

		// log-spaced home frequencies; band edges at the geometric midpoints,
		// widened by _bandOverlap so a partial between two homes is reachable
		// by both and the winner is decided by lock quality, not by a gap
		const MYFLOAT ratio = pow(fHi / fLo, (MYFLOAT)1. / (MYFLOAT)(_n - 1));
		const MYFLOAT half = pow(ratio, (MYFLOAT).5 * _bandOverlap);
		uint32_t seed = 0x9e3779b9u;
		for (int32_t i = 0; i < _n; i++) {
			const MYFLOAT f = fLo * pow(ratio, (MYFLOAT)i);
			_w[i] = TWOPI_P * f / sr;
			_wLo[i] = _w[i] / half;
			_wHi[i] = _w[i] * half;
			_wHome[i] = _w[i];
			// deterministic scattered start phases (no <random> on this path)
			seed = seed * 1664525u + 1013904223u;
			const MYFLOAT ph = (MYFLOAT)(seed >> 8) / (MYFLOAT)(1 << 24) * TWOPI_P;
			_phR[i] = cos(ph); _phI[i] = sin(ph);
			_psR[i] = _phR[i]; _psI[i] = _phI[i];
		}

		_logHalfStep = (MYFLOAT).5 * log(ratio);

		// The envelope MUST be faster than the demodulator, or the floor it
		// references is still near zero when the onset transient reaches the
		// demod output - which is exactly how a single click ratcheted every
		// oscillator in the pool. 1 ms attack against a 2x30 ms demod.
		_envAtt = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).001 * sr));
		_envRel = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).100 * sr));
		_demodK = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).030 * sr));
		_attK   = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).030 * sr));
		_relK   = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).150 * sr));
		_envFRel = (MYFLOAT)1. - exp((MYFLOAT)-1. / ((MYFLOAT).015 * sr));
	}

	// One worker's oscillators over one block. in = mono input at _sr,
	// outL/outR are ACCUMULATED into (caller clears/owns them). rt60 in
	// seconds; pitch = output transposition factor; amp = output gain;
	// hold != 0 freezes (no adaptation, no decay).
	void processSlice(int32_t worker, const MYFLOAT* in, MYFLOAT* outL, MYFLOAT* outR,
		int32_t nSamp, MYFLOAT rt60, MYFLOAT pitch, MYFLOAT amp, int32_t hold) {
		const int32_t a = worker * _per, b = a + _per;
		const MYFLOAT d = hold ? (MYFLOAT)1.
			: pow((MYFLOAT)10., (MYFLOAT)-3. / (rt60 * _sr));
		MYFLOAT env = _env[worker];
		MYFLOAT envF = _envF[worker];
		const bool unity = (pitch == (MYFLOAT)1.);

		// slice-local rotator scratch - workers run concurrently, so this must
		// live on each worker's stack, not in the object
		MYFLOAT _rotR[512], _rotI[512], _rotPR[512], _rotPI[512];

		// per-block rotators: omega moves slowly, so one sin/cos pair per
		// oscillator per block is enough; the PLL nudge handles the residual
		for (int32_t i = a; i < b; i++) {
			_rotR[i - a] = cos(_w[i]);          _rotI[i - a] = sin(_w[i]);
			if (!unity) { _rotPR[i - a] = cos(_w[i] * pitch); _rotPI[i - a] = sin(_w[i] * pitch); }
		}

		for (int32_t s = 0; s < nSamp; s++) {
			const MYFLOAT x = in[s];
			const MYFLOAT ax = fabs(x);
			env += (ax > env ? _envAtt : _envRel) * (ax - env);
			// The bleed must stop the instant the input does, or it eats the
			// tail it is supposed to protect - the main envelope's 100 ms
			// release cost 30 dB of tail before this. 15 ms release.
			envF += (ax > envF ? _envAtt : _envFRel) * (ax - envF);
			// hold must freeze the bleed too, or FREEZE nibbles its own drone
			const MYFLOAT gB = hold ? (MYFLOAT)0. : envF / (envF + _knee);
			// presence gate: 0 in silence (coast), ~1 with signal
			const MYFLOAT g = hold ? (MYFLOAT)0. : env / (env + _knee);

			MYFLOAT sumL = 0., sumR = 0.;
			for (int32_t i = a; i < b; i++) {
				const int32_t k = i - a;
				MYFLOAT pr = _phR[i], pi = _phI[i];

				// advance tracking phasor by omega
				{
					const MYFLOAT r = pr * _rotR[k] - pi * _rotI[k];
					pi = pr * _rotI[k] + pi * _rotR[k];
					pr = r;
				}

				// complex demodulation of the in-band component: c settles to
				// (amplitude, phase offset) of a locked partial; for noise or
				// an off-frequency component, arg(c) rotates at the frequency
				// error - which is exactly what the per-block discriminator
				// below feeds on. No per-sample phase detector: a PLL nudge
				// only pulls in from a few loop-bandwidths away, while the
				// discriminator sees the error across the whole band.
				// TWO cascaded one-poles: mixing to baseband and lowpassing is
				// a bandpass around omega, and a single pole is only 6 dB/oct -
				// a partial 30 cents away still arrived at about -10 dB and
				// drove the whole skirt. Two poles halve that skirt in dB.
				_c1R[i] += _demodK * ((MYFLOAT)2. * x * pr - _c1R[i]);
				_c1I[i] += _demodK * ((MYFLOAT)-2. * x * pi - _c1I[i]);
				_cR[i] += _demodK * (_c1R[i] - _cR[i]);
				_cI[i] += _demodK * (_c1I[i] - _cI[i]);
				const MYFLOAT t = sqrt(_cR[i] * _cR[i] + _cI[i] * _cI[i]);

				_phR[i] = pr; _phI[i] = pi;

				// asymmetric follower toward the COHERENT amplitude: up at
				// attack speed, down ONLY at RT60. _coh (last block's lock
				// quality) keeps narrowband noise, whose demod phase spins,
				// out of the tail.
				// Gated by ownership too: a suppressed duplicate must stop
				// ACCUMULATING, not just stop sounding. Otherwise it keeps
				// growing alpha, stays the loudest thing near the partial, and
				// the suppression has to fight it forever. Decaying the loser
				// at RT60 makes the decision self-reinforcing and leaves one
				// oscillator per partial within a couple hundred ms.
				// Floor referenced to the INPUT level. The discriminator
				// measures demod ROTATION, so a transient leaves a decaying,
				// non-rotating residue that reads as a perfect lock - and since
				// the follower only releases at RT60, every oscillator kept
				// whatever the attack gave it. Individually that skirt sits
				// near -37 dB, but ~80 of them cluster in the bass and sum to a
				// fluctuating rumble. Anything this far under the input is not
				// a partial worth resynthesising. (A dwell/persistence gate was
				// tried here first and made it worse: delaying growth let
				// duplicates coexist and the skirt came back bigger.)
				MYFLOAT teff = t * _coh[i] * _own[i];
				if (teff < _floorRel * env) teff = (MYFLOAT)0.;
				if (teff > _alpha[i]) {
					_alpha[i] += _attK * g * (teff - _alpha[i]);
				} else {
					// Break the RATCHET. The follower deliberately releases only
					// at RT60 - that asymmetry is what makes a tail - but it
					// also means anything a transient deposits is held for the
					// entire tail, which is where the surviving detuned
					// duplicates came from. So while the input is PRESENT,
					// amplitude that the coherent evidence does not support
					// bleeds off quickly. Scaled by g, this term vanishes in
					// silence, leaving the RT60 decay alone to shape the tail.
					if (_alpha[i] > teff * _holdRatio)
						_alpha[i] += _relK * gB * (teff - _alpha[i]);
					_alpha[i] *= d;
				}

				// PHASE-COHERENT RESYNTHESIS. arg(c) is the offset between the
				// input partial and this oscillator's phasor, so u*ph is the
				// partial itself - correct phase, and correct FREQUENCY even
				// when the lock carries a residual error, because u rotates at
				// exactly that error. Frozen when the demod is too small to
				// define an angle, so a tail keeps the phase it ended on and
				// free-runs from there.
				if (t > _uFloor) {
					const MYFLOAT inv = (MYFLOAT)1. / t;
					_uR[i] = _cR[i] * inv;
					_uI[i] = _cI[i] * inv;
				}

				// PITCH 0 (factor 1) is the default, and there the output
				// phasor is bit-identical to the tracking phasor - same seed,
				// same rotator, same renormalisation. Skip it entirely.
				MYFLOAT qr, qi;
				if (unity) { qr = pr; qi = pi; }
				else {
					const MYFLOAT r = _psR[i] * _rotPR[k] - _psI[i] * _rotPI[k];
					_psI[i] = _psR[i] * _rotPI[k] + _psI[i] * _rotPR[k];
					_psR[i] = r;
					qr = _psR[i]; qi = _psI[i];
				}
				const MYFLOAT o = _alpha[i] * _own[i];
				sumL += o * (_uR[i] * qr - _uI[i] * qi);
				sumR += o * (_uR[i] * qi + _uI[i] * qr);
			}
			outL[s] += amp * sumL;
			outR[s] += amp * sumR;
		}
		// keep the (unused at unity) output phasor in step, so turning PITCH
		// away from 0 later starts from the right phase instead of a stale one
		if (unity)
			for (int32_t i = a; i < b; i++) { _psR[i] = _phR[i]; _psI[i] = _phI[i]; }

		_env[worker] = env;
		_envF[worker] = envF;

		// frequency discriminator, once per block: the demod's phase advance
		// over the block IS the frequency error (a quadrature FM detector).
		// Locked partial -> tiny drift -> coherence ~1 and omega converges;
		// noise -> the demod phase random-walks -> coherence ~0, so the
		// oscillator neither retunes nor sounds.
		{
			const MYFLOAT gBlock = hold ? (MYFLOAT)0. : env / (env + _knee);
			const int32_t par = _blk[worker] & 1;
			_blk[worker]++;
			const MYFLOAT* wPrev = _wPub[par ^ 1].data();
			const MYFLOAT* aPrev = _aPub[par ^ 1].data();
			MYFLOAT* wCur = _wPub[par].data();
			MYFLOAT* aCur = _aPub[par].data();
			const MYFLOAT ownK = (MYFLOAT)1. - exp(-(MYFLOAT)nSamp / (_ownTau * _sr));

			// Pool-referenced noise floor. An oscillator whose amplitude is
			// this far under the loudest one in the pool is not a partial, it
			// is residue - and residue is what a transient leaves behind before
			// the demodulator has settled, which no input-referenced gate can
			// undo afterwards because by then the input is gone. Referencing
			// the POOL makes the floor scale with the tail as it decays, so the
			// skirt is removed for as long as the tail lives. Cross-worker
			// maxima are read from the previous block's parity, i.e. across the
			// workerLatch barrier.
			MYFLOAT poolMax = (MYFLOAT)0.;
			for (int32_t q = 0; q < _workers; q++)
				poolMax = std::max(poolMax, _sliceMax[par ^ 1][q]);
			const MYFLOAT aFloor = poolMax * _floorPool;
			MYFLOAT myMax = (MYFLOAT)0.;
			for (int32_t i = a; i < b; i++) {
				const MYFLOAT dot = _cR[i] * _cPR[i] + _cI[i] * _cPI[i];
				const MYFLOAT crs = _cI[i] * _cPR[i] - _cR[i] * _cPI[i];
				_cPR[i] = _cR[i]; _cPI[i] = _cI[i];
				const MYFLOAT mag = sqrt(dot * dot + crs * crs);
				if (mag < (MYFLOAT)1e-18) {
					_coh[i] *= (MYFLOAT).9;
					wCur[i] = _w[i];
					aCur[i] = _alpha[i];
					continue;
				}
				const MYFLOAT dArg = atan2(crs, dot);           // rad per block
				const MYFLOAT werr = dArg / (MYFLOAT)nSamp;     // rad per sample
				// salience: a coasting oscillator (ring-out louder than its
				// demod) must not be retuned by whatever is left in the band
				const MYFLOAT t = sqrt(_cR[i] * _cR[i] + _cI[i] * _cI[i]);
				const MYFLOAT sal = t / (t + _alpha[i] + (MYFLOAT)1e-6);
				// ACTIVITY: is this oscillator being driven RIGHT NOW? Salience
				// alone is not enough - a settled oscillator sits at ~.5 and a
				// coasting one only falls to ~.1, so a plain multiply still let
				// an unrelated note drag a ringing oscillator 7 Hz off pitch in
				// 100 ms (measured). The soft-knee gate makes coasting EXACTLY
				// zero, and multiplying by coherence freezes everything through
				// note-onset transients, where every band sees broadband energy
				// and salience briefly spikes.
				const MYFLOAT act = (sal > _salGate)
					? (sal - _salGate) / ((MYFLOAT)1. - _salGate) * _coh[i]
					: (MYFLOAT)0.;
				MYFLOAT wNew = _w[i] + _Kf * gBlock * act * werr;
				// A CLAMPED oscillator is, by construction, mistuned against
				// whatever is driving it - and a mistuned oscillator beside the
				// one that locked properly is exactly what makes the tail beat.
				// Mute it. With bands overlapping (1.4 steps) every frequency is
				// interior to at least one band, so this can never silence a
				// partial outright.
				bool clamped = false;
				if (wNew < _wLo[i]) { wNew = _wLo[i]; clamped = true; }
				else if (wNew > _wHi[i]) { wNew = _wHi[i]; clamped = true; }
				_w[i] = wNew;
				// coherence: residual error against a tolerance that scales with
				// this oscillator's own frequency
				// Squared Lorentzian, not plain: a single 1/(1+q^2) is still
				// .07 for a neighbour 30 cents off the partial, and because the
				// amplitude follower only releases at RT60 that leakage
				// RATCHETS - every oscillator in the pool ends up holding
				// amplitude, which is what turned the tail into a dense
				// detuned cluster (comb / "delay character"). Fourth-order
				// rolloff puts the same neighbour 22 dB further down.
				const MYFLOAT q = fabs(werr) / (_cohFrac * _w[i]);
				const MYFLOAT lor = (MYFLOAT)1. / ((MYFLOAT)1. + q * q);
				const MYFLOAT target = lor * lor;
				_coh[i] += (MYFLOAT).2 * (target - _coh[i]);

				// DUPLICATE SUPPRESSION, geometric rather than by score.
				// Bands overlap, so one partial can be captured by two adjacent
				// oscillators; they settle a few cents apart and their sum BEATS
				// - measured at 36 dB of tail tremolo before this.
				//
				// The test is simply: have two neighbours CONVERGED onto each
				// other? At rest they sit a full grid step apart, so a
				// separation under half a step means they are chasing the same
				// partial. The one further from its own home yields. This is
				// deterministic and stateless - an earlier score-based
				// winner-take-all (coherence x amplitude x affinity, with
				// hysteresis) never settled: the winner hovered near .2 during
				// drive and drifted upward in silence, so both duplicates stayed
				// half-on and kept beating.
				// Rank by AMPLITUDE, and make the ranking the whole decision.
				// A clamped oscillator is mistuned by construction, so its
				// effective amplitude is knocked down three decades - it yields
				// to any non-clamped neighbour, but still sounds if nothing
				// else covers the partial (never leaves a hole).
				// the clamp penalty applies only while the oscillator is being
				// DRIVEN: a coasting one is frozen, and penalising it there
				// muted exactly the ringing tail a new note should leave alone
				const MYFLOAT effA = (clamped && act > (MYFLOAT)0.)
					? _alpha[i] * (MYFLOAT)1e-3 : _alpha[i];
				wCur[i] = _w[i];
				aCur[i] = effA;
				bool yield = false;
				for (int32_t nb = i - 1; nb <= i + 1; nb += 2) {
					if (nb < 0 || nb >= _n) continue;
					const MYFLOAT wn = wPrev[nb];
					if (wn <= (MYFLOAT)0.) continue;
					const MYFLOAT sep = fabs(log(_w[i] / wn)) / _logHalfStep;
					if (sep >= _mergeFrac) continue;      // distinct partials
					const MYFLOAT an = aPrev[nb];
					if (an > effA || (an == effA && nb < i)) yield = true;
				}
				const MYFLOAT tgt = yield ? (MYFLOAT)0. : (MYFLOAT)1.;

				if (_alpha[i] < aFloor) _alpha[i] = (MYFLOAT)0.;
				myMax = std::max(myMax, _alpha[i]);
				// Gated by presence AND by this oscillator's own salience,
				// SQUARED: an oscillator that is coasting must keep its
				// ownership frozen, or a later unrelated note re-decides - and
				// ducks - a tail that is still sounding. Plain salience is not
				// sharp enough, because an off-frequency note still leaks a few
				// percent through the demodulator's 30 ms lowpass; squaring
				// turns that residue into a negligible adaptation rate while
				// leaving a genuinely driven oscillator (salience ~.5+) fast.
				// Plain crossfade, no gating. The amplitude ranking is already
				// invariant under a decaying tail (every alpha scales by the
				// same factor per block), so the target does not drift once the
				// input stops - which is exactly what every gated variant of
				// this got wrong, leaving both duplicates half-on and beating.
				_own[i] += ownK * (tgt - _own[i]);
			}
			_sliceMax[par][worker] = myMax;
		}

		// renormalize the phasors once per block (drift is O(dphi^2))
		for (int32_t i = a; i < b; i++) {
			MYFLOAT m = _phR[i] * _phR[i] + _phI[i] * _phI[i];
			if (m > (MYFLOAT)1e-12) { m = (MYFLOAT)1. / sqrt(m); _phR[i] *= m; _phI[i] *= m; }
			m = _psR[i] * _psR[i] + _psI[i] * _psI[i];
			if (m > (MYFLOAT)1e-12) { m = (MYFLOAT)1. / sqrt(m); _psR[i] *= m; _psI[i] *= m; }
		}
	}

	// harness hook: retune the demodulator's pole pair
	void setDemodTau(MYFLOAT tau) {
		_demodK = (MYFLOAT)1. - exp((MYFLOAT)-1. / (tau * _sr));
	}
	int32_t size() const { return _n; }
	int32_t perWorker() const { return _per; }
	MYFLOAT oscFreq(int32_t i) const { return _w[i] * _sr / (MYFLOAT)TWOPI_P; }
	MYFLOAT oscAmp(int32_t i) const { return _alpha[i]; }
	// what the oscillator actually contributes after local competition
	MYFLOAT oscOut(int32_t i) const { return _alpha[i] * _own[i]; }

	// --- tunables (harness-verified defaults) -------------------------------
	// Discriminator gain. THE DEMODULATOR'S LAG IS INSIDE THIS LOOP (two poles
	// at _demodK), so the loop bandwidth must stay well under the demodulator's
	// or it limit-cycles. At .6 it did exactly that: on a dead-steady sine the
	// tracked frequency swung +-15 cents with a period of seconds - every
	// partial slowly detuning on its own, which reads as "repeating patterns,
	// faster in higher regions" because 15 cents is more Hz up top. Measured
	// wander vs gain at 220/880/3520 Hz: .6 -> 30 cents, .15 -> 19, .05 -> 0.01.
	MYFLOAT _Kf{ .05 };
	// Lock tolerance as a RELATIVE frequency deviation, not an absolute
	// rad/sample: .005 is ~8.6 cents at every frequency. An absolute tolerance
	// is meaninglessly loose at the bottom (9 Hz = 150 cents at 100 Hz, so
	// neighbours clamped a whole band away still counted as locked) and far too
	// tight at the top (9 Hz = 2 cents at 8 kHz, so real locks flickered out).
	MYFLOAT _cohFrac{ .005 };
	MYFLOAT _ownTau{ .05 };      // ownership crossfade, seconds
	// Neighbours closer than this (in HALF-steps; they rest 2 apart) count as
	// chasing the same partial. At .5 - a quarter of the natural spacing - two
	// oscillators 15 cents apart never merged and beat at 4 Hz; 1.5 catches
	// anything meaningfully converged while leaving resting neighbours alone.
	MYFLOAT _mergeFrac{ 1.5 };
	MYFLOAT _uFloor{ 1e-7 };     // |c| below which the phase reference freezes
	MYFLOAT _floorRel{ .004 };   // ~-48 dB of the input envelope: below this an
	                             // oscillator may not accumulate at all
	MYFLOAT _floorPool{ .012 };  // ~-38 dB of the loudest oscillator: below this
	                             // an oscillator is muted outright
	MYFLOAT _holdRatio{ 2. };    // holding more than this multiple of what the
	                             // demod supports counts as unsupported residue
	MYFLOAT _salGate{ .3 };      // salience below which an oscillator counts as
	                             // coasting and freezes completely
	MYFLOAT _knee{ 1e-3 };       // presence gate knee, ~-60 dBFS
	MYFLOAT _bandOverlap{ 1.4 }; // band half-width stretch beyond the midpoint

private:
	MYFLOAT _sr{ 48000. };
	int32_t _n{}, _per{}, _workers{ 4 };
	std::vector<MYFLOAT> _phR, _phI;     // tracking phasor (PLL)
	std::vector<MYFLOAT> _psR, _psI;     // output phasor (omega * pitch)
	std::vector<MYFLOAT> _cR, _cI;       // complex demodulator (2nd stage)
	std::vector<MYFLOAT> _c1R, _c1I;     // 1st stage of the demod lowpass
	std::vector<MYFLOAT> _uR, _uI;       // unit phase reference, arg(c)
	std::vector<MYFLOAT> _cPR, _cPI;     // demod at the previous block edge
	std::vector<MYFLOAT> _coh;           // lock coherence, 0..1, updated per block
	// Neighbour state, double-buffered on block parity: a worker writes this
	// block's parity and reads its neighbours from the previous one, so the
	// oscillators on a worker-slice boundary are read across the workerLatch
	// barrier - never concurrently written.
	MYFLOAT _sliceMax[2][8]{};           // per-worker loudest alpha, by parity
	std::vector<MYFLOAT> _wPub[2];       // published frequency
	std::vector<MYFLOAT> _aPub[2];       // published effective amplitude
	std::vector<MYFLOAT> _own;           // output gain from local competition
	std::vector<int32_t> _blk;           // per-worker block counter (parity)
	std::vector<MYFLOAT> _alpha;         // followed amplitude
	std::vector<MYFLOAT> _w, _wLo, _wHi; // rad/sample + band clamp
	std::vector<MYFLOAT> _wHome;         // grid home, for the ownership tiebreak
	MYFLOAT _logHalfStep{ 1. };          // half a grid step in log-frequency
	std::vector<MYFLOAT> _env;           // per-worker input envelope copy
	std::vector<MYFLOAT> _envF;          // fast-release copy, gates the bleed
	MYFLOAT _envAtt{}, _envRel{}, _demodK{}, _attK{}, _relK{}, _envFRel{};
};

} // namespace tsl

#endif // GRAINSTORM_HOPFPOOL_H
