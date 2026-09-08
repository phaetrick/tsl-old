#pragma once
//
// Created by pr on 31.08.20.
//

#ifndef GRAINSTORM_REVERBPROGENITOR_H
#define GRAINSTORM_REVERBPROGENITOR_H


#include <resample.h>
#include "track.h"
#include "Reverb.h"
#include "Allpass.h"
#include "defines.h"
#include "app.h"

/*
// Fs = 34.125 kHz
const long FV3_(progenitor)::allpassLCo[] = {239, 392, 1944, 612, 1212, 121, 816, 1264,};
const long FV3_(progenitor)::allpassRCo[] = {205, 329, 2032, 368, 1452,   5, 688, 1340,};
const long FV3_(progenitor)::delayLCo[] = {1, 2, 1055, 344, 1572, 0,};
//                                        61 16    23   31    37
const long FV3_(progenitor)::delayRCo[] = {1, 625, 835, 500, 16, 1460,};
//                                        66   40   41   49  58 40+41
const long FV3_(progenitor)::allpM_EXCURSION = 32;
const long FV3_(progenitor)::idxOutCo[] = {276,   468, 625, 312,  8,   24, 36,   40,  1, 192, 1572,};
//                                         d31   {d49  d40  d31 d58}  d49 d37  {d31 d23  d49   d37}
*/


class DownUpSampler : public Effect {
public:
	DownUpSampler(TRACK* t, int32_t fxnum, int type, MYFLOAT inrate, MYFLOAT outrate) : Effect(t, fxnum,
		type) {
		rsIn.init(inrate / outrate);
		rsOut.init(outrate / inrate);
	}

protected:

	inline void loop(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int frames, MYFLOAT mix,
		MYFLOAT gain) {
		if (_inBuf.size() < frames * 2) {
			_inBuf.resize(frames * 2);
		}
		
		MYFLOAT* in[2] = { _inBuf.data(), _inBuf.data() + frames };
		
		for (int i = 0; i < frames; i++) {
			in[0][i] = inl[i];
			in[1][i] = inr[i];
		}
		/*
		int numFrames = rsOut.inputSamplesNeeded(frames);
		if (_rsBuf.size() < numFrames * 2) {
			_rsBuf.resize(numFrames * 2);
		}

		MYFLOAT* rsBuf[2] = { _rsBuf.data(), _rsBuf.data() + numFrames };
		
		int samplesRead = 0, samplesWritten = 0;
		while (samplesRead < frames) {
			if (rsIn.isWriteNeeded()) {
				MYFLOAT tmp[2] = { inl[samplesRead], inr[samplesRead] };
				rsIn.writeNextFrame(tmp);
				samplesRead++;
			}
			else {
				//sampleTSL tmp[2];
				rsIn.readNextFrame(rsRingBuf[readPos]);
				tick(rsRingBuf[readPos]);
				(++readPos) &= 3;
				if (samplesWritten < numFrames) {
					rsBuf[0][samplesWritten] = rsRingBuf[writePos][0];
					rsBuf[1][samplesWritten] = rsRingBuf[writePos][1];
					samplesWritten++;
					(++writePos) &= 3;
				}
			}
		}
		while (!rsIn.isWriteNeeded()) {
			rsIn.readNextFrame(rsRingBuf[readPos]);
			tick(rsRingBuf[readPos]);
			(++readPos) &= 3;
			if (samplesWritten < numFrames) {
				rsBuf[0][samplesWritten] = rsRingBuf[writePos][0];
				rsBuf[1][samplesWritten] = rsRingBuf[writePos][1];
				samplesWritten++;
				(++writePos) &= 3;
			}

		}
		while (samplesWritten < numFrames) {
			rsBuf[0][samplesWritten] = rsRingBuf[writePos][0];
			rsBuf[1][samplesWritten] = rsRingBuf[writePos][1];
			samplesWritten++;
			(++writePos) &= 3;
		}

		samplesWritten = 0;
		samplesRead = 0;
		while (samplesWritten < frames) {
			while (rsOut.isWriteNeeded()) {
				MYFLOAT tmp[2] = { rsBuf[0][samplesRead], rsBuf[1][samplesRead] };
				samplesRead++;
				rsOut.writeNextFrame(tmp);
			}

			MYFLOAT tt[2] = {};
			rsOut.readNextFrame(tt);
			MYFLOAT mixsrc = 1. - _smooth1;
			outl[samplesWritten] =
				tt[0] * _smooth1 * _smooth2 + in[0][samplesWritten] * mixsrc;
			outr[samplesWritten] =
				tt[1] * _smooth1 * _smooth2 + in[1][samplesWritten] * mixsrc;
			smmixgain(mix, gain);
			samplesWritten++;
		}
		*/

		/*
		int numFrames = rsUp.inputSamplesNeeded(s);

		int samplesRead = 0, samplesWritten = 0;
		while (samplesRead < s) {
			if (rsDown.isWriteNeeded()) {
				MYFLOAT tmp[2] = { inl[samplesRead], inr[samplesRead] };
				rsDown.writeNextFrame(tmp);
				samplesRead++;
			}
			else {
				//sampleTSL tmp[2];
				rsDown.readNextFrame(_line[readpos]);
				tick(_line[readpos]);
				(++readpos) &= 3;
				if(rsUp.isWriteNeeded()) {
					rsUp.writeNextFrame(_line[writepos]);
					(++writepos) &= 3;
				}
				else if(samplesWritten<s){
					MYFLOAT tt[2] = {};
					rsUp.readNextFrame(tt);
					MYFLOAT mixsrc = 1. - _smooth1;
					outl[samplesWritten] =
						tt[0] * _smooth1 * _smooth2 + iL[samplesWritten] * mixsrc;
					outr[samplesWritten] =
						tt[1] * _smooth1 * _smooth2 + iR[samplesWritten] * mixsrc;
					samplesWritten++;
					smmixgain(mix, gain);
				}
			}
		}
		while (!rsDown.isWriteNeeded()) {
			rsDown.readNextFrame(_line[readpos]);
			tick(_line[readpos]);
			(++readpos) &= 3;
			if (rsUp.isWriteNeeded()) {
				rsUp.writeNextFrame(_line[writepos]);
				(++writepos) &= 3;
			}
			else if (samplesWritten < s) {
				MYFLOAT tt[2] = {};
				rsUp.readNextFrame(tt);
				MYFLOAT mixsrc = 1. - _smooth1;
				outl[samplesWritten] =
					tt[0] * _smooth1 * _smooth2 + iL[samplesWritten] * mixsrc;
				outr[samplesWritten] =
					tt[1] * _smooth1 * _smooth2 + iR[samplesWritten] * mixsrc;
				samplesWritten++;
				smmixgain(mix, gain);
			}

		}

		while (samplesWritten < s) {
			MYFLOAT tt[2] = {};
			rsUp.readNextFrame(tt);
			MYFLOAT mixsrc = 1. - _smooth1;
			outl[samplesWritten] =
				tt[0] * _smooth1 * _smooth2 + iL[samplesWritten] * mixsrc;
			outr[samplesWritten] =
				tt[1] * _smooth1 * _smooth2 + iR[samplesWritten] * mixsrc;
			samplesWritten++;
			smmixgain(mix, gain);
		}
		*/
		int samplesWritten = 0;
		int samplesRead = 0;

		while (samplesWritten < frames && samplesRead < frames) {
			if (rsIn.isWriteNeeded()) {
				MYFLOAT tt[2] = { in[0][samplesRead], in[1][samplesRead] };
				rsIn.writeNextFrame(tt);
				samplesRead++;
			}
			else {
				MYFLOAT s[2];
				rsIn.readNextFrame(s);
				tick(s);
				ringPush(s);
			}

			if (rsOut.isWriteNeeded()) {
				MYFLOAT s[2];
				ringPop(s);
				rsOut.writeNextFrame(s);
			}
			else {
				MYFLOAT tt[2] = {};
				rsOut.readNextFrame(tt);
				MYFLOAT mixsrc = 1. - _smooth1;
				outl[samplesWritten] =
					tt[0] * _smooth1 * _smooth2 + in[0][samplesWritten] * mixsrc;
				outr[samplesWritten] =
					tt[1] * _smooth1 * _smooth2 + in[1][samplesWritten] * mixsrc;
				samplesWritten++;
				smmixgain(mix, gain);
			}
		}
		while (samplesRead < frames) {
			if (rsIn.isWriteNeeded()) {
				MYFLOAT tt[2] = { in[0][samplesRead], in[1][samplesRead] };
				rsIn.writeNextFrame(tt);
				samplesRead++;
			}
			else {
				MYFLOAT s[2];
				rsIn.readNextFrame(s);
				tick(s);
				ringPush(s);
			}
		}

		while (samplesWritten < frames) {
			if (rsOut.isWriteNeeded()) {
				MYFLOAT s[2];
				ringPop(s);
				rsOut.writeNextFrame(s);
			}
			else {
				MYFLOAT tt[2] = {};
				rsOut.readNextFrame(tt);
				MYFLOAT mixsrc = 1. - _smooth1;
				outl[samplesWritten] =
					tt[0] * _smooth1 * _smooth2 + in[0][samplesWritten] * mixsrc;
				outr[samplesWritten] =
					tt[1] * _smooth1 * _smooth2 + in[1][samplesWritten] * mixsrc;
				samplesWritten++;
				smmixgain(mix, gain);
			}
		}
	}

	virtual void tick(MYFLOAT in[]) = 0;

protected:
	// rsIn and rsOut are independent phase accumulators: the number of internal-rate
	// frames the first produces per block and the number the second consumes to emit one
	// host block differ by +-1. This used to be a blind rotation (readPos{1}/writePos{})
	// encoding a fixed one-slot producer lead, which only holds while the host rate is
	// ABOVE _internalsr. Below it the two ceil(mRate) startup phases swap roles, the lead
	// goes negative, and the consumer read slots the producer had not written yet.
	// An explicit occupancy count makes it rate-independent.
	inline void ringPush(const MYFLOAT v[2]) {
		if (ringOcc == 4) return;                        // full: drop, never clobber unread
		rsRingBuf[ringTail][0] = v[0];
		rsRingBuf[ringTail][1] = v[1];
		(++ringTail) &= 3;
		ringOcc++;
	}

	inline void ringPop(MYFLOAT v[2]) {
		if (ringOcc == 0) { v[0] = v[1] = 0; return; }   // empty: silence, never a stale slot
		v[0] = rsRingBuf[ringHead][0];
		v[1] = rsRingBuf[ringHead][1];
		(++ringHead) &= 3;
		ringOcc--;
	}

private:
	tsl::AlignedVector <MYFLOAT> _rsBuf, _inBuf;
	alignas(64)MYFLOAT rsRingBuf[4][2]{};
	int ringHead{};
	int ringTail{ 2 };
	int ringOcc{ 2 };                                    // primed with 2 silent frames
	int prevFrames{}, prevInternalFrames{};
	MultiChanPolyPhaseResampler<MYFLOAT> rsIn{};
	MultiChanPolyPhaseResampler<MYFLOAT> rsOut{};
};

class Progenitor1Orig : public DownUpSampler {
public:
	Progenitor1Orig(TRACK* t) : DownUpSampler(t, SPACE_REVERB4, STEREOEFFECT, t->_STATE->sr,
		_internalsr) {
		_decay = &t->_STATE->params[t->index][REV4REF];
		_lpcutold = *(_lpcut = &t->_STATE->params[t->index][REV4LPCUT]);
		_hpcutold = *(_hpcut = &t->_STATE->params[t->index][REV4HPCUT]);
		_mix = &t->_STATE->params[t->index][REV4MIX];
		_gain = &_STATE->params[t->index][REV4GAIN];
		_smooth2 = dbToLinear60(*_gain);
		_bypass = &t->bypass[SPACE_REVERB4];
		_predelay = &t->_STATE->params[t->index][REV4PREDELAY];
		_predelayprev = *_predelay;
		_predelayL.init(_internalsr * 1.05, _predelayprev * _internalsr * 0.001);
		_predelayR.init(_internalsr * 1.05, _predelayprev * _internalsr * 0.001);
		allpassmL_15_16._appState = allpassmL_17_18._appState = allpassmR_19_20._appState = allpassmR_21_22._appState = allpass2L_25_27._appState = allpass2R_43_45._appState = allpass3L_34_37._appState = allpass3R_52_55._appState = _appState;

		allpassmL_15_16.init(_internalsr, 239, 0.001,
			3.123, .375);
		allpassmR_19_20.init(_internalsr, 205, 0.0012,
			1.6544, .375);

		allpassmL_17_18.init(_internalsr, 392, 0.0015,
			1.52392, .312);
		allpassmR_21_22.init(_internalsr, 329, 0.001,
			2.5545694, .312);
		//  void init(int32_t size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _STATE->sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

		allpass2L_25_27.init(_internalsr, 612, 0.0013, 1.6, .25, 1944, 0.0009, 1.4554, .406, .781, .219);
		allpass2R_43_45.init(_internalsr, 368, 0.0009, 1.110, .25, 2032, 0.0007, 1.973, .406, .781, .219);

		allpass3L_34_37.init(_internalsr, 1264, 0.0009,
			1.456, .25, 816,
			0.0010,
			1.425, .25,
			1212, 0.0007, 1.546, .406, .781, .219);
		allpass3R_52_55.init(_internalsr, 1340, 0.001,
			1.924, .25, 688,
			0.0011,
			1.1353, .25,
			1452, 0.0010, 1.6234, .406, .188, .812);


		lpfL_in_59_60.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfR_in_64_65.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		lpfLdamp_11_12.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfRdamp_13_14.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		lpfL_9_10.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfR_7_8.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		out1_lpf.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		out2_lpf.init(_internalsr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		delayL_16.init(2);
		delayL_23.init(1055);
		delayL_31.init(344);
		delayL_37.init(1572);
		delayR_40.init(1460);
		delayR_41.init(835);
		delayR_49.init(500);
		delayR_58.init(16);
		delayR_ts.init(1);

		_totaldelay = delayL_16.getDelay() + delayL_23.getDelay() + delayL_31.getDelay() +
			delayL_37.getDelay() + delayR_40.getDelay() + delayR_41.getDelay() +
			delayR_49.getDelay() + delayR_58.getDelay() +
			allpassmL_15_16.getTotalDelay() + allpassmR_21_22.getTotalDelay() +
			allpassmR_19_20.getTotalDelay() + allpassmL_17_18.getTotalDelay() +
			allpass2L_25_27.getTotalDelay() + allpass2R_43_45.getTotalDelay() +
			allpass3L_34_37.getTotalDelay() + allpass3R_52_55.getTotalDelay();
	}

	void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) override {
		MYFLOAT gain, mix;
		if (*_bypass || destroyRequested) {
			gain = 1.f;
			mix = 0.f;
		}
		else {
			gain = dbToLinear60(*_gain);
			mix = *_mix;
		}
		check();
		_loopdecay = .6 + _decay->load() * .399;
		loop(inl, inr, outl, outr, s, mix, gain);
	}

	inline void tick(MYFLOAT in[]) override {
		UDD(_lastL);
		UDD(_lastR);

		auto lastL = out1_lpf.tick(delayL_37.readwrite(allpass3L_34_37.tick3n(
			delayL_31.readwrite(allpass2L_25_27.tick2n(delayL_23.readwrite(
				allpassmL_17_18.tick1(
					delayL_16.readwrite(allpassmL_15_16.tick1(
						lpfLdamp_11_12.tick(
							(lpfL_in_59_60.tick(
								_predelayL.tick(in[0] * .5)) +
								_loopdecay * _lastR)))))))))));

		auto lastR = out2_lpf.tick(delayR_58.readwrite(allpass3R_52_55.tick3n(
			delayR_49.readwrite(allpass2R_43_45.tick2n(delayR_41.readwrite(
				delayR_40.readwrite(
					allpassmR_21_22.tick1(allpassmR_19_20.tick1(
						lpfRdamp_13_14.tick(
							(lpfR_in_64_65.tick(
								_predelayR.tick(in[1] * .5)) +
								_loopdecay * _lastL)))))))))));
		_lastL = lastL;
		_lastR = lastR;
		in[0] = (delayR_49.tap(468) * .438 + delayR_40.tap(625) * .938 - delayL_31.tap(312) * .438 +
			delayR_58.tap(8) * .125);
		in[1] = (delayL_31.tap(40) * .438 + delayL_23.tap(1) * .938 - delayR_49.tap(192) * .438 +
			delayL_37.read() * .125);

	}

	void check() {

		if (_hpcutold != *_hpcut) {
			_hpcutold = *_hpcut;
			lpfL_in_59_60.setNextHp(LOG2NORMALF(_hpcutold));
			lpfR_in_64_65.setNextHp(LOG2NORMALF(_hpcutold));

			lpfLdamp_11_12.setNextHp(LOG2NORMALF(_hpcutold));
			lpfRdamp_13_14.setNextHp(LOG2NORMALF(_hpcutold));

			lpfL_9_10.setNextHp(LOG2NORMALF(_hpcutold));
			lpfR_7_8.setNextHp(LOG2NORMALF(_hpcutold));

			out1_lpf.setNextHp(LOG2NORMALF(_hpcutold));
			out2_lpf.setNextHp(LOG2NORMALF(_hpcutold));

		}

		if (_lpcutold != *_lpcut) {
			_lpcutold = *_lpcut;
			lpfL_in_59_60.setNextLp(LOG2NORMALF(_lpcutold));
			lpfR_in_64_65.setNextLp(LOG2NORMALF(_lpcutold));

			lpfLdamp_11_12.setNextLp(LOG2NORMALF(_lpcutold));
			lpfRdamp_13_14.setNextLp(LOG2NORMALF(_lpcutold));

			lpfL_9_10.setNextLp(LOG2NORMALF(_lpcutold));
			lpfR_7_8.setNextLp(LOG2NORMALF(_lpcutold));

			out1_lpf.setNextLp(LOG2NORMALF(_lpcutold));
			out2_lpf.setNextLp(LOG2NORMALF(_lpcutold));

		}

		if (_predelayprev != *_predelay) {
			_predelayprev = *_predelay;
			_predelayL.setDelayMS(_predelayprev, _internalsr);
			_predelayR.setDelayMS(_predelayprev, _internalsr);
		}
	}


private:
	// int32_t _outtaps[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572,};
	static constexpr int32_t _outtaps[8] = { 468, 625, 312, 8, 40, 0, 192, 1572 };

	static constexpr MYFLOAT _internalsr = 34125.;

	MYFLOAT _totaldelay{}, _loopdecay{ .5 };
	MYFLOAT _lastL{}, _lastR{};
	ReverbButter1<MYFLOAT> lpfL_in_59_60, lpfR_in_64_65, lpfLdamp_11_12, lpfRdamp_13_14;
	ReverbButter1<MYFLOAT> lpfL_9_10, lpfR_7_8, out1_lpf, out2_lpf;
	Delay<MYFLOAT> delayL_16, delayL_23, delayL_31, delayL_37;
	Delay<MYFLOAT> delayR_49, delayR_ts, delayR_40, delayR_41, delayR_58;

	Ap1ModRndSpline<MYFLOAT> allpassmL_15_16, allpassmL_17_18, allpassmR_19_20, allpassmR_21_22;
	Ap2ModRndSpline<MYFLOAT> allpass2L_25_27, allpass2R_43_45;
	Ap3ModRndSpline<MYFLOAT> allpass3L_34_37, allpass3R_52_55;
	std::atomic<MYFLOAT>* _decay, * _lpcut, * _hpcut, * _mix, * _gain, * _predelay;
	MYFLOAT _olddamp{ -1 }, _olddec{ -1 }, _predelayprev{}, _lpcutold, _hpcutold;
	SimpleDelay2<MYFLOAT> _predelayL, _predelayR;
	DCBlocker<MYFLOAT> dcl, dcr;
};

class DatorroOrig : public DownUpSampler {
public:
	DatorroOrig(TRACK* t) : DownUpSampler(t, SPACE_REVERB3, STEREOEFFECT, t->_STATE->sr, _dattorroSampleRate),
		prelp(_dattorroSampleRate,
			LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])),
		damping1(_dattorroSampleRate,
			LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])),
		damping2(_dattorroSampleRate,
			LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])) {
		_lpcutold = *(_lpcut = &t->_STATE->params[t->index][REV3LPCUT]);
		_hpcutold = *(_hpcut = &t->_STATE->params[t->index][REV3HPCUT]);
		_mix = &t->_STATE->params[t->index][REV3MIX];
		_gain = &t->_STATE->params[t->index][REV3GAIN];
		_smooth2 = dbToLinear60(*_gain);
		_decay = &t->_STATE->params[t->index][REV3REF];
		_damp = &t->_STATE->params[t->index][REV3DAMP];
		_predelay = &t->_STATE->params[t->index][REV3PREDELAY];
		_predelayprev = *_predelay;
		_bypass = &t->bypass[SPACE_REVERB3];
		predelay.init(t->_STATE->sr * 1.05, _predelayprev * t->_STATE->sr * 0.001);
		//  prelp.setCoeff(1.0f - _damp->load() * .9f);
		//   _olddamp = _damp->load();
		//   prelp.setLPF_BW(0.05 + .45 - _olddamp * .45, 1);
		//damping1.setCoeff(_damp->load() * .9f);
		//damping2.setCoeff(_damp->load() * .9f);


		decaydiff11._appState = decaydiff12._appState = decaydiff21._appState = decaydiff22._appState = _appState;

		decaydiff11.init(_dattorroSampleRate, 672 / _dattorroSampleRate, 0.0005);
		decaydiff11.setRndRate(3.7334);
		decaydiff11.setDiff(-.7f);
		decaydiff12.init(_dattorroSampleRate, 908 / _dattorroSampleRate, 0.0007);
		decaydiff12.setRndRate(3.4234);
		decaydiff12.setDiff(-.7f);
		decaydiff21.init(_dattorroSampleRate, 1800 / _dattorroSampleRate, 0.001);
		decaydiff21.setRndRate(2.0234);
		decaydiff21.setDiff(.5f);
		decaydiff22.init(_dattorroSampleRate, 2656 / _dattorroSampleRate, 0.0015);
		decaydiff22.setRndRate(1.07453);
		decaydiff22.setDiff(.5f);

		/*
		decaydiff11.init(_STATE->sr, 672 * _scale, _STATE->sr * 0.0017, 1., 0, 1300 * _scale, -.7);
		decaydiff12.init(_STATE->sr, 908 * _scale, _STATE->sr * 0.0017, 1., .25, 1300 * _scale, -.7);
		decaydiff21.init(_STATE->sr, 1800 * _scale, _STATE->sr * 0.0017, 1., .5, 3000 * _scale, .5);
		decaydiff22.init(_STATE->sr, 2656 * _scale, _STATE->sr * 0.0017, 1., .75, 3000 * _scale, .5);
*/
		inputdiff11.init(142, .75);
		inputdiff12.init(107, .75);
		inputdiff21.init(379, .625);
		inputdiff22.init(277, .625);
		delay1.init(4453);
		delay2.init(3720);
		delay3.init(4217);
		delay4.init(3163);

		_totaldelay =
			delay1.getDelay() + delay2.getDelay() + delay3.getDelay() + delay4.getDelay() +
			673 + 907 + 1801 + 2657;

		tapl1 = 266;
		tapl2 = 2974;
		tapl3 = 1913;
		tapl4 = 1996;
		tapl5 = 1990;
		tapl6 = 187;
		tapl7 = 1066;
		tapr1 = 353;
		tapr2 = 3627;
		tapr3 = 1228;
		tapr4 = 2673;
		tapr5 = 2111;
		tapr6 = 335;
		tapr7 = 121;
	}


	inline void tick(MYFLOAT in[])override {
		MYFLOAT pre = inputdiff22.process(inputdiff21.process(inputdiff12.process(
			inputdiff11.process(
				prelp.tick(predelay.tick((in[0] + in[1]) * .5))))));
		MYFLOAT stateL = dcBlocker1.process(
			delay2.readwrite(decaydiff21.tick(_loopdecay * damping1.tick(
				delay1.readwrite(
					decaydiff11.tick(
						_loopdecay * _stateR +
						pre))))));
		MYFLOAT stateR = dcBlocker2.process(
			delay4.readwrite(decaydiff22.tick(_loopdecay * damping2.tick(
				delay3.readwrite(
					decaydiff12.tick(
						_loopdecay * _stateL +
						pre))))));
		UDD(stateL);
		UDD(stateR);
		_stateL = stateL;
		_stateR = stateR;

		MYFLOAT left = delay3.tap(tapl1);
		left += delay3.tap(tapl2);
		left -= decaydiff22.tap(tapl3);
		left += delay4.tap(tapl4);
		left -= delay1.tap(tapl5);
		left -= decaydiff21.tap(tapl6);
		left -= delay2.tap(tapl7);
		MYFLOAT right = delay1.tap(tapr1);
		right += delay1.tap(tapr2);
		right -= decaydiff21.tap(tapr3);
		right += delay2.tap(tapr4);
		right -= delay3.tap(tapr5);
		right -= decaydiff22.tap(tapr6);
		right -= delay4.tap(tapr7);
		in[0] = left * (1. / 7.);
		in[1] = right * (1. / 7.);
	}

	void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) override {
		MYFLOAT gain = dbToLinear60(*_gain), mix;
		if (*_bypass || destroyRequested) {
			mix = 0.f;
		}
		else {
			mix = *_mix;
		}
		check();
		_loopdecay = .6f + _decay->load() * .399f;
		loop(inl, inr, outl, outr, s, mix, gain);
	}

	void check() {
		if (_olddamp != _damp->load()) {
			//prelp.setCoeff(1.0f - _damp->load() * .9f);
			_olddamp = _damp->load();
			//   prelp.setLPF_BW(0.01 + .19 - _olddamp * .19, 1);

			// damping1.setCoeff(.8f + _olddamp  * .19f);
			// damping2.setCoeff(.8f + _olddamp  * .19f);
		}

		if (_predelayprev != *_predelay) {
			_predelayprev = *_predelay;
			predelay.setDelayMS(_predelayprev, _STATE->sr);
		}

		if (_hpcutold != *_hpcut) {
			_hpcutold = *_hpcut;
			prelp.setNextHp(LOG2NORMALF(_hpcutold));
			damping1.setNextHp(LOG2NORMALF(_hpcutold));
			damping2.setNextHp(LOG2NORMALF(_hpcutold));
		}

		if (_lpcutold != *_lpcut) {
			_lpcutold = *_lpcut;
			prelp.setNextLp(LOG2NORMALF(_lpcutold));
			damping1.setNextLp(LOG2NORMALF(_lpcutold));
			damping2.setNextLp(LOG2NORMALF(_lpcutold));
		}
	}

private:
	DCBlocker<MYFLOAT> dcBlocker1, dcBlocker2;
	SimpleDelay2<MYFLOAT> predelay;
	ReverbButter1<MYFLOAT> prelp;
	Ap<MYFLOAT> inputdiff11, inputdiff12, inputdiff21, inputdiff22;
	Delay<MYFLOAT> delay1, delay2, delay3, delay4;
	ApSincSpline<MYFLOAT> decaydiff11, decaydiff12;
	ApSincSpline<MYFLOAT> decaydiff21, decaydiff22;
	ReverbButter1<MYFLOAT> damping1, damping2;
	MYFLOAT _stateL{}, _stateR{};
	MYFLOAT _totaldelay{}, _loopdecay{ .5 };;
	static constexpr MYFLOAT _dattorroSampleRate = 29761.0;
	std::atomic<MYFLOAT>* _mix, * _gain, * _decay, * _damp, * _predelay, * _lpcut, * _hpcut;
	MYFLOAT _olddamp, _olddec{ -1000000 }, _predelayprev, _lpcutold, _hpcutold;
	int32_t tapl1;
	int32_t tapl2;
	int32_t tapl3;
	int32_t tapl4;
	int32_t tapl5;
	int32_t tapl6;
	int32_t tapl7;
	int32_t tapr1;
	int32_t tapr2;
	int32_t tapr3;
	int32_t tapr4;
	int32_t tapr5;
	int32_t tapr6;
	int32_t tapr7;
};


class Progenitor1 : public Effect {
public:
	Progenitor1(TRACK* t) : Effect(t, SPACE_REVERB4, STEREOEFFECT) {
		_scale = (_STATE->sr) / _internalsr;
		_internalsr = _STATE->sr;
		_decay = &_STATE->params[t->index][REV4REF];
		_lpcutold = *(_lpcut = &_STATE->params[t->index][REV4LPCUT]);
		_hpcutold = *(_hpcut = &_STATE->params[t->index][REV4HPCUT]);
		_mix = &_STATE->params[t->index][REV4MIX];
		_gain = &_STATE->params[t->index][REV4GAIN];
		_smooth2 = dbToLinear60(*_gain);
		_bypass = &t->bypass[SPACE_REVERB4];
		_predelay = &_STATE->params[t->index][REV4PREDELAY];
		_predelayprev = *_predelay;
		_predelayL.init(_STATE->sr * 1.05, _predelayprev * _STATE->sr * 0.001);
		_predelayR.init(_STATE->sr * 1.05, _predelayprev * _STATE->sr * 0.001);

		for (int32_t i = 0; i < 8; i++)
			_outtaps[i] = (int)(_outtaps[i] * _scale);

		/*

				allpassmL_15_16.init(239 * _scale, .375, 0, .406, 0, .406, .781, .219, _internalsr, 0.001,
									 3.123, 0.00052, 1.2,
									 0.0007, 2.5);
				allpassmR_19_20.init(205 * _scale, .375, 0, .406, 0, .406, .781, .219, _internalsr, 0.0012,
									 1.6544, 0.00052,
									 1.2, 0.0007, 2.5);

				allpassmL_17_18.init(392 * _scale, .312, 0, .406, 0, .406, .781, .219, _internalsr, 0.0015,
									 1.52392, 0.00052,
									 1.2, 0.0007, 2.5);
				allpassmR_21_22.init(329 * _scale, .312, 0, .406, 0, .406, .781, .219, _internalsr, 0.001,
									 2.5545694, 0.00052,
									 1.2, 0.0007, 2.5);
				//  void init(int32_t size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _STATE->sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

				allpass2L_25_27.init(612 * _scale, .25, 1944 * _scale, .406, 0, .406, .781, .219,
									 _internalsr, 0.0013, 1.6, 0.0009, 2.4554,
									 0.0005, 3.934);
				allpass2R_43_45.init(368 * _scale, .25, 2032 * _scale, .406, 0, .406, .781, .219,
									 _internalsr, 0.0015, 1.110, 0.0007, 3.973,
									 0.0006, 3.7343);

				allpass3L_34_37.init(1264 * _scale, .25, 816 * _scale, .25,
									 1212 * _scale, .406, .781, .219, _internalsr, 0.0009, 2.456,
									 0.0012,
									 1.425, 0.001, 1.546);
				allpass3R_52_55.init(1340 * _scale, .25, 688 * _scale, .25,
									 1452 * _scale, .406, .188, .812, _internalsr, 0.001, 2.924,
									 0.0011,
									 2.1353, 0.0013, 1.6234);

				*/
		allpassmL_15_16.init(findNextPrime(239 * _scale), .375, 0, .406, 0, .406, .781, .219,
			_internalsr, 0.001,
			3.123, 0.00052, 1.2,
			0.0007, 2.5);
		allpassmR_19_20.init(findNextPrime(205 * _scale), .375, 0, .406, 0, .406, .781, .219,
			_internalsr, 0.0012,
			1.6544, 0.00052,
			1.2, 0.0007, 2.5);

		allpassmL_17_18.init(findNextPrime(392 * _scale), .312, 0, .406, 0, .406, .781, .219,
			_internalsr, 0.0015,
			1.52392, 0.00052,
			1.2, 0.0007, 2.5);
		allpassmR_21_22.init(findNextPrime(329 * _scale), .312, 0, .406, 0, .406, .781, .219,
			_internalsr, 0.001,
			2.5545694, 0.00052,
			1.2, 0.0007, 2.5);
		//  void init(int32_t size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _STATE->sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

		allpass2L_25_27.init(findNextPrime(612 * _scale), .25, findNextPrime(1944 * _scale), .406,
			0, .406, .781, .219,
			_internalsr, 0.0013, 1.6, 0.0009, 1.4554,
			0.0005, 1.934);
		allpass2R_43_45.init(findNextPrime(368 * _scale), .25, findNextPrime(2032 * _scale), .406,
			0, .406, .781, .219,
			_internalsr, 0.0009, 1.110, 0.0007, 1.973,
			0.0006, 3.7343);

		allpass3L_34_37.init(findNextPrime(1264 * _scale), .25, findNextPrime(816 * _scale), .25,
			findNextPrime(1212 * _scale), .406, .781, .219, _internalsr, 0.0009,
			1.456,
			0.0010,
			1.425, 0.0007, 1.546);
		allpass3R_52_55.init(findNextPrime(1340 * _scale), .25, findNextPrime(688 * _scale), .25,
			findNextPrime(1452 * _scale), .406, .188, .812, _internalsr, 0.001,
			1.924,
			0.0011,
			1.1353, 0.0010, 1.6234);


		lpfL_in_59_60.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfR_in_64_65.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		lpfLdamp_11_12.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfRdamp_13_14.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		lpfL_9_10.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		lpfR_7_8.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		out1_lpf.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
		out2_lpf.init(_STATE->sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

		delayL_16.init(2 * _scale);
		delayL_23.init(findNextPrime(1055 * _scale));
		delayL_31.init(findNextPrime(344 * _scale));
		delayL_37.init(findNextPrime(1572 * _scale));
		delayR_40.init(findNextPrime(1460 * _scale));
		delayR_41.init(findNextPrime(835 * _scale));
		delayR_49.init(findNextPrime(500 * _scale));
		delayR_58.init(findNextPrime(16 * _scale));

		_totaldelay = delayL_16.getDelay() + delayL_23.getDelay() + delayL_31.getDelay() +
			delayL_37.getDelay() + delayR_40.getDelay() + delayR_41.getDelay() +
			delayR_49.getDelay() + delayR_58.getDelay() +
			allpassmL_15_16.getTotalDelay() + allpassmR_21_22.getTotalDelay() +
			allpassmR_19_20.getTotalDelay() + allpassmL_17_18.getTotalDelay() +
			allpass2L_25_27.getTotalDelay() + allpass2R_43_45.getTotalDelay() +
			allpass3L_34_37.getTotalDelay() + allpass3R_52_55.getTotalDelay();

	}

	void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) override {
		MYFLOAT gain, mix;
		if (*_bypass || destroyRequested) {
			gain = 1.f;
			mix = 0.f;
		}
		else {
			gain = dbToLinear60(*_gain);
			mix = *_mix;
		}
		check();
		const MYFLOAT decay = .6f + _decay->load() * .399f;


		UDF(_lastL);
		UDF(_lastR);

		for (int32_t i = 0; i < s; i++) {
			//delayL_37.readwrite(inl[i]);
			//outl[i] = outr[i] = delayL_37.tap(_outtaps[10]);
			//continue;
			// _rsl[i] = _lastR = allpass3L_34_37.tick2(.5 * rsl[i] + _lastR);
			// _rsr[i] = _lastL = allpass3R_52_55.tick2(.5 * rsr[i] + _lastR);
			MYFLOAT lastL = out1_lpf.tick(delayL_37.readwrite(allpass3L_34_37.tick3(
				delayL_31.readwrite(allpass2L_25_27.tick2(delayL_23.readwrite(
					allpassmL_17_18.tick1(
						delayL_16.readwrite(allpassmL_15_16.tick1(
							lpfLdamp_11_12.tick(
								(lpfL_in_59_60.tick(
									_predelayL.tick(inl[i] * .5f)) +
									decay * _lastR)))))))))));

			MYFLOAT lastR = out2_lpf.tick(delayR_58.readwrite(allpass3R_52_55.tick3(
				delayR_49.readwrite(allpass2R_43_45.tick2(delayR_41.readwrite(
					delayR_40.readwrite(
						allpassmR_21_22.tick1(allpassmR_19_20.tick1(
							lpfRdamp_13_14.tick(
								(lpfR_in_64_65.tick(
									_predelayR.tick(inr[i] * .5f)) +
									decay * _lastL)))))))))));
			_lastL = lastL;
			_lastR = lastR;
			/*
			MYFLOAT tapL = delayL_23.tap(_outtaps[8]) * 0.938f +
						 (delayL_31.tap(_outtaps[7]) - delayR_49.tap(_outtaps[9])) * 0.438f +
						 delayL_37.tap(_outtaps[10]) * 0.125f;
			MYFLOAT tapR = delayR_40.tap(_outtaps[2]) * 0.938f +
						 (delayR_49.tap(_outtaps[1]) - delayL_31.tap(_outtaps[3])) * 0.438f +
						 delayR_58.tap(_outtaps[4]) * 0.125f;
*/
			MYFLOAT tapL = delayR_49.tap(_outtaps[0]) * .438 + delayR_40.tap(_outtaps[1]) * .938 -
				delayL_31.tap(_outtaps[2]) * .438 + delayR_58.tap(_outtaps[3]) * .125;
			MYFLOAT tapR = delayL_31.tap(_outtaps[4]) * .438 + delayL_23.tap(_outtaps[5]) * .938 -
				delayR_49.tap(_outtaps[6]) * .438 + delayL_37.tap(_outtaps[7]) * .125;

			MYFLOAT mixsrc = 1.f - _smooth1;
			outl[i] = tapL * _smooth1 * _smooth2 + outl[i] * mixsrc;
			outr[i] = tapR * _smooth1 * _smooth2 + outr[i] * mixsrc;
			smmixgain(mix, gain);
		}
	}

	void tick(MYFLOAT inl, MYFLOAT inr, MYFLOAT* outl, MYFLOAT* outr) {
		//const MYFLOAT decay = _loopdecay; //_decay->load() * .999;


		UDF(_lastL);
		UDF(_lastR);

		MYFLOAT lastL = out1_lpf.tick(delayL_37.readwrite(allpass3L_34_37.tick3(
			delayL_31.readwrite(allpass2L_25_27.tick2(delayL_23.readwrite(
				allpassmL_17_18.tick1(
					delayL_16.readwrite(allpassmL_15_16.tick1(
						lpfLdamp_11_12.tick(
							(lpfL_in_59_60.tick(
								_predelayL.tick(inl * .5)) +
								_loopdecay * _lastR)))))))))));

		MYFLOAT lastR = out2_lpf.tick(delayR_58.readwrite(allpass3R_52_55.tick3(
			delayR_49.readwrite(allpass2R_43_45.tick2(delayR_41.readwrite(
				delayR_40.readwrite(
					allpassmR_21_22.tick1(allpassmR_19_20.tick1(
						lpfRdamp_13_14.tick(
							(lpfR_in_64_65.tick(
								_predelayR.tick(inr * .5)) +
								_loopdecay * _lastL)))))))))));
		_lastL = lastL;
		_lastR = lastR;
		/*
		MYFLOAT tapL = delayL_23.tap(_outtaps[8]) * 0.938f +
					 (delayL_31.tap(_outtaps[7]) - delayR_49.tap(_outtaps[9])) * 0.438f +
					 delayL_37.tap(_outtaps[10]) * 0.125f;
		MYFLOAT tapR = delayR_40.tap(_outtaps[2]) * 0.938f +
					 (delayR_49.tap(_outtaps[1]) - delayL_31.tap(_outtaps[3])) * 0.438f +
					 delayR_58.tap(_outtaps[4]) * 0.125f;
*/
		*outl = delayR_49.tap(_outtaps[0]) * .438 + delayR_40.tap(_outtaps[1]) * .938 -
			delayL_31.tap(_outtaps[2]) * .438 + delayR_58.tap(_outtaps[3]) * .125;
		*outr = delayL_31.tap(_outtaps[4]) * .438 + delayL_23.tap(_outtaps[5]) * .938 -
			delayR_49.tap(_outtaps[6]) * .438 + delayL_37.tap(_outtaps[7]) * .125;

	}

	void check() {

		if (_hpcutold != *_hpcut) {
			_hpcutold = *_hpcut;
			lpfL_in_59_60.setNextHp(LOG2NORMALF(_hpcutold));
			lpfR_in_64_65.setNextHp(LOG2NORMALF(_hpcutold));

			lpfLdamp_11_12.setNextHp(LOG2NORMALF(_hpcutold));
			lpfRdamp_13_14.setNextHp(LOG2NORMALF(_hpcutold));

			lpfL_9_10.setNextHp(LOG2NORMALF(_hpcutold));
			lpfR_7_8.setNextHp(LOG2NORMALF(_hpcutold));

			out1_lpf.setNextHp(LOG2NORMALF(_hpcutold));
			out2_lpf.setNextHp(LOG2NORMALF(_hpcutold));

		}

		if (_lpcutold != *_lpcut) {
			_lpcutold = *_lpcut;
			lpfL_in_59_60.setNextLp(LOG2NORMALF(_lpcutold));
			lpfR_in_64_65.setNextLp(LOG2NORMALF(_lpcutold));

			lpfLdamp_11_12.setNextLp(LOG2NORMALF(_lpcutold));
			lpfRdamp_13_14.setNextLp(LOG2NORMALF(_lpcutold));

			lpfL_9_10.setNextLp(LOG2NORMALF(_lpcutold));
			lpfR_7_8.setNextLp(LOG2NORMALF(_lpcutold));

			out1_lpf.setNextLp(LOG2NORMALF(_lpcutold));
			out2_lpf.setNextLp(LOG2NORMALF(_lpcutold));

		}

		if (_predelayprev != *_predelay) {
			_predelayprev = *_predelay;
			_predelayL.setDelayMS(_predelayprev, _STATE->sr);
			_predelayR.setDelayMS(_predelayprev, _STATE->sr);
		}
	}


private:


	// int32_t _outtaps[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572,};
	int32_t _outtaps[8] = { 468, 625, 312, 8, 40, 0, 192, 1572 };

	MYFLOAT _internalsr = 34125.;

	MYFLOAT _scale{}, _totaldelay{}, _loopdecay{ .5 };
	MYFLOAT _lastL{}, _lastR{};
	ReverbButter1<MYFLOAT> lpfL_in_59_60, lpfR_in_64_65, lpfLdamp_11_12, lpfRdamp_13_14;
	ReverbButter1<MYFLOAT> lpfL_9_10, lpfR_7_8, out1_lpf, out2_lpf;
	Delay<MYFLOAT> delayL_16, delayL_23, delayL_31, delayL_37;
	Delay<MYFLOAT> delayR_49, delayR_40, delayR_41, delayR_58;

	ApModRnd<MYFLOAT> allpassmL_15_16, allpassmL_17_18, allpassmR_19_20, allpassmR_21_22;
	ApModRnd<MYFLOAT> allpass2L_25_27, allpass2R_43_45;
	ApModRnd<MYFLOAT> allpass3L_34_37, allpass3R_52_55;
	std::atomic<MYFLOAT>* _decay, * _lpcut, * _hpcut, * _mix, * _gain, * _predelay;
	MYFLOAT _olddamp{ -1 }, _olddec{ -1 }, _predelayprev{}, _lpcutold, _hpcutold;
	SimpleDelay2<MYFLOAT> _predelayL, _predelayR;
	DCBlocker<MYFLOAT> dcl, dcr;
};

class Progenitor1Dark : public Effect {
public:
	Progenitor1Dark(TRACK* t) : Effect(t, SPACE_REVERB3, STEREOEFFECT) {
		_scale = (_STATE->sr / 2.) / _internalsr;
		_internalsr = _STATE->sr / 2.;
		_decay = &_STATE->params[t->index][REV3REF];
		_mix = &_STATE->params[t->index][REV3MIX];
		_gain = &_STATE->params[t->index][REV3GAIN];
		_smooth2 = dbToLinear60(*_gain);
		_bypass = &t->bypass[SPACE_REVERB3];


		for (int32_t i = 0; i < 8; i++)
			_outtaps[i] = _outtaps[i] * _scale;

		//        MYFLOAT sr, int32_t delay, T c, MYFLOAT rate, int depth, MYFLOAT randfact = 0.0, MYFLOAT phase = 0.0,
				//              T decay = 1.0
		allpassmL_15_16.init(239 * _scale, .375, 0, .406, 0, .406, .781, .219, _internalsr, 0.001,
			3.123, 0.00052, 1.2,
			0.0007, 2.5);
		allpassmR_19_20.init(205 * _scale, .375, 0, .406, 0, .406, .781, .219, _internalsr, 0.0012,
			1.6544, 0.00052,
			1.2, 0.0007, 2.5);

		allpassmL_17_18.init(392 * _scale, .312, 0, .406, 0, .406, .781, .219, _internalsr, 0.0015,
			1.52392, 0.00052,
			1.2, 0.0007, 2.5);
		allpassmR_21_22.init(329 * _scale, .312, 0, .406, 0, .406, .781, .219, _internalsr, 0.001,
			2.5545694, 0.00052,
			1.2, 0.0007, 2.5);
		//  void init(int32_t size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _STATE->sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

		allpass2L_25_27.init(612 * _scale, .25, 1944 * _scale, .406, 0, .406, .781, .219,
			_internalsr, 0.0013, 1.6, 0.0009, 2.4554,
			0.0005, 3.934);
		allpass2R_43_45.init(368 * _scale, .25, 2032 * _scale, .406, 0, .406, .781, .219,
			_internalsr, 0.0015, 1.110, 0.0007, 3.973,
			0.0006, 3.7343);

		allpass3L_34_37.init(1264 * _scale, .25, 816 * _scale, .25,
			1212 * _scale, .406, .781, .219, _internalsr, 0.0009, 2.456,
			0.0012,
			1.425, 0.001, 1.546);
		allpass3R_52_55.init(1340 * _scale, .25, 688 * _scale, .25,
			1452 * _scale, .406, .188, .812, _internalsr, 0.001, 2.924,
			0.0011,
			2.1353, 0.0013, 1.6234);
		_totaldelay =
			(2 + 1055 + 344 + 1572 + 625 + 835 + 500 + 16 + 239 + 205 + 392 + 329 + 1944 + 612 +
				2032 + 368 + 1212 + 816 + 1264 + 1452 + 688 + 1340) * _scale;


		lpfL_in_59_60.setCoeff(.5);
		lpfR_in_64_65.setCoeff(.5);

		lpfLdamp_11_12.setCoeff(.5);
		lpfRdamp_13_14.setCoeff(.5);

		lpfL_9_10.setCoeff(.5);
		lpfR_7_8.setCoeff(.5);

		out1_lpf.setCoeff(.5);
		out2_lpf.setCoeff(.5);

		delayL_16.init(2 * _scale);
		delayL_23.init(1055 * _scale);
		delayL_31.init(344 * _scale);
		delayL_37.init(1572 * _scale + 5);
		delayR_40.init(625 * _scale + 5);
		delayR_41.init(835 * _scale);
		delayR_49.init(500 * _scale);
		delayR_58.init(16 * _scale);
	}

	void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) override {
		MYFLOAT gain = dbToLinear60(*_gain), mix;
		if (*_bypass || destroyRequested) {
			mix = 0.f;
		}
		else {
			mix = *_mix;
		}
		check();

		for (int32_t i = 0; i < s; i += 2) {
			//delayL_37.readwrite(inl[i]);
			//outl[i] = outr[i] = delayL_37.tap(_outtaps[10]);
			//continue;
			// _rsl[i] = _lastR = allpass3L_34_37.tick2(.5 * rsl[i] + _lastR);
			// _rsr[i] = _lastL = allpass3R_52_55.tick2(.5 * rsr[i] + _lastR);

			MYFLOAT lastL = dccutL.process(
				out1_lpf.tickbw(delayL_37.readwrite(allpass3L_34_37.tick3(
					delayL_31.readwrite(allpass2L_25_27.tick2(delayL_23.readwrite(
						allpassmL_17_18.tick1(
							delayL_16.readwrite(allpassmL_15_16.tick1(
								lpfLdamp_11_12.tickdamping(
									(lpfL_in_59_60.tickbw(inl[i] * .5f) +
										_loopdecay * _lastR))))))))))));

			MYFLOAT lastR = dccutR.process(
				out2_lpf.tickbw(delayR_58.readwrite(allpass3R_52_55.tick3(
					delayR_49.readwrite(allpass2R_43_45.tick2(delayR_41.readwrite(
						delayR_40.readwrite(
							allpassmR_21_22.tick1(allpassmR_19_20.tick1(
								lpfRdamp_13_14.tickdamping(
									(lpfR_in_64_65.tickbw(inr[i] * .5f) +
										_loopdecay * _lastL))))))))))));
			_lastL = lastL;
			_lastR = lastR;
			/*
			MYFLOAT tapL = delayL_23.tap(_outtaps[8]) * 0.938f +
						 (delayL_31.tap(_outtaps[7]) - delayR_49.tap(_outtaps[9])) * 0.438f +
						 delayL_37.tap(_outtaps[10]) * 0.125f;
			MYFLOAT tapR = delayR_40.tap(_outtaps[2]) * 0.938f +
						 (delayR_49.tap(_outtaps[1]) - delayL_31.tap(_outtaps[3])) * 0.438f +
						 delayR_58.tap(_outtaps[4]) * 0.125f;
			*/
			MYFLOAT tapL = delayR_49.tap(_outtaps[0]) * .438f + delayR_40.tap(_outtaps[1]) * .938f -
				delayL_31.tap(_outtaps[2]) * .438f + delayR_58.tap(_outtaps[3]) * .125f;
			MYFLOAT tapR = delayL_31.tap(_outtaps[4]) * .438f + delayL_23.tap(_outtaps[5]) * .938f -
				delayR_49.tap(_outtaps[6]) * .438f + delayL_37.tap(_outtaps[7]) * .125f;


			MYFLOAT mixsrc = 1.f - _smooth1;
			outl[i] = tapL * _smooth1 * _smooth2 + outl[i] * mixsrc;
			outr[i] = tapR * _smooth1 * _smooth2 + outr[i] * mixsrc;
			smmixgain(mix, gain);
			outl[i + 1] = tapL * _smooth1 * _smooth2 + outl[i + 1] * mixsrc;
			outr[i + 1] = tapR * _smooth1 * _smooth2 + outr[i + 1] * mixsrc;
			smmixgain(mix, gain);
		}
	}

	void check() {
		if (_olddamp != _damp->load()) {
			_olddamp = _damp->load();

			lpfL_in_59_60.setCoeff(1.0f - _damp->load() * .9f);
			lpfR_in_64_65.setCoeff(1.0f - _damp->load() * .9f);
			out1_lpf.setCoeff(1.0f - _damp->load() * .9f);
			out2_lpf.setCoeff(1.0f - _damp->load() * .9f);

			lpfLdamp_11_12.setCoeff(_damp->load() * .9f);
			lpfRdamp_13_14.setCoeff(_damp->load() * .9f);
			lpfL_9_10.setCoeff(_damp->load() * .9f);
			lpfR_7_8.setCoeff(_damp->load() * .9f);
		}
		if (_olddec != _decay->load()) {
			_olddec = _decay->load();
			resetdecay();
		}
	}

	void resetdecay() {
		MYFLOAT T60 = LOG2NORMALF(_olddec);
		_loopdecay = powf(0.001f, (_totaldelay) / (T60 * _internalsr));
	}


private:
	int32_t _outtaps[8] = { 468, 625, 312, 8, 40, 0, 192, 1572 };

	//int32_t _outtaps[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572,};
	MYFLOAT _internalsr = 34125.0;

	MYFLOAT _scale{}, _totaldelay{}, _loopdecay{ .5f };
	MYFLOAT _lastL{}, _lastR{};
	DCBlocker<MYFLOAT> dccutL, dccutR;
	OnePoleLp<MYFLOAT> lpfL_in_59_60, lpfR_in_64_65, lpfLdamp_11_12, lpfRdamp_13_14;
	OnePoleLp<MYFLOAT> lpfL_9_10, lpfR_7_8, out1_lpf, out2_lpf;
	Delay<MYFLOAT> delayL_16, delayL_23, delayL_31, delayL_37;
	Delay<MYFLOAT> delayR_49, delayR_ts, delayR_40, delayR_41, delayR_58;

	ApModRnd<MYFLOAT> allpassmL_15_16, allpassmL_17_18, allpassmR_19_20, allpassmR_21_22;
	ApModRnd<MYFLOAT> allpass2L_25_27, allpass2R_43_45;
	ApModRnd<MYFLOAT> allpass3L_34_37, allpass3R_52_55;
	std::atomic<MYFLOAT>* _decay, * _damp, * _mix, * _gain;
	MYFLOAT _olddamp{ -1 }, _olddec{ -1 };
	SimpleDelay2<MYFLOAT> predelayL, predelayR;
};

class Datorro1 : public Effect {
public:
	Datorro1(TRACK* t) : Effect(t, SPACE_REVERB3, STEREOEFFECT),
		prelp(_STATE->sr, LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])),
		damping1(_STATE->sr, LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])),
		damping2(_STATE->sr, LOG2NORMALF(_STATE->params[t->index][REV3HPCUT]),
			LOG2NORMALF(_STATE->params[t->index][REV3LPCUT])) {
		_scale = _STATE->sr / _dattorroSampleRate;
		_lpcutold = *(_lpcut = &_STATE->params[t->index][REV3LPCUT]);
		_hpcutold = *(_hpcut = &_STATE->params[t->index][REV3HPCUT]);
		_mix = &_STATE->params[t->index][REV3MIX];
		_gain = &_STATE->params[t->index][REV3GAIN];
		_smooth2 = dbToLinear60(*_gain);
		_decay = &_STATE->params[t->index][REV3REF];
		_damp = &_STATE->params[t->index][REV3DAMP];
		_predelay = &_STATE->params[t->index][REV3PREDELAY];
		_predelayprev = *_predelay;
		_bypass = &t->bypass[SPACE_REVERB3];
		predelay.init(_STATE->sr * 1.05, _predelayprev * _STATE->sr * 0.001);
		//  prelp.setCoeff(1.0f - _damp->load() * .9f);
		//   _olddamp = _damp->load();
		//   prelp.setLPF_BW(0.05 + .45 - _olddamp * .45, 1);
		//damping1.setCoeff(_damp->load() * .9f);
		//damping2.setCoeff(_damp->load() * .9f);
		decaydiff11.init(_STATE->sr, findNextPrime(673 * _scale) / _STATE->sr, 0.0005);
		decaydiff11.setRndRate(3.7334f);
		decaydiff11.setDiff(-.7f);
		decaydiff12.init(_STATE->sr, findNextPrime(907 * _scale) / _STATE->sr, 0.0007);
		decaydiff12.setRndRate(3.4234f);
		decaydiff12.setDiff(-.7f);
		decaydiff21.init(_STATE->sr, findNextPrime(1801 * _scale) / _STATE->sr, 0.001);
		decaydiff21.setRndRate(2.0234f);
		decaydiff21.setDiff(.5f);
		decaydiff22.init(_STATE->sr, findNextPrime(2657 * _scale) / _STATE->sr, 0.0015);
		decaydiff22.setRndRate(1.07453);
		decaydiff22.setDiff(.5f);

		/*
		decaydiff11.init(_STATE->sr, 672 * _scale, _STATE->sr * 0.0017, 1., 0, 1300 * _scale, -.7);
		decaydiff12.init(_STATE->sr, 908 * _scale, _STATE->sr * 0.0017, 1., .25, 1300 * _scale, -.7);
		decaydiff21.init(_STATE->sr, 1800 * _scale, _STATE->sr * 0.0017, 1., .5, 3000 * _scale, .5);
		decaydiff22.init(_STATE->sr, 2656 * _scale, _STATE->sr * 0.0017, 1., .75, 3000 * _scale, .5);
*/
		inputdiff11.init(findNextPrime(141 * _scale), .75);
		inputdiff12.init(findNextPrime(107 * _scale), .75);
		inputdiff21.init(findNextPrime(383 * _scale), .625);
		inputdiff22.init(findNextPrime(281 * _scale), .625);
		delay1.init(findNextPrime(4451 * _scale));
		delay2.init(findNextPrime(3719 * _scale));
		delay3.init(findNextPrime(4219 * _scale));
		delay4.init(findNextPrime(3167 * _scale));

		_totaldelay =
			(delay1.getDelay() + delay2.getDelay() + delay3.getDelay() + delay4.getDelay() +
				673 * _scale + 907 * _scale + 1801 * _scale + 2657 * _scale);

		tapl1 = findNextPrime(266 * _scale);
		tapl2 = findNextPrime(2954 * _scale);
		tapl3 = findNextPrime(1913 * _scale);
		tapl4 = findNextPrime(1996 * _scale);
		tapl5 = findNextPrime(1990 * _scale);
		tapl6 = findNextPrime(187 * _scale);
		tapl7 = findNextPrime(1066 * _scale);
		tapr1 = findNextPrime(353 * _scale);
		tapr2 = findNextPrime(3627 * _scale);
		tapr3 = findNextPrime(1228 * _scale);
		tapr4 = findNextPrime(2673 * _scale);
		tapr5 = findNextPrime(2111 * _scale);
		tapr6 = findNextPrime(335 * _scale);
		tapr7 = findNextPrime(121 * _scale);


	}

	void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) override {
		MYFLOAT gain = dbToLinear60(*_gain), mix;
		if (*_bypass || destroyRequested) {
			mix = 0.f;
		}
		else {
			mix = *_mix;
		}
		check();
		const MYFLOAT decay = .6f + _decay->load() * .399f;
		for (int32_t i = 0; i < s; i++) {
			// outl[i] = outr[i] = decaydiff11.tick((inl[i] + inr[i]) * .5f);

			MYFLOAT pre = inputdiff22.process(inputdiff21.process(inputdiff12.process(
				inputdiff11.process(
					prelp.tick(predelay.tick((inl[i] + inr[i]) * .5f))))));
			MYFLOAT stateL = dcBlocker1.process(
				delay2.readwrite(decaydiff21.tickok(decay * damping1.tick(
					delay1.readwrite(
						decaydiff11.tickok(
							decay * _stateR +
							pre))))));
			MYFLOAT stateR = dcBlocker2.process(
				delay4.readwrite(decaydiff22.tickok(decay * damping2.tick(
					delay3.readwrite(
						decaydiff12.tickok(
							decay * _stateL +
							pre))))));
			_stateL = stateL;
			_stateR = stateR;


			MYFLOAT left = delay3.tap(tapl1);
			left += delay3.tap(tapl2);
			left -= decaydiff22.tap(tapl3);
			left += delay4.tap(tapl4);
			left -= delay1.tap(tapl5);
			left -= decaydiff21.tap(tapl6);
			left -= delay2.tap(tapl7);
			MYFLOAT right = delay1.tap(tapr1);
			right += delay1.tap(tapr2);
			right -= decaydiff21.tap(tapr3);
			right += delay2.tap(tapr4);
			right -= delay3.tap(tapr5);
			right -= decaydiff22.tap(tapr6);
			right -= delay4.tap(tapr7);

			const MYFLOAT mixsrc = 1.f - _smooth1;
			outl[i] = left * _smooth1 * _smooth2 * .6f + inl[i] * mixsrc;
			outr[i] = right * _smooth1 * _smooth2 * .6f + inr[i] * mixsrc;
			smmixgain(mix, gain);
		}
	}

	void check() {
		if (_olddamp != _damp->load()) {
			//prelp.setCoeff(1.0f - _damp->load() * .9f);
			_olddamp = _damp->load();
			//   prelp.setLPF_BW(0.01 + .19 - _olddamp * .19, 1);

			// damping1.setCoeff(.8f + _olddamp  * .19f);
			// damping2.setCoeff(.8f + _olddamp  * .19f);
		}

		if (_predelayprev != *_predelay) {
			_predelayprev = *_predelay;
			predelay.setDelayMS(_predelayprev, _STATE->sr);
		}

		if (_hpcutold != *_hpcut) {
			_hpcutold = *_hpcut;
			prelp.setNextHp(LOG2NORMALF(_hpcutold));
			damping1.setNextHp(LOG2NORMALF(_hpcutold));
			damping2.setNextHp(LOG2NORMALF(_hpcutold));
		}

		if (_lpcutold != *_lpcut) {
			_lpcutold = *_lpcut;
			prelp.setNextLp(LOG2NORMALF(_lpcutold));
			damping1.setNextLp(LOG2NORMALF(_lpcutold));
			damping2.setNextLp(LOG2NORMALF(_lpcutold));
		}
	}

private:
	DCBlocker<MYFLOAT> dcBlocker1, dcBlocker2;
	SimpleDelay2<MYFLOAT> predelay;
	ReverbButter1<MYFLOAT> prelp;
	Ap<MYFLOAT> inputdiff11, inputdiff12, inputdiff21, inputdiff22;
	Delay<MYFLOAT> delay1, delay2, delay3, delay4;
	Ap1<MYFLOAT> decaydiff11, decaydiff12;
	Ap1<MYFLOAT> decaydiff21, decaydiff22;
	ReverbButter1<MYFLOAT> damping1, damping2;
	MYFLOAT _stateL{}, _stateR{};
	MYFLOAT _scale{}, _totaldelay{}, _loopdecay{ .5f };;
	const MYFLOAT _dattorroSampleRate = 29761.0;
	std::atomic<MYFLOAT>* _mix, * _gain, * _decay, * _damp, * _predelay, * _lpcut, * _hpcut;
	MYFLOAT _olddamp, _olddec{ -1000000 }, _predelayprev, _lpcutold, _hpcutold;
	int32_t tapl1;
	int32_t tapl2;
	int32_t tapl3;
	int32_t tapl4;
	int32_t tapl5;
	int32_t tapl6;
	int32_t tapl7;
	int32_t tapr1;
	int32_t tapr2;
	int32_t tapr3;
	int32_t tapr4;
	int32_t tapr5;
	int32_t tapr6;
	int32_t tapr7;
};


#endif //GRAINSTORM_REVERBPROGENITOR_H
