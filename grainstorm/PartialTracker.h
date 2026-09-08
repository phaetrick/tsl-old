#pragma once
//
// Partial tracking + mode-slot allocation for the adaptive (Adaptiverb-style)
// ModalReverb mode.
//
// The tail of a resonator bank has no colour of its own: its spectrum is a
// subset of whatever the modes are tuned to. Tune them to the input's own
// steady partials and the tail becomes "what was just there, decaying" - no
// comb structure to smear, so no modulation needed either.
//
// Three pieces, deliberately free of project dependencies so they can be
// exercised by a standalone harness:
//
//   PartialAnalyzer  ring buffer + Hann window + hop scheduling
//   PartialTracker   peak picking -> match/birth/death -> steady-state gate
//   ModeAllocator    birth -> mode slot, with amplitude-ranked stealing
//
// The steady-state gate is the part that matters most and the part that is
// easy to skip: a peak only becomes a partial after it has survived several
// frames WITHOUT jumping in frequency. Transients vanish before they qualify
// and noise peaks wander, so neither ever reaches the bank. That exclusion -
// not any smoothing filter - is what keeps the tail free of graininess.
//

#ifndef GRAINSTORM_PARTIALTRACKER_H
#define GRAINSTORM_PARTIALTRACKER_H

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

// ---------------------------------------------------------------------------
// PartialAnalyzer: hands out windowed frames of fftSize every hop samples.
// The caller owns the FFT so this header stays dependency-free (the plugin
// uses the project FFT class, the harness uses PocketFFT directly).
// ---------------------------------------------------------------------------
class PartialAnalyzer {
public:
	void setUp(int32_t fftSize, int32_t hop) {
		_fftSize = fftSize;
		_hop = hop;
		_ring.assign(fftSize, (MYFLOAT)0.);
		_frame.assign(fftSize, (MYFLOAT)0.);
		_win.resize(fftSize);
		for (int32_t i = 0; i < fftSize; i++)
			_win[i] = (MYFLOAT).5 - (MYFLOAT).5 * cos(TWOPI_P * i / (MYFLOAT)fftSize);
		_pos = 0;
		_fill = 0;
	}

	// Push n samples; fn(windowedFrame) fires once per completed hop.
	template<typename F>
	void push(const MYFLOAT* in, int32_t n, F&& fn) {
		for (int32_t i = 0; i < n; i++) {
			_ring[_pos] = in[i];
			if (++_pos >= _fftSize) _pos = 0;
			if (++_fill < _hop) continue;
			_fill = 0;
			// oldest sample of the fftSize-long history sits at _pos
			for (int32_t j = 0; j < _fftSize; j++) {
				int32_t k = _pos + j;
				if (k >= _fftSize) k -= _fftSize;
				_frame[j] = _ring[k] * _win[j];
			}
			fn(_frame.data());
		}
	}

	void reset() {
		std::fill(_ring.begin(), _ring.end(), (MYFLOAT)0.);
		_pos = 0;
		_fill = 0;
	}

	int32_t fftSize() const { return _fftSize; }
	int32_t hop() const { return _hop; }

private:
	std::vector<MYFLOAT> _ring, _frame, _win;
	int32_t _fftSize{}, _hop{}, _pos{}, _fill{};
};


// ---------------------------------------------------------------------------
// PartialTracker
// ---------------------------------------------------------------------------
class PartialTracker {
public:
	struct Partial {
		MYFLOAT freq{};        // Hz, smoothed
		MYFLOAT mag{};         // linear, smoothed
		MYFLOAT jitter{};      // max frame-to-frame movement in cents while unconfirmed
		int32_t age{};         // frames since first seen
		int32_t missing{};     // consecutive unmatched frames
		int32_t id{};          // stable across the partial's life
		bool confirmed{};      // passed the steady-state gate
		bool justConfirmed{};  // became confirmed on THIS frame (a birth event)
		bool matched{};        // scratch, per frame
		int32_t user{ -1 };    // caller-owned (mode slot); tracker never reads it
	};

	void setUp(MYFLOAT sr, int32_t fftSize) {
		_sr = sr;
		_fftSize = fftSize;
		_partials.clear();
		_peaks.clear();
		_births.clear();
		_deaths.clear();
		_nextId = 1;
	}

	void processSpectrum(const MYFLOAT* mag, int32_t nbins);

	const std::vector<Partial>& partials() const { return _partials; }
	std::vector<Partial>& partials() { return _partials; }

	// Indices into partials() confirmed on the most recent frame.
	const std::vector<int32_t>& births() const { return _births; }
	// user values of partials that died on the most recent frame. A death does
	// NOT free the mode slot - the mode keeps ringing out; the slot is only
	// reclaimed by amplitude-ranked stealing in ModeAllocator.
	const std::vector<int32_t>& deaths() const { return _deaths; }

	int32_t confirmedCount() const {
		int32_t n = 0;
		for (const auto& p : _partials) if (p.confirmed) n++;
		return n;
	}

	// --- tunables -----------------------------------------------------------
	int32_t maxPeaks{ 64 };        // past this you allocate modes to the noise floor
	MYFLOAT relFloorDb{ -60. };    // relative to the loudest bin in the frame
	MYFLOAT absFloor{ 1e-5 };      // ~-100 dB, keeps silence from producing peaks
	MYFLOAT matchCents{ 35. };     // below this you fight vibrato, above it semitones merge
	MYFLOAT stabilityCents{ 60. }; // max frame-to-frame movement for a candidate
	int32_t minAge{ 3 };           // frames a peak must survive to become a partial
	int32_t maxMissing{ 4 };       // dropout tolerance before a partial dies
	MYFLOAT freqSmooth{ .3 };
	MYFLOAT magSmooth{ .5 };
	MYFLOAT fMin{ 30. };
	MYFLOAT fMax{ 12000. };

	// Prominence over the LOCAL spectral floor, in dB - the discriminator that
	// actually separates partials from noise. Persistence alone does not:
	// consecutive frames share fftSize-hop samples, so a noise peak sits still
	// for fftSize/hop frames purely from overlapped input and sails through any
	// age/jitter gate. A windowed sinusoid stands 20-50 dB above its
	// surroundings; a noise maximum is Rayleigh-distributed and clears the
	// local median by under ~10 dB.
	MYFLOAT promDb{ 15. };
	int32_t promSkirt{ 5 };        // bins to skip either side (Hann main lobe is ~4)
	int32_t promSpan{ 40 };        // bins out to which the floor is measured

private:
	struct Peak { MYFLOAT freq, mag; int32_t bin; };

	MYFLOAT _sr{ 48000. };
	int32_t _fftSize{ 4096 };
	int32_t _nextId{ 1 };
	std::vector<Partial> _partials;
	std::vector<Peak> _peaks;
	std::vector<int32_t> _births, _deaths;
	std::vector<MYFLOAT> _floorScratch;

	static MYFLOAT cents(MYFLOAT a, MYFLOAT b) {
		return (MYFLOAT)1200. * (MYFLOAT)log2(a / b);
	}

	// median magnitude in [k-span, k-skirt] u [k+skirt, k+span]
	MYFLOAT localFloor(const MYFLOAT* mag, int32_t nbins, int32_t k) {
		_floorScratch.clear();
		for (int32_t d = promSkirt; d <= promSpan; d++) {
			const int32_t a = k - d, b = k + d;
			if (a >= 0) _floorScratch.push_back(mag[a]);
			if (b < nbins) _floorScratch.push_back(mag[b]);
		}
		if (_floorScratch.empty()) return (MYFLOAT)0.;
		auto mid = _floorScratch.begin() + _floorScratch.size() / 2;
		std::nth_element(_floorScratch.begin(), mid, _floorScratch.end());
		return *mid;
	}
};

inline void PartialTracker::processSpectrum(const MYFLOAT* mag, int32_t nbins) {
	_peaks.clear();
	_births.clear();
	_deaths.clear();

	const MYFLOAT binHz = _sr / (MYFLOAT)_fftSize;
	int32_t kLo = (int32_t)ceil(fMin / binHz);
	int32_t kHi = (int32_t)floor(fMax / binHz);
	if (kLo < 1) kLo = 1;
	if (kHi > nbins - 2) kHi = nbins - 2;

	// --- threshold from the loudest bin in band -----------------------------
	MYFLOAT frameMax = 0.;
	for (int32_t k = kLo; k <= kHi; k++)
		if (mag[k] > frameMax) frameMax = mag[k];

	MYFLOAT thr = frameMax * (MYFLOAT)pow(10., relFloorDb / 20.);
	if (thr < absFloor) thr = absFloor;

	// --- peak pick with parabolic interpolation on log magnitude ------------
	for (int32_t k = kLo; k <= kHi; k++) {
		const MYFLOAT b = mag[k];
		if (b <= thr) continue;
		if (b <= mag[k - 1] || b < mag[k + 1]) continue;   // strict left, >= right

		const MYFLOAT tiny = (MYFLOAT)1e-30;
		const MYFLOAT la = (MYFLOAT)log(mag[k - 1] + tiny);
		const MYFLOAT lb = (MYFLOAT)log(b + tiny);
		const MYFLOAT lc = (MYFLOAT)log(mag[k + 1] + tiny);

		const MYFLOAT denom = la - 2. * lb + lc;
		MYFLOAT d = 0.;
		if (fabs(denom) > (MYFLOAT)1e-12) d = (MYFLOAT).5 * (la - lc) / denom;
		if (d > (MYFLOAT).5) d = (MYFLOAT).5;
		if (d < (MYFLOAT)-.5) d = (MYFLOAT)-.5;

		Peak p;
		p.freq = ((MYFLOAT)k + d) * binHz;
		p.mag = (MYFLOAT)exp(lb - (MYFLOAT).25 * (la - lc) * d);
		p.bin = k;
		if (p.freq >= fMin && p.freq <= fMax)
			_peaks.push_back(p);
	}

	// --- cap, then reject anything that is not sinusoid-shaped --------------
	if ((int32_t)_peaks.size() > maxPeaks) {
		std::nth_element(_peaks.begin(), _peaks.begin() + maxPeaks, _peaks.end(),
			[](const Peak& a, const Peak& b) { return a.mag > b.mag; });
		_peaks.resize(maxPeaks);
	}

	const MYFLOAT promLin = (MYFLOAT)pow(10., promDb / 20.);
	_peaks.erase(std::remove_if(_peaks.begin(), _peaks.end(),
		[&](const Peak& p) {
			return p.mag < localFloor(mag, nbins, p.bin) * promLin;
		}), _peaks.end());

	// --- greedy match loudest-first -----------------------------------------
	std::sort(_peaks.begin(), _peaks.end(),
		[](const Peak& a, const Peak& b) { return a.mag > b.mag; });

	for (auto& p : _partials) {
		p.matched = false;
		p.justConfirmed = false;
	}

	for (const auto& pk : _peaks) {
		int32_t best = -1;
		MYFLOAT bestDist = matchCents;
		for (int32_t i = 0; i < (int32_t)_partials.size(); i++) {
			if (_partials[i].matched) continue;
			const MYFLOAT dist = (MYFLOAT)fabs(cents(pk.freq, _partials[i].freq));
			if (dist < bestDist) { bestDist = dist; best = i; }
		}

		if (best >= 0) {
			Partial& p = _partials[best];
			// frame-to-frame movement, measured BEFORE smoothing: a slow glide
			// stays small, a noise peak hopping bins does not
			if (!p.confirmed) {
				const MYFLOAT mv = (MYFLOAT)fabs(cents(pk.freq, p.freq));
				if (mv > p.jitter) p.jitter = mv;
			}
			p.freq += freqSmooth * (pk.freq - p.freq);
			p.mag += magSmooth * (pk.mag - p.mag);
			p.matched = true;
			p.missing = 0;
			p.age++;
		} else {
			Partial p;
			p.freq = pk.freq;
			p.mag = pk.mag;
			p.id = _nextId++;
			p.age = 1;
			p.matched = true;
			_partials.push_back(p);
		}
	}

	// --- confirm / age out --------------------------------------------------
	for (int32_t i = 0; i < (int32_t)_partials.size(); i++) {
		Partial& p = _partials[i];
		if (!p.matched) p.missing++;
		if (!p.confirmed && p.matched && p.age >= minAge && p.jitter <= stabilityCents) {
			p.confirmed = true;
			p.justConfirmed = true;
			_births.push_back(i);
		}
	}

	for (int32_t i = (int32_t)_partials.size() - 1; i >= 0; i--) {
		if (_partials[i].missing > maxMissing) {
			if (_partials[i].confirmed)
				_deaths.push_back(_partials[i].user);
			_partials.erase(_partials.begin() + i);
			// births hold indices into _partials, so anything after the erased
			// slot has shifted down by one
			for (auto& b : _births) if (b > i) b--;
		}
	}
}


// ---------------------------------------------------------------------------
// ModeAllocator
//
// Slots are partitioned per worker because ModalReverb's rolling refresh only
// ever writes into its own contiguous M/4 block - a pool-wide allocator would
// need cross-block writes and break that contract. Births round-robin across
// workers; stealing is local to one quarter, which costs nothing measurable at
// 512 slots per quarter.
//
// Running out of slots is a SOFT failure: the quietest still-ringing mode is
// reused, so the least audible component of the tail disappears rather than a
// loud one being cut off. That is what makes the exact slot count uncritical.
// ---------------------------------------------------------------------------
class ModeAllocator {
public:
	// nSlots      total addressable slots (the whole bank)
	// perWorker   adaptive slots owned inside each worker's block
	// stride      distance between worker blocks (the bank's M/4)
	// The adaptive pool is the FIRST perWorker slots of each worker block, so
	// slot = w*stride + i. The rest of each block keeps its random identity.
	void setUp(int32_t nSlots, int32_t perWorker, int32_t nWorkers, int32_t stride) {
		_n = nSlots;
		_workers = nWorkers > 0 ? nWorkers : 1;
		_size = perWorker;
		_stride = stride;
		_freq.assign(_n, (MYFLOAT)0.);
		_live.assign(_n, (unsigned char)0);
		_used.assign(_n, (unsigned char)0);
		_cursor.assign(_workers, 0);
		_rr = 0;
		_steals = 0;
		_liveSteals = 0;
		_adoptions = 0;
		_allocs = 0;
	}

	// Preferred entry point. A partial that dies and reappears at nearly the
	// same frequency - which is what deep vibrato does to high harmonics, since
	// they smear across bins between the turning points of the swing - must not
	// burn a second slot. Re-adopting keeps the existing mode AT ITS CURRENT
	// FREQUENCY, so no mode parameter is rewritten and the adoption can scan
	// across worker blocks without violating the per-block write contract.
	int32_t acquire(MYFLOAT freqHz, const MYFLOAT* amp) {
		const int32_t adopted = findNear(freqHz);
		if (adopted >= 0) {
			_live[adopted] = 1;
			_adoptions++;
			return adopted;
		}
		return allocate(freqHz, amp);
	}

	// A used slot within adoptCents of freqHz, preferring the closest.
	int32_t findNear(MYFLOAT freqHz) const {
		int32_t best = -1;
		MYFLOAT bestDist = adoptCents;
		for (int32_t i = 0; i < _n; i++) {
			if (!_used[i] || _freq[i] <= 0.) continue;
			const MYFLOAT d = (MYFLOAT)fabs(1200. * log2(freqHz / _freq[i]));
			if (d < bestDist) { bestDist = d; best = i; }
		}
		return best;
	}

	// amp: current ring amplitude per slot (|_ym_prev|, or the cheaper
	// |re|+|im|). Returns the slot index, never -1.
	int32_t allocate(MYFLOAT freqHz, const MYFLOAT* amp) {
		if (_n <= 0 || _size <= 0) return -1;
		_allocs++;
		const int32_t w = _rr;
		_rr = (_rr + 1) % _workers;
		const int32_t base = w * _stride;

		// 1) a slot that has never been used
		for (int32_t i = 0; i < _size; i++) {
			const int32_t c = base + _cursor[w];
			_cursor[w] = (_cursor[w] + 1) % _size;
			if (!_used[c]) { claim(c, freqHz); return c; }
		}

		// 2) quietest slot that is only ringing out (no live partial on it)
		int32_t best = -1;
		MYFLOAT bestAmp = 0.;
		for (int32_t i = base; i < base + _size; i++) {
			if (_live[i]) continue;
			if (best < 0 || amp[i] < bestAmp) { best = i; bestAmp = amp[i]; }
		}

		// 3) everything is live - take the quietest anyway
		if (best < 0) {
			_liveSteals++;
			for (int32_t i = base; i < base + _size; i++)
				if (best < 0 || amp[i] < bestAmp) { best = i; bestAmp = amp[i]; }
		}

		_steals++;
		claim(best, freqHz);
		return best;
	}

	// the partial owning this slot died; the mode keeps ringing, it just
	// becomes eligible for stealing
	void release(int32_t slot) {
		if (slot >= 0 && slot < _n) _live[slot] = 0;
	}

	MYFLOAT slotFreq(int32_t slot) const { return _freq[slot]; }
	bool slotLive(int32_t slot) const { return _live[slot] != 0; }
	int32_t slots() const { return _n; }
	int32_t steals() const { return _steals; }
	int32_t liveSteals() const { return _liveSteals; }
	int32_t adoptions() const { return _adoptions; }
	int32_t allocations() const { return _allocs; }

	MYFLOAT adoptCents{ 50. };

private:
	void claim(int32_t slot, MYFLOAT freqHz) {
		_freq[slot] = freqHz;
		_live[slot] = 1;
		_used[slot] = 1;
	}

	int32_t _n{}, _workers{ 4 }, _size{}, _stride{}, _rr{}, _steals{},
		_liveSteals{}, _adoptions{}, _allocs{};
	std::vector<MYFLOAT> _freq;
	std::vector<unsigned char> _live, _used;
	std::vector<int32_t> _cursor;
};

} // namespace tsl

#endif // GRAINSTORM_PARTIALTRACKER_H
