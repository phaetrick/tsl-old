//
// Created by pr on 23.01.19.
//

#include <cstdint>
#include <csignal>
#include "callbacks_fxpower.h"
#include "grainstorm.h"
#include "saturator.h"
#include "phaser.h"
#include "filter.h"
#include "eq.h"
#include "oscil.h"
#include "pv.h"
#include "compressor.h"
#include "vocoder.h"
#include "Convolver.h"
#include "synth.h"
#include "Modal.h"
#include "Shimmer.h"
#include "Reverb.h"
#include "reverbroom.h"
#include "reverbprogenitor.h"
#include "dplimit1.h"
#include "distortion.h"
#include "pitchdetect.h"
#include "delay.h"
#include "Noise.h"
#include "Vox.h"
#include "Vox2.h"
#include "GrainWaveset.h"
#include "GrainDrive.h"
#include "GrainDisperse.h"
#include "GrainVowel.h"
#include "GrainPluck.h"
#include "pitchmap.h"
#include "gs_common.h"
#if GS_ENABLE_PITCHMAP2
#include "pitchmap2.h"
#endif

static bool checkMonoEffect(TRACK* track, int32_t id, bool power) {
	auto _appState = track->_appState;
	for (int32_t i = 0; i < _STATE->channels; i++) {
		auto n = track->fx_queue[i]._first;
		while (n) {
			if (n->data->_id == id) {
				if (power)
					n->data->destroyRequested = n->data->readyToDestroy = false;
				else
					n->data->deactivate();
				if (i == _STATE->channels - 1) {
					return false;
				}
			}
			n = n->next;
		}

	}
	return power;
}

static bool checkGrainEffect(TRACK* track, int32_t id, bool power) {
	auto _appState = track->_appState;
	for (int32_t i = 0; i < _STATE->channels; i++) {
		auto n = track->fx_queue_grain[i]._first;
		while (n) {
			if (n->data->_id == id) {
				if (power)
					n->data->destroyRequested = n->data->readyToDestroy = false;
				else
					n->data->deactivate();
				if (i == _STATE->channels - 1) {
					return false;
				}
			}
			n = n->next;
		}

	}
	return power;
}

static bool checkStereoEffect(TRACK* track, int32_t id, bool power) {
	auto n = track->fx_queue_stereo._first;
	while (n) {
		if (n->data->_id == id) {
			if (power)
				n->data->destroyRequested = n->data->readyToDestroy = false;
			else
				n->data->deactivate();
			return false;
		}
		n = n->next;
	}
	return power;
}


static bool callback_offbutton_grainvco(TRACK* track, bool power) {
	//    test();
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINVCO;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainVCO>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_grainfilter(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINFILTER;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainFilter>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

// The five below all follow the GRAINFILTER pattern above: one instance per
// channel on fx_queue_grain, reaped through destroyRequested.
static bool callback_offbutton_grainwaveset(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINWAVESET;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainWaveset>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_graindrive(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINDRIVE;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainDrive>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_graindisperse(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINDISPERSE;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainDisperse>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_grainvowel(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINVOWEL;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainVowel>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_grainpluck(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_GRAINPLUCK;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainPluck>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_sat(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_SATURATOR;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<SATURATOR>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);
}

static bool callback_offbutton_looper(TRACK* track, bool power) {
	auto _appState = track->_appState;
	return track->fxpower[SPACE_LOOPER].exchange(power);
	
}


static bool callback_offbutton_moogladder(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_MOOGLADDER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<MOOGLADDER>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_eq(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_EQ;
	auto _appState = track->_appState;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Eq10>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_delay(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_DELAY;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<TapDelay>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_mdelay(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_MDELAY;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<MDELAY>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_phaser4(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PHASER4;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PHASER4>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_phaser(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PHASER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PHASER>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_flanger(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_FLANGER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<FLANGER>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_filter(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_HPLP;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<ButterLPHP>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_bandpass_grain(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_GRAIN_BP;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<RESONGRAIN>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_rm_grain(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_RM_GRAIN;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<RINGG>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_grainreverb(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_GRAINREVERB;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<SimpleReverb>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_grainmodal(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_GRAINMODAL;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Modal>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_grainpart(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_GRAINPART;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<GrainPart>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_vox(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_VOX;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Vox>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_vox2(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_VOX2;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Vox2>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_specdel(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_SPECDEL;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<SpectralDelay>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_specdel2(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_SPECDEL2;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<SpectralDelay2>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_eq5(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_EQ5;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Eq5>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}
static bool callback_offbutton_dyneq5(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_DYNEQ5;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<DynEq<double>>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}
static bool callback_offbutton_bandpass_final(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_BANDPASS;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<RESON>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_bandreject_final(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_BANDREJECT;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<ButterBR>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pitchshift(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_SHIFTER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PITCHSHIFT>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_ssb(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_SSB;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<FreqShift>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_distort(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_DISTORT;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<DISTORT>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_bc(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_BITCRUSHER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<BitCrusher>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pitch(TRACK* track, bool power) {
	return track->fxpower[SPACE_PITCH].exchange(power);
}


static bool callback_offbutton_grainseq(TRACK* track, bool power) {
	return track->fxpower[SPACE_GRAIN_SEQUENCER].exchange(power);
}

static bool callback_offbutton_compressor(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_COMPRESSION;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<compfullmono>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_vocoder(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_VOCODER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Vocoder>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_lpcvocoder(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_LPCVOCODER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<LPCVocoder5>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_spectral_filter(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_SPECTRAL_FILTER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<SpectralFilter>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pvamps(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PVAMPS;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PVAmps>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pitchmap(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PITCHMAP;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PitchMap>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

#if GS_ENABLE_PITCHMAP2
static bool callback_offbutton_pitchmap2(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PITCHMAP2;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PitchMap2>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}
#endif

static bool callback_offbutton_convolver(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_CONVOLVER;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Convolver>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_reverb1(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB1;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<Freeverb>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_reverb2(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB2;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<NRev>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_reverb3(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB3;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<DatorroOrig>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_reverb4(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB4;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<Progenitor1Orig>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_reverb5(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB5;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<REVERB5>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_reverb6(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_REVERB6;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<ShimmerDark>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_limiter(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_LIMITER;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<Dplimit1<MYFLOAT>>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}
#include "chorus.h"

static bool callback_offbutton_chorus(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_CHORUS;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<Chorus>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);
}


static bool callback_offbutton_pingpong(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_PINGPONG;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<PingPongDelay>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_stc(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_STC;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<compfullstereo>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

#include "filt.h"

static bool callback_offbutton_spectrum(TRACK* track, bool power) {
	return false;
}
static bool callback_offbutton_dcs(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_DCS;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<DCBLOCKER>(track);
		//    auto newfx = std::make_shared<SpectrumAnalyzer>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}
static bool callback_offbutton_monostereo(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_MONOSTEREO;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<StereoMix>(track);
		//    auto newfx = std::make_shared<SpectrumAnalyzer>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}
#include "physmod.h"

static bool callback_offbutton_bowed(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_BOWED;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<Bowed>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pdetectgrain(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_PDETECTGRAIN;
	if (checkGrainEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PitchDetectGrain>(track, i,
				&_STATE->params[track->index][PDETECTGRAINSMOOTH2],
				&_STATE->params[track->index][PDETECTGRAINPRELP],
				&_STATE->params[track->index][PDETECTGRAINA],
				&_STATE->params[track->index][PDETECTGRAINB],
				&_STATE->params[track->index][PDETECTGRAINTRANSPOSE],
				&_STATE->params[track->index][
					PITCHDETECTGRAINFXTRACKLASTPITCH0 +
						i], &_STATE->params[track->index][
							PITCHDETECTGRAINFXTRACKOUT0 + i],
						&track->bypass[SPACE_PDETECTGRAIN]);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_pdetect(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_PDETECT;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<PitchDetect>(track, i,
				&_STATE->params[track->index][PDETECTSMOOTH2],
				&_STATE->params[track->index][PDETECTPRELP],
				&_STATE->params[track->index][PDETECTA],
				&_STATE->params[track->index][PDETECTB],
				&_STATE->params[track->index][PDETECTTRANSPOSE],
				&_STATE->params[track->index][
					PITCHDETECTFXTRACKLASTPITCH0 +
						i],
				&_STATE->params[track->index][PITCHDETECTFXTRACKOUT0 +
				i],
				&_STATE->params[track->index][PITCHDETECTFXTRACK],
				track->pitchdetectoutbuf[i],
				&track->bypass[SPACE_PDETECT]);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}
#include "fm.h"

static bool callback_offbutton_fm(TRACK* track, bool power) {
	auto _appState = track->_appState;

	const int32_t fxnum = SPACE_FM;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<FM>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}


static bool callback_offbutton_pv(TRACK* track, bool power) {
	auto _appState = track->_appState;

	return track->fxpower[SPACE_PV_MAIN].exchange(power);
	}

static bool callback_offbutton_cross(TRACK* track, bool power) {
	auto _appState = track->_appState;
	TRACK* dest = track->destinationz;
	if (power) {
		if (dest->fxpower[SPACE_CROSS_MAIN].load() ||
			track->lockerz->fxpower[SPACE_CROSS_MAIN].load()) {
			showToast(_STATE, "Cross Synthesis: Chaining not possible.");
			return track->fxpower[SPACE_CROSS_MAIN].exchange(false);
		}
		else
			return track->fxpower[SPACE_CROSS_MAIN].exchange(true);
	}
	else {
		return track->fxpower[SPACE_CROSS_MAIN].exchange(false);
	}
}

#include "ModalReverb.h"
static bool callback_offbutton_modalrev(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_MODALREV;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<ModalReverbDownSampled>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}
static bool callback_offbutton_clip(TRACK* track, bool power) {
	const int32_t fxnum = SPACE_CLIPPER;
	if (checkStereoEffect(track, fxnum, power)) {
		auto newfx = std::make_shared<Clipper>(track);
		newfx->activate();
	}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_modal(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_MODAL;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<ModalEffect>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_mcomp(TRACK* track, bool power) {
	auto _appState = track->_appState;
	const int32_t fxnum = SPACE_MULTICOMP;
	if (checkMonoEffect(track, fxnum, power))
		for (int32_t i = 0; i < _STATE->channels; i++) {
			auto newfx = std::make_shared<MultiBandCompressor<double>>(track, i);
			newfx->activate();
		}
	return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_arp(TRACK* track, bool power) {
	auto old = track->fxpower[SPACE_ARP].exchange(power);
	if (power)
		track->grainsequencer.arpReset();
	return old;
}

static bool callback_offbutton_noisemono(TRACK* track, bool power) {
    auto _appState = track->_appState;

    const int32_t fxnum = NOISEMONOEFFECT;
    if (checkMonoEffect(track, fxnum, power))
        for (int32_t i = 0; i < _STATE->channels; i++) {
            auto newfx = std::make_shared<NoiseEffect>(track, i, Effect::MONOEFFECT);
            newfx->activate();
        }
    return track->fxpower[fxnum].exchange(power);

}

static bool callback_offbutton_noisegrain(TRACK* track, bool power) {
    auto _appState = track->_appState;

    const int32_t fxnum = NOISEGRAINEFFECT;
    if (checkGrainEffect(track, fxnum, power))
        for (int32_t i = 0; i < _STATE->channels; i++) {
            auto newfx = std::make_shared<NoiseEffect>(track, i, Effect::GRAINEFFECT);
            newfx->activate();
        }
	return track->fxpower[fxnum].exchange(power);

}


void setup_fx_callbacks(callback_fx_power* cbs) {
    cbs[NOISEMONOEFFECT] = callback_offbutton_noisemono;
    cbs[NOISEGRAINEFFECT] = callback_offbutton_noisegrain;

	cbs[SPACE_ARP] = callback_offbutton_arp;
	cbs[SPACE_MONOSTEREO] = callback_offbutton_monostereo;
	cbs[SPACE_GRAINSETTINGS] = nullptr;
	cbs[SPACE_PV_MAIN] = callback_offbutton_pv;
	cbs[SPACE_CROSS_MAIN] = callback_offbutton_cross;
	cbs[SPACE_PITCH] = callback_offbutton_pitch;
	cbs[SPACE_GRAIN_BP] = callback_offbutton_bandpass_grain;
	cbs[SPACE_RM_GRAIN] = callback_offbutton_rm_grain;
	cbs[SPACE_SHIFTER] = callback_offbutton_pitchshift;
	cbs[SPACE_SSB] = callback_offbutton_ssb;
	cbs[SPACE_PHASER] = callback_offbutton_phaser;
	cbs[SPACE_PHASER4] = callback_offbutton_phaser4;
	cbs[SPACE_FLANGER] = callback_offbutton_flanger;
	cbs[SPACE_DISTORT] = callback_offbutton_distort;
	cbs[SPACE_SATURATOR] = callback_offbutton_sat;
	cbs[SPACE_DELAY] = callback_offbutton_delay;
	cbs[SPACE_MDELAY] = callback_offbutton_mdelay;
	cbs[SPACE_BANDREJECT] = callback_offbutton_bandreject_final;
	cbs[SPACE_BANDPASS] = callback_offbutton_bandpass_final;
	cbs[SPACE_MOOGLADDER] = callback_offbutton_moogladder;
	cbs[SPACE_HPLP] = callback_offbutton_filter;
	cbs[SPACE_EQ] = callback_offbutton_eq;
	cbs[SPACE_COMPRESSION] = callback_offbutton_compressor;
	cbs[SPACE_VOCODER] = callback_offbutton_vocoder;
	cbs[SPACE_CONVOLVER] = callback_offbutton_convolver;
	cbs[SPACE_CHORUS] = callback_offbutton_chorus;
	cbs[SPACE_REVERB1] = callback_offbutton_reverb1;
	cbs[SPACE_REVERB2] = callback_offbutton_reverb2;
	cbs[SPACE_REVERB3] = callback_offbutton_reverb3;
	cbs[SPACE_REVERB4] = callback_offbutton_reverb4;
	cbs[SPACE_STC] = callback_offbutton_stc;
	cbs[SPACE_DCS] = callback_offbutton_dcs;
	cbs[SPACE_GRAINREVERB] = callback_offbutton_grainreverb;
	cbs[SPACE_LPCVOCODER] = callback_offbutton_lpcvocoder;
	cbs[SPACE_SPECTRAL_FILTER] = callback_offbutton_spectral_filter;
	cbs[SPACE_PVAMPS] = callback_offbutton_pvamps;
	cbs[SPACE_PITCHMAP] = callback_offbutton_pitchmap;
#if GS_ENABLE_PITCHMAP2
	cbs[SPACE_PITCHMAP2] = callback_offbutton_pitchmap2;
#else
	cbs[SPACE_PITCHMAP2] = nullptr;
#endif
	cbs[SPACE_GRAINMODAL] = callback_offbutton_grainmodal;
	cbs[SPACE_REVERB5] = callback_offbutton_reverb5;
	cbs[SPACE_REVERB6] = callback_offbutton_reverb6;
	cbs[SPACE_GRAINPART] = callback_offbutton_grainpart;
	cbs[SPACE_EQ5] = callback_offbutton_eq5;
	cbs[SPACE_SPECDEL] = callback_offbutton_specdel;
	cbs[SPACE_BITCRUSHER] = callback_offbutton_bc;
	cbs[SPACE_GRAINFILTER] = callback_offbutton_grainfilter;
	cbs[SPACE_GRAINVCO] = callback_offbutton_grainvco;
	cbs[SPACE_GRAINVCO2] = nullptr;
	cbs[SPACE_SPECDEL2] = callback_offbutton_specdel2;
	cbs[SPACE_SPECTRUM] = callback_offbutton_spectrum;
	cbs[SPACE_BOWED] = callback_offbutton_bowed;
	cbs[SPACE_PDETECT] = callback_offbutton_pdetect;
	cbs[SPACE_FM] = callback_offbutton_fm;
	cbs[SPACE_PDETECTGRAIN] = callback_offbutton_pdetectgrain;
	cbs[SPACE_LOOPER] = callback_offbutton_looper;
	cbs[SPACE_GRAIN_SEQUENCER] = callback_offbutton_grainseq;
	cbs[SPACE_LIMITER] = callback_offbutton_limiter;
	cbs[SPACE_MODALREV] = callback_offbutton_modalrev;
	cbs[SPACE_MODAL] = callback_offbutton_modal;
	cbs[SPACE_MULTICOMP] = callback_offbutton_mcomp;
	if constexpr (VOXFREQ < NUM_PARAMS) {
		cbs[SPACE_VOX] = callback_offbutton_vox;
	}
	else {
		(void) callback_offbutton_vox; // parked; silences unused warning
	}
	cbs[SPACE_VOX2] = callback_offbutton_vox2;
	cbs[SPACE_GRAINWAVESET] = callback_offbutton_grainwaveset;
	cbs[SPACE_GRAINDRIVE] = callback_offbutton_graindrive;
	cbs[SPACE_GRAINDISPERSE] = callback_offbutton_graindisperse;
	cbs[SPACE_GRAINVOWEL] = callback_offbutton_grainvowel;
	cbs[SPACE_GRAINPLUCK] = callback_offbutton_grainpluck;
	cbs[SPACE_DYNEQ5] = callback_offbutton_dyneq5;
	cbs[SPACE_CLIPPER] = callback_offbutton_clip;
	cbs[SPACE_PINGPONG] = callback_offbutton_pingpong;
}
