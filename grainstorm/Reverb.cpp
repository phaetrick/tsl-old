//
// Created by pr on 15.08.20.
//

#include "Reverb.h"
#include "track.h"
#include "defines.h"
#include "tools.h"
#include "app.h"


/*
 * Rlinctl (X_window   *parent,
             X_callback *cbobj,
             RotaryImg  *image,
             int32_t        xp,
             int32_t        yp,
             int32_t        cm,
             int32_t        dd,
             MYFLOAT     vmin,
             MYFLOAT     vmax,
             MYFLOAT     vini,
             int32_t        cbind = 0);

 *
_rotary [R_DELAY] = new Rlinctl (this, this, &r_delay_img, x, 0, 160, 5,  0.02,  0.100,  0.04, R_DELAY);
_rotary [R_XOVER] = new Rlogctl (this, this, &r_xover_img, x, 0, 200, 5,  50.0, 1000.0, 200.0, R_XOVER);
_rotary [R_RTLOW] = new Rlogctl (this, this, &r_rtlow_img, x, 0, 200, 5,   1.0,    8.0,   3.0, R_RTLOW);
_rotary [R_RTMID] = new Rlogctl (this, this, &r_rtmid_img, x, 0, 200, 5,   1.0,    8.0,   2.0, R_RTMID);
_rotary [R_FDAMP] = new Rlogctl (this, this, &r_fdamp_img, x, 0, 200, 5, 1.5e3, 24.0e3, 6.0e3, R_FDAMP);
x += 315;
_rotary [R_EQ1FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5,  40.0,  2.5e3, 160.0, R_EQ1FR);
_rotary [R_EQ1GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ1GN);
x += 110;
_rotary [R_EQ2FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5, 160.0,   10e3, 2.5e3, R_EQ2FR);
_rotary [R_EQ2GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ2GN);
x += 110;
_rotary [R_OPMIX] = new Rlinctl (this, this, &r_opmix_img, x, 0, 180, 5,   0.0 ,   1.0,   0.5, R_OPMIX);
_rotary [R_RGXYZ] = new Rlinctl (this, this, &r_rgxyz_img, x, 0, 180, 5,  -9.0 ,   9.0,   0.0, R_RGXYZ);
*/
Pareq::Pareq() :
        _touch0(0),
        _touch1(0),
        _state(BYPASS),
        _g0(1),
        _g1(1),
        _f0(1e3),
        _f1(1e3) {
    setfsamp(0.0);
}


void Pareq::setfsamp(MYFLOAT fsamp) {
    _fsamp = fsamp;
    reset();
}


void Pareq::reset() {
    memset(_z1, 0, sizeof(MYFLOAT) * MAXCH);
    memset(_z2, 0, sizeof(MYFLOAT) * MAXCH);
}


void Pareq::prepare(int32_t nsamp) {
    bool upd = false;
    MYFLOAT g, f;

    if (_touch1 != _touch0) {
        g = _g0;
        f = _f0;
        if (g != _g1) {
            upd = true;
            if (g > 2 * _g1) _g1 *= 2;
            else if (_g1 > 2 * g) _g1 /= 2;
            else _g1 = g;
        }
        if (f != _f1) {
            upd = true;
            if (f > 2 * _f1) _f1 *= 2;
            else if (_f1 > 2 * f) _f1 /= 2;
            else _f1 = f;
        }
        if (upd) {
            if ((_state == BYPASS) && (_g1 == 1)) {
                calcpar1(0, _g1, _f1);
            } else {
                _state = SMOOTH;
                calcpar1(nsamp, _g1, _f1);
            }
        } else {
            _touch1 = _touch0;
            if (fabs(_g1 - 1) < 0.001) {
                _state = BYPASS;
                reset();
            } else {
                _state = STATIC;
            }
        }
    }
}


void Pareq::calcpar1(int32_t nsamp, MYFLOAT g, MYFLOAT f) {
    MYFLOAT b, c1, c2, gg;

    f *= MYFLOAT(PI_P) / _fsamp;
    b = 2 * f / sqrt(g);
    gg = 0.5f * (g - 1);
    c1 = -cos(2 * f);
    c2 = (1 - b) / (1 + b);
    if (nsamp) {
        _dc1 = (c1 - _c1) / nsamp + 1e-30;
        _dc2 = (c2 - _c2) / nsamp + 1e-30;
        _dgg = (gg - _gg) / nsamp + 1e-30;
    } else {
        _c1 = c1;
        _c2 = c2;
        _gg = gg;
    }
}


void Pareq::process1(int32_t nsamp, int nchan, MYFLOAT *data[]) {
    int32_t i, j;
    MYFLOAT c1, c2, gg;
    MYFLOAT x, y, z1, z2;
    MYFLOAT *p;

    c1 = _c1;
    c2 = _c2;
    gg = _gg;
    if (_state == SMOOTH) {
        for (i = 0; i < nchan; i++) {
            p = data[i];
            z1 = _z1[i];
            z2 = _z2[i];
            c1 = _c1;
            c2 = _c2;
            gg = _gg;
            for (j = 0; j < nsamp; j++) {
                c1 += _dc1;
                c2 += _dc2;
                gg += _dgg;
                x = *p;
                y = x - c2 * z2;
                *p++ = x - gg * (z2 + c2 * y - x);
                y -= c1 * z1;
                z2 = z1 + c1 * y;
                z1 = y + 1e-20;
            }
            _z1[i] = z1;
            _z2[i] = z2;
        }
        _c1 = c1;
        _c2 = c2;
        _gg = gg;
    } else {
        for (i = 0; i < nchan; i++) {
            p = data[i];
            z1 = _z1[i];
            z2 = _z2[i];
            for (j = 0; j < nsamp; j++) {
                x = *p;
                y = x - c2 * z2;
                *p++ = x - gg * (z2 + c2 * y - x);
                y -= c1 * z1;
                z2 = z1 + c1 * y;
                z1 = y + 1e-20;
            }
            _z1[i] = z1;
            _z2[i] = z2;
        }
    }
}
// -----------------------------------------------------------------------


#define MAX_PITCHMOD    20.0
#define DELAYPOS_SHIFT  28
#define DELAYPOS_SCALE  0x10000000
#define DELAYPOS_MASK   0x0FFFFFFF


static const MYFLOAT jpScale = 0.25;


static const MYFLOAT reverbParams2[8][4] = {
        {2473.0, 0.0010, 3.100, 1966.0},
        {2767.0, 0.0011, 3.500, 29491.0},
        {3217.0, 0.0017, 1.110, 22937.0},
        {3557.0, 0.0006, 3.973, 9830.0},
        {3907.0, 0.0010, 2.341, 20643.0},
        {4127.0, 0.0011, 1.897, 22937.0},
        {2143.0, 0.0017, 0.891, 29491.0},
        {1933.0, 0.0006, 3.221, 14417.0}
};

REVERB5::REVERB5(TRACK *track) : Effect(track, SPACE_REVERB5,
                                        STEREOEFFECT)/*, str{float(_STATE->sr)} */{
    int32_t i;
    int32_t nBytes;
    _bypass = &track->bypass[SPACE_REVERB5];
    kFeedBack = &_STATE->params[track->index][REVERB5FB];
    kLPFreq = &_STATE->params[track->index][REVERB5DAMP];
    _mix = &_STATE->params[track->index][REVERB5MIX];
    _gain = &_STATE->params[track->index][REVERB5GAIN];
    _smooth2 = dbToLinear60(*_gain);
    for (i = 0; i < 8; i++) {
        _tdelay[i] = reverbParams2[i][0] / 32768.;
        _averagedelay += _tdelay[i];
        rndLine[i]._appState = _appState;
        rndLine[i].init(_STATE->sr, reverbParams2[i][0] * _STATE->sr / 32768., reverbParams2[i][1],
                        reverbParams2[i][2], 0);
    }
    _averagedelay *= .125;

    _dampFact = 1.f;
    prv_LPFreq = 0.0f;

    _damp = &_STATE->params[track->index][REV5DAMP2];
    _xover = &_STATE->params[track->index][REV5XOVER];
    _t60low = &_STATE->params[track->index][REV5T60LOW];
    _t60mid = &_STATE->params[track->index][REV5T60HI];

    _predelay = &_STATE->params[track->index][REV5PREDELAY];
    _predelayprev = *_predelay;
    _predelayL.init(_STATE->sr * 1.05, _predelayprev * _STATE->sr * 0.001);
    _predelayR.init(_STATE->sr * 1.05, _predelayprev * _STATE->sr * 0.001);
/*
MYFLOAT r[tsl::app::bufsize_init]{}, l[tsl::app::bufsize_init]{};
    MEASUSEINIT

    for (int32_t i = 0; i < 100; i++)
        compute(l, r, l, r);
    MEASURESTOP
    MEASURESTART
    for (int32_t i = 0; i < 100; i++)
        compute2(l, r, l, r);
    MEASURESTOP
*/
}
// MYFLOAT must be defined before including this header.

// ---------------------------------------------------------------------------
// Single complex resonator — one sinusoidal mode
// Implements a 2D rotation with per-sample decay:
//   [cos, sin] → R * decay * [cos, sin] + [input, 0]
// where R is the 2x2 rotation matrix for frequency w
// Output is the real part (cos component)
// ---------------------------------------------------------------------------
struct ModalResonator {
    MYFLOAT rc = static_cast<MYFLOAT>(0); // real (cos) state
    MYFLOAT rs = static_cast<MYFLOAT>(0); // imag (sin) state
    MYFLOAT c = static_cast<MYFLOAT>(1); // cos(w)
    MYFLOAT s = static_cast<MYFLOAT>(0); // sin(w)
    MYFLOAT gain = static_cast<MYFLOAT>(0); // input gain (normalisation)
    MYFLOAT d = static_cast<MYFLOAT>(1); // per-sample decay

    void init(MYFLOAT freqHz, MYFLOAT rt60, MYFLOAT sampleRate) {
        MYFLOAT w = static_cast<MYFLOAT>(2.0 * PI_P) * freqHz / sampleRate;
        c = std::cos(w);
        s = std::sin(w);
        d = std::pow(static_cast<MYFLOAT>(10),
                     static_cast<MYFLOAT>(-3)
                     / (rt60 * sampleRate));
        gain = static_cast<MYFLOAT>(1); // fixed — output normalised at tick level
        rc = static_cast<MYFLOAT>(0);
        rs = static_cast<MYFLOAT>(0);
    }

    void setRT60(MYFLOAT rt60, MYFLOAT sampleRate) {
        d = std::pow(static_cast<MYFLOAT>(10),
                     static_cast<MYFLOAT>(-3)
                     / (rt60 * sampleRate));
        // gain unchanged
    }

    inline MYFLOAT tick(MYFLOAT input) {
        MYFLOAT newRc = d * (c * rc - s * rs) + gain * input;
        MYFLOAT newRs = d * (s * rc + c * rs);
        rc = newRc;
        rs = newRs;
        return rc;
    }

    void clear() {
        rc = rs = static_cast<MYFLOAT>(0);
    }
};

// ---------------------------------------------------------------------------
// Modal reverb — kModes complex resonators, log-spaced phi-distributed
// frequencies from minHz to maxHz.
//
// No delay lines, no matrix, no comb filtering.
// Each mode decays independently at exactly RT60.
// Stereo output: even modes → L, odd modes → R
// ---------------------------------------------------------------------------
template<int kModes = 1024>
class ModalReverb {

public:
    ModalReverb() = default;

    void init(MYFLOAT rt60,
              MYFLOAT sampleRate,
              MYFLOAT minHz = static_cast<MYFLOAT>(30),
              MYFLOAT maxHz = static_cast<MYFLOAT>(18000)) {
        sampleRate_ = sampleRate;
        rt60_ = rt60;

        constexpr MYFLOAT kPhi = static_cast<MYFLOAT>(1.6180339887498948);

        for (int i = 0; i < kModes; i++) {
            // Phi low-discrepancy sequence — no two modes at near-rational
            // frequency ratios, maximally spread across log spectrum
            MYFLOAT t = std::fmod(static_cast<MYFLOAT>(i + 1) * kPhi,
                                  static_cast<MYFLOAT>(1));

            // Log spacing: denser at low frequencies where ear is sensitive
            MYFLOAT freq = minHz
                           * std::pow(maxHz / minHz, t);

            modes_[i].init(freq, rt60, sampleRate);
            freqs_[i] = freq;
        }
    }

    void setRT60(MYFLOAT rt60) {
        rt60_ = rt60;
        for (int i = 0; i < kModes; i++)
            modes_[i].setRT60(rt60, sampleRate_);
    }

    void clear() {
        for (int i = 0; i < kModes; i++)
            modes_[i].clear();
    }

    inline void tick(MYFLOAT input, MYFLOAT &outL, MYFLOAT &outR) {
        outL = outR = static_cast<MYFLOAT>(0);

        for (int i = 0; i < kModes; i++) {
            MYFLOAT out = modes_[i].tick(input);
            if (i & 1) outL += out;
            else outR += out;
        }

        // Normalise by number of modes per channel
        const MYFLOAT norm = static_cast<MYFLOAT>(2)
                             / static_cast<MYFLOAT>(kModes);
        outL *= norm;
        outR *= norm;
    }

private:
    ModalResonator modes_[kModes];
    MYFLOAT freqs_[kModes] = {};
    MYFLOAT sampleRate_ = static_cast<MYFLOAT>(48000);
    MYFLOAT rt60_ = static_cast<MYFLOAT>(2);
};

#include <random>
#include "fdn.h"


// param_N8_d1 N=8 ortho_err=5.96e-07 sv_min=1.000000 sv_max=1.000000
// delays=[809, 877, 937, 1049, 1151, 1249, 1373, 1499] gain_per_sample=0.9999
// Gamma (per-delay gains): [0.92228216 0.91603166 0.9105516  0.9004098  0.8912719  0.8825796
// 0.8717027  0.8607876 ]
static constexpr float kA_param_N8_d1[8][8] = {
        {0.71321416f,  -0.18355420f, -0.29587966f, 0.29766893f,  0.13886628f,  0.30391252f,  -0.08329856f, -0.40360451f},
        {0.13922007f,  0.49081266f,  -0.23325412f, -0.00280910f, -0.13437298f, -0.06098419f, -0.77231228f, 0.25896817f},
        {0.53830302f,  0.13519223f,  0.67122179f,  -0.01928226f, -0.24912332f, 0.13990235f,  0.13819888f,  0.37457737f},
        {-0.02543332f, 0.72611856f,  -0.06971505f, 0.49061272f,  0.10513887f,  -0.13248914f, 0.42990342f,  -0.11454089f},
        {0.19577657f,  0.36107230f,  0.02157569f,  -0.75455737f, 0.43713823f,  0.02242890f,  0.10127822f,  -0.24418427f},
        {-0.35351500f, 0.11326367f,  0.48884499f,  0.19795606f,  0.25634393f,  0.55610764f,  -0.33350819f, -0.31280506f},
        {-0.12721515f, 0.12403762f,  -0.40139538f, -0.15671630f, -0.21090800f, 0.74633336f,  0.26672640f,  0.33183467f},
        {0.04518125f,  -0.14594220f, -0.04577706f, 0.19289729f,  0.76698059f,  0.00447632f,  -0.00717957f, 0.59078228f},
};
static constexpr float kB_param_N8_d1[8] = {
        -0.49115184f, 0.20544095f, 0.69220364f, -0.03672509f, -0.11027370f, -0.30709079f,
        0.37546685f, 0.12325484f
};
static constexpr float kC_param_N8_d1[8] = {
        0.50977111f, -0.52534205f, -0.30620790f, -0.55188727f, -0.82068568f, -0.41111624f,
        0.46313474f, 0.85422212f
};
static constexpr float kGamma_param_N8_d1[8] = {
        0.92228216f, 0.91603166f, 0.91055161f, 0.90040982f, 0.89127189f, 0.88257962f, 0.87170267f,
        0.86078757f
};
static constexpr int kD_param_N8_d1[8] = {
        809, 877, 937, 1049, 1151, 1249, 1373, 1499
};

// param_N8_d2 N=8 ortho_err=4.77e-07 sv_min=1.000000 sv_max=1.000000
// delays=[241, 263, 281, 293, 1193, 1319, 1453, 1597] gain_per_sample=0.9999
// Gamma (per-delay gains): [0.97618693 0.9740416  0.97228974 0.97112364 0.8875362  0.8764229
// 0.8647565  0.8523927 ]
static constexpr float kA_param_N8_d2[8][8] = {
        {0.69380569f,  0.18810682f,  0.44061077f,  -0.28362530f, 0.11169088f,  0.10091075f,  -0.41919655f, -0.10141596f},
        {-0.01352495f, 0.63229299f,  -0.12748682f, 0.57576001f,  -0.36750630f, 0.06256318f,  -0.31930363f, -0.10649252f},
        {-0.15671343f, -0.30272377f, 0.65486014f,  0.18462811f,  -0.51182204f, 0.31619072f,  0.05047743f,  0.23745298f},
        {0.46865991f,  -0.23248005f, 0.08952004f,  0.68365872f,  0.30020297f,  -0.08681664f, 0.38232404f,  -0.08412412f},
        {-0.30005932f, 0.02790034f,  0.10334963f,  0.22751886f,  0.64413583f,  0.31275046f,  -0.38646448f, 0.42972252f},
        {0.15141878f,  -0.50230700f, -0.45674333f, 0.06807724f,  -0.13652712f, 0.54644525f,  -0.33704668f, -0.28402257f},
        {0.16794366f,  0.39112085f,  -0.13850033f, -0.17372441f, 0.02624261f,  0.65991485f,  0.54572177f,  0.18833025f},
        {0.36480340f,  -0.11430933f, -0.33804733f, 0.00064415f,  -0.25717980f, -0.21560794f, -0.11327991f, 0.78365505f},
};
static constexpr float kB_param_N8_d2[8] = {
        -0.69748926f, -0.48592743f, 0.25427216f, 0.48707733f, -0.03345867f, -0.97894919f,
        0.02699689f, -0.59174395f
};
static constexpr float kC_param_N8_d2[8] = {
        0.22230783f, -0.22674535f, -0.19677198f, -0.19029875f, -0.22343133f, -0.24900776f,
        -0.24183618f, 0.28189889f
};
static constexpr float kGamma_param_N8_d2[8] = {
        0.97618693f, 0.97404158f, 0.97228974f, 0.97112364f, 0.88753623f, 0.87642288f, 0.86475652f,
        0.85239267f
};
static constexpr int kD_param_N8_d2[8] = {
        241, 263, 281, 293, 1193, 1319, 1453, 1597
};

// param_N16_d1 N=16 ortho_err=8.34e-07 sv_min=1.000000 sv_max=1.000001
// delays=[241, 271, 293, 331, 359, 401, 443, 487, 523, 587, 647, 709, 787, 863, 953, 1049] gain_per_sample=0.9999
// Gamma (per-delay gains): [0.97618693 0.9732626  0.97112364 0.9674402  0.96473503 0.96069145
//0.9566648  0.9524645  0.9490416  0.94298685 0.93734556 0.93155175
//0.92431355 0.917315   0.9090958  0.9004098 ]
static constexpr float kA_param_N16_d1[16][16] = {
        {0.49240553f,  -0.05814151f, 0.07139155f,  0.48486876f,  -0.14084785f, -0.00157393f, 0.03810756f,  0.28471839f,  0.16746597f,  0.15387729f,  -0.30923155f, -0.19997329f, 0.34902728f,  -0.12152348f, -0.14587879f, 0.25768054f},
        {0.33761799f,  0.47863588f,  -0.40861189f, -0.16351074f, 0.01514725f,  -0.03946481f, -0.08902765f, -0.19965339f, -0.35837603f, 0.19521028f,  0.03034678f,  0.08249214f,  -0.07798895f, -0.43165952f, -0.06834672f, 0.20566571f},
        {0.08700861f,  0.10094267f,  0.41577867f,  0.00176535f,  0.11605933f,  0.03495902f,  -0.38687244f, 0.07026091f,  -0.24747124f, -0.19632426f, 0.04708787f,  0.21462145f,  0.00356165f,  0.21264191f,  0.34445316f,  0.57282138f},
        {-0.15180932f, 0.52809358f,  0.22490108f,  0.40752679f,  0.12516828f,  0.07318355f,  0.40102711f,  0.20528832f,  0.08856819f,  0.23497619f,  0.18784603f,  0.04827609f,  -0.27262002f, -0.05464809f, 0.27174202f,  -0.07481817f},
        {-0.06885087f, -0.28864583f, -0.05846545f, 0.10063509f,  0.71011609f,  -0.09163343f, -0.21212517f, -0.01858478f, 0.27170110f,  0.30935848f,  0.02819002f,  0.28529823f,  0.04305230f,  -0.29184562f, 0.01345396f,  0.03835717f},
        {-0.24147889f, -0.08895108f, -0.21029846f, 0.14828090f,  -0.18235582f, 0.57594842f,  -0.11701451f, 0.10071266f,  -0.01676284f, -0.06540185f, -0.51929897f, 0.30933630f,  -0.26651761f, -0.17233841f, 0.08624253f,  0.02585901f},
        {-0.26163232f, -0.11463210f, -0.20724532f, -0.21173233f, 0.25830090f,  0.36134347f,  0.29086939f,  0.38737112f,  -0.22065082f, 0.01965212f,  0.19296767f,  -0.35157630f, 0.18167254f,  -0.01512329f, -0.10462526f, 0.38023961f},
        {-0.17816053f, 0.17950405f,  -0.27664647f, -0.14056981f, -0.20431942f, -0.22396798f, -0.02274480f, 0.37360263f,  0.19990025f,  -0.16701555f, 0.06728437f,  0.33111981f,  0.52677852f,  -0.13906276f, 0.35151276f,  -0.07295637f},
        {0.02177910f,  -0.07411528f, 0.11483192f,  -0.44842130f, -0.22215660f, 0.02875231f,  0.27561718f,  -0.25193229f, 0.42036521f,  0.41340032f,  -0.16406414f, -0.02377630f, -0.04527366f, 0.00108355f,  0.28887397f,  0.35994747f},
        {-0.48435923f, 0.10696876f,  0.01741674f,  0.17048469f,  -0.35266671f, -0.05727609f, -0.45253760f, -0.01293144f, 0.03634386f,  0.45864955f,  0.21300080f,  -0.06955203f, 0.07218019f,  0.04384437f,  -0.31011459f, 0.16365586f},
        {0.05162267f,  -0.27167189f, -0.13947225f, 0.32749367f,  -0.20008831f, 0.12265784f,  0.34463295f,  -0.34293056f, -0.01070119f, -0.13495083f, 0.45504481f,  0.43591321f,  0.09719513f,  0.00398481f,  -0.11091620f, 0.26114064f},
        {0.28409958f,  -0.09612538f, 0.04143520f,  -0.27014813f, -0.09527320f, -0.04575755f, 0.06313861f,  0.52930015f,  -0.05919452f, 0.27291599f,  0.09682075f,  0.45598063f,  -0.26808804f, 0.25571889f,  -0.32100010f, -0.06671516f},
        {-0.23663266f, 0.14932215f,  -0.09972084f, 0.08544962f,  0.20089482f,  -0.25735292f, 0.28489438f,  -0.18639855f, -0.29328746f, 0.15867926f,  -0.48562339f, 0.21985932f,  0.26076373f,  0.43750140f,  -0.14422926f, 0.07994737f},
        {0.17375676f,  -0.02737427f, -0.61236322f, 0.19291884f,  0.05724367f,  -0.00570090f, -0.18324707f, 0.02102032f,  0.16583876f,  0.09720025f,  0.11001877f,  -0.17689295f, -0.20955063f, 0.53449541f,  0.32670063f,  0.05335372f},
        {0.04555376f,  -0.39514488f, 0.07102904f,  0.08160262f,  -0.15355170f, -0.00868735f, 0.01988906f,  0.01715280f,  -0.55513006f, 0.42121688f,  0.05282044f,  -0.04481774f, 0.12714249f,  -0.09721110f, 0.46398729f,  -0.27285004f},
        {-0.20729654f, -0.25039431f, -0.11279441f, 0.11757551f,  -0.12148245f, -0.61922002f, 0.13290773f,  0.18706471f,  -0.09138421f, -0.17375976f, -0.11946349f, -0.09748695f, -0.44466352f, -0.24976668f, 0.04059415f,  0.30472854f},
};
static constexpr float kB_param_N16_d1[16] = {
        -0.04265509f, 0.04639428f, 0.01173983f, -0.17008616f, -0.36652946f, -0.42233178f,
        0.07938836f, -0.32110137f, -0.13806878f, 0.26221618f, 0.10827556f, 0.40287837f, 0.48321772f,
        0.03231901f, -0.17075874f, -0.48788187f
};
static constexpr float kC_param_N16_d1[16] = {
        -0.35754663f, -0.36568585f, -0.38311896f, 0.35409889f, -0.36101031f, 0.24329551f,
        0.32991460f, 0.32570747f, -0.34248915f, 0.28304502f, -0.30029738f, 0.42782968f,
        -0.31943744f, 0.47297019f, -0.46666542f, 0.26844901f
};
static constexpr float kGamma_param_N16_d1[16] = {
        0.97618693f, 0.97326261f, 0.97112364f, 0.96744019f, 0.96473503f, 0.96069145f, 0.95666480f,
        0.95246452f, 0.94904160f, 0.94298685f, 0.93734556f, 0.93155175f, 0.92431355f, 0.91731501f,
        0.90909582f, 0.90040982f
};
static constexpr int kD_param_N16_d1[16] = {
        241, 271, 293, 331, 359, 401, 443, 487, 523, 587, 647, 709, 787, 863, 953, 1049
};

// param_N16_d2 N=16 ortho_err=8.34e-07 sv_min=0.999999 sv_max=1.000000
// delays=[241, 271, 283, 301, 310, 331, 343, 357, 361, 687, 704, 715, 737, 753, 763, 801] gain_per_sample=0.9999
// Gamma (per-delay gains): [0.97618693 0.9732626  0.9720953  0.97034705 0.9694741  0.9674402
//0.9662799  0.96492803 0.9645421  0.9336035  0.9320177  0.93099296
//0.9289469  0.9274617  0.92653465 0.92302036]
static constexpr float kA_param_N16_d2[16][16] = {
        {0.59939480f,  0.04767910f,  -0.02433031f, -0.26677057f, 0.31545109f,  0.12104963f,  0.04437609f,  -0.23923221f, 0.17781599f,  -0.01636015f, 0.10607905f,  -0.19351436f, -0.21030176f, -0.46163851f, 0.10564449f,  -0.21034631f},
        {0.56574649f,  0.33766457f,  0.16588266f,  0.29648352f,  -0.12149614f, -0.15103503f, 0.12757094f,  0.32446605f,  0.04541640f,  -0.20545749f, -0.10070598f, 0.20601618f,  0.14128259f,  0.13914925f,  0.32987612f,  0.21533057f},
        {0.16593790f,  -0.31194705f, 0.49975997f,  0.25639978f,  0.01231175f,  0.02226554f,  0.20072772f,  -0.44052359f, -0.44506186f, 0.18153322f,  -0.24305768f, 0.12500152f,  -0.06197629f, -0.04468507f, -0.06879780f, 0.09135279f},
        {0.20516497f,  -0.33196378f, -0.32505152f, 0.27413988f,  0.51814902f,  -0.27128017f, -0.20669888f, 0.09332251f,  -0.18426457f, -0.03812257f, -0.00842122f, -0.24362825f, 0.37671310f,  0.09299941f,  -0.14457265f, 0.08423860f},
        {-0.00601133f, 0.59657460f,  0.06983505f,  -0.12735949f, 0.47164831f,  -0.06886517f, 0.07098538f,  -0.17868768f, -0.12014842f, 0.12701294f,  0.01862453f,  0.07415339f,  -0.15094525f, 0.46346906f,  -0.29014778f, -0.02480944f},
        {0.00279831f,  0.22064829f,  0.19226463f,  0.04948768f,  0.07530738f,  0.60432035f,  -0.13842037f, 0.38255274f,  -0.35682485f, 0.21276303f,  0.17624895f,  -0.07506048f, 0.30804214f,  -0.23778941f, -0.11634175f, -0.03538631f},
        {-0.06794635f, -0.03434666f, -0.43176845f, 0.11094625f,  0.06311298f,  0.14817807f,  0.58860427f,  0.08354925f,  -0.41531733f, -0.36062893f, 0.17008805f,  0.15666884f,  -0.21160866f, -0.06262846f, 0.02217960f,  -0.10433602f},
        {0.10048466f,  -0.17784555f, 0.23705979f,  -0.53845483f, -0.05446045f, -0.13009775f, 0.16730517f,  0.28454047f,  -0.18068627f, -0.20118465f, -0.30383018f, -0.11541431f, 0.22193561f,  0.21544136f,  -0.06113575f, -0.45291674f},
        {-0.02976334f, -0.35574135f, 0.32958490f,  0.10531625f,  0.35089678f,  0.10862504f,  0.17886226f,  0.43752587f,  0.41008678f,  -0.02313025f, 0.11415547f,  0.19770654f,  -0.30741495f, 0.05128324f,  -0.26564559f, 0.07726109f},
        {0.12237664f,  -0.03742184f, -0.16405886f, -0.12506495f, -0.07260485f, -0.41733333f, -0.05431064f, 0.20736504f,  -0.17003536f, 0.53763324f,  0.17868683f,  0.52883768f,  -0.04429490f, -0.20672649f, -0.06224922f, -0.19376831f},
        {0.01039642f,  -0.05864715f, 0.22244158f,  -0.14202756f, 0.02435141f,  -0.08849017f, -0.54302311f, -0.07274033f, -0.27511439f, -0.55224460f, 0.36531666f,  0.27937499f,  -0.15313177f, -0.01737435f, -0.01726608f, 0.04464766f},
        {-0.16256684f, 0.04052776f,  -0.02060887f, 0.27687317f,  0.25075600f,  0.18663295f,  -0.11190542f, -0.19091642f, 0.23127322f,  -0.13283505f, -0.23449565f, 0.46147621f,  0.27843356f,  -0.04765287f, 0.18807770f,  -0.54213715f},
        {-0.03092302f, -0.04123851f, 0.21721658f,  -0.04349668f, -0.04390952f, -0.12539077f, 0.35736045f,  -0.24657118f, 0.18184842f,  -0.00374122f, 0.66169143f,  -0.00973868f, 0.51406747f,  0.07620414f,  0.00492927f,  -0.03166629f},
        {0.37447482f,  -0.29590681f, -0.26937562f, -0.11454397f, -0.19052304f, 0.46841222f,  -0.13250314f, -0.12291595f, 0.05038800f,  0.16169654f,  0.12544699f,  0.18487974f,  -0.02509781f, 0.56445855f,  0.01261800f,  -0.01204309f},
        {-0.14473900f, -0.08038794f, 0.16001977f,  0.15435170f,  0.16005126f,  -0.07256386f, -0.06933077f, 0.12712915f,  -0.15408698f, 0.22067052f,  0.24356914f,  -0.28942585f, -0.28754050f, 0.24488063f,  0.65770423f,  -0.28382227f},
        {-0.17814443f, -0.10022511f, -0.03478024f, -0.46553761f, 0.35540402f,  0.10291819f,  0.08767311f,  -0.01623560f, -0.04219937f, 0.06422183f,  -0.14318135f, 0.26138055f,  0.18257767f,  -0.05358776f, 0.45778164f,  0.50490993f},
};
static constexpr float kB_param_N16_d2[16] = {
        0.57508045f, -0.45278421f, -0.21000040f, -0.15177499f, 0.14530692f, -0.55031139f,
        -0.03825020f, -0.31553140f, 0.23698558f, 0.19338688f, 0.28639439f, -0.35465932f,
        -0.14831272f, 0.24608371f, 0.55345142f, -0.37902781f
};
static constexpr float kC_param_N16_d2[16] = {
        -0.20524341f, 0.24000195f, 0.28009322f, 0.28225383f, 0.24192826f, -0.22927739f, 0.27597547f,
        0.28329614f, -0.28558716f, 0.29026622f, 0.34714779f, -0.30736297f, -0.39909557f,
        -0.27516395f, 0.22585106f, -0.25234503f
};
static constexpr float kGamma_param_N16_d2[16] = {
        0.97618693f, 0.97326261f, 0.97209531f, 0.97034705f, 0.96947408f, 0.96744019f, 0.96627992f,
        0.96492803f, 0.96454209f, 0.93360353f, 0.93201768f, 0.93099296f, 0.92894691f, 0.92746168f,
        0.92653465f, 0.92302036f
};
static constexpr int kD_param_N16_d2[16] = {
        241, 271, 283, 301, 310, 331, 343, 357, 361, 687, 704, 715, 737, 753, 763, 801
};


// MYFLOAT must be defined before including this header.

// ---------------------------------------------------------------------------
// SpectralReverb
//
// Bank of complex resonators extract per-frequency energy from input.
// Envelope followers on each resonator drive sinusoidal oscillators
// with RT60 decay. No FFT, no hop boundaries, no discontinuities.
//
// Frequencies: phi-spaced from minFreq to maxFreq (inharmonic, no comb)
// Bandwidth:   touching filters — each filter covers its share of spectrum
// Oscillators: run at resonator center frequencies, cosine table lookup
//
// Usage:
//   SpectralReverb verb(48000.0f, 2.0f);
//   float outL, outR;
//   verb.tick(input, outL, outR);
// ---------------------------------------------------------------------------

class FilterOscBank {
public:
    static constexpr int     kNumOsc  = 64;
    static constexpr MYFLOAT kMinFreq = static_cast<MYFLOAT>(80);
    static constexpr MYFLOAT kMaxFreq = static_cast<MYFLOAT>(6000);
    static constexpr int     kCosSize = 4096;
    static constexpr int     kCosMask = kCosSize - 1;

    FilterOscBank(MYFLOAT sr, MYFLOAT rt60)
            : sr_(sr)
    {
        // Cosine table
        for (int i = 0; i < kCosSize; i++)
            cosTable_[i] = static_cast<MYFLOAT>(
                    std::cos(2.0 * PI_P * i / kCosSize));

        initResonators();
        setRT60(rt60);
        clear();
    }

    void setRT60(MYFLOAT rt60) {
        rt60_ = rt60;
        // Per-sample decay
        sampleDecay_ = std::pow(static_cast<MYFLOAT>(10),
                                static_cast<MYFLOAT>(-3)
                                / (rt60 * sr_));
    }

    void clear() {
        std::memset(resRe_,      0, sizeof(resRe_));
        std::memset(resIm_,      0, sizeof(resIm_));
        std::memset(envMag_,     0, sizeof(envMag_));
        std::memset(phaseAccum_, 0, sizeof(phaseAccum_));
    }

    // ------------------------------------------------------------------
    // tick — stereo out
    // ------------------------------------------------------------------
    inline void tick(MYFLOAT input, MYFLOAT& outL, MYFLOAT& outR)
    {
        outL = outR = static_cast<MYFLOAT>(0);

        for (int k = 0; k < kNumOsc; k++)
        {
            const MYFLOAT newRe = resCr_[k] * resRe_[k]
                                  - resCi_[k] * resIm_[k]
                                  + input * inputGain_[k];
            const MYFLOAT newIm = resCi_[k] * resRe_[k]
                                  + resCr_[k] * resIm_[k];
            resRe_[k] = newRe;
            resIm_[k] = newIm;

            // 2. Magnitude of resonator output
            const MYFLOAT mag = std::sqrt(newRe * newRe + newIm * newIm);

            // 3. Envelope follower — attack fast, release = RT60
            if (mag > envMag_[k])
                envMag_[k] = attCoeff_  * envMag_[k]
                             + (static_cast<MYFLOAT>(1) - attCoeff_) * mag;
            else
                envMag_[k] *= sampleDecay_;

            // 4. Advance oscillator phase
            phaseAccum_[k] += phaseInc_[k];
            if (phaseAccum_[k] >= static_cast<MYFLOAT>(kCosSize))
                phaseAccum_[k] -= static_cast<MYFLOAT>(kCosSize);

            // 5. Cosine table lookup
            const int     idx = static_cast<int>(phaseAccum_[k]) & kCosMask;
            const MYFLOAT out = envMag_[k] * cosTable_[idx];

            if (k & 1) outR += out;
            else       outL += out;
        }

        // Normalise — half oscillators per channel
        const MYFLOAT norm = static_cast<MYFLOAT>(1)
                             / static_cast<MYFLOAT>(kNumOsc / 2);
        outL *= norm;
        outR *= norm;
    }

    inline MYFLOAT tickMono(MYFLOAT input) {
        MYFLOAT outL, outR;
        tick(input, outL, outR);
        return (outL + outR) * static_cast<MYFLOAT>(0.5);
    }

private:
    void initResonators()
    {
        constexpr MYFLOAT kPhi = static_cast<MYFLOAT>(1.6180339887498948);

        // Phi-spaced frequencies — inharmonic, no comb filter
        MYFLOAT freqs[kNumOsc];
        for (int k = 0; k < kNumOsc; k++) {
            MYFLOAT t   = std::fmod(static_cast<MYFLOAT>(k + 1) * kPhi,
                                    static_cast<MYFLOAT>(1));
            freqs[k]    = kMinFreq
                          * std::pow(kMaxFreq / kMinFreq, t);
        }

        // Sort frequencies ascending — needed for bandwidth computation
        std::sort(freqs, freqs + kNumOsc);

        // Init resonators and oscillators
        for (int k = 0; k < kNumOsc; k++) {
            const MYFLOAT freq = freqs[k];

            // Bandwidth = half distance to neighbors (touching filters)
            MYFLOAT bwLo = (k > 0)
                           ? (freq - freqs[k-1]) * static_cast<MYFLOAT>(0.5)
                           : freq * static_cast<MYFLOAT>(0.1);
            MYFLOAT bwHi = (k < kNumOsc - 1)
                           ? (freqs[k+1] - freq) * static_cast<MYFLOAT>(0.5)
                           : freq * static_cast<MYFLOAT>(0.1);
            MYFLOAT bw   = bwLo + bwHi; // full bandwidth

            // Resonator pole radius from bandwidth
            // r = exp(-pi * bw / sr)
            MYFLOAT r       = std::exp(-static_cast<MYFLOAT>(PI_P) * bw / sr_);
            inputGain_[k]   = static_cast<MYFLOAT>(1) - r;
            MYFLOAT w    = static_cast<MYFLOAT>(2.0 * PI_P) * freq / sr_;
            resCr_[k]    = r * std::cos(w);
            resCi_[k]    = r * std::sin(w);

            // Oscillator phase increment in cosine table units per sample
            phaseInc_[k] = freq * static_cast<MYFLOAT>(kCosSize) / sr_;
        }
    }

    MYFLOAT sr_          = static_cast<MYFLOAT>(48000);
    MYFLOAT rt60_        = static_cast<MYFLOAT>(2);
    MYFLOAT sampleDecay_ = static_cast<MYFLOAT>(1);
    MYFLOAT attCoeff_    = static_cast<MYFLOAT>(0);

    MYFLOAT cosTable_  [kCosSize] = {};
    MYFLOAT resCr_     [kNumOsc]  = {}; // r * cos(w) per resonator
    MYFLOAT resCi_     [kNumOsc]  = {}; // r * sin(w) per resonator
    MYFLOAT resRe_     [kNumOsc]  = {}; // resonator real state
    MYFLOAT resIm_     [kNumOsc]  = {}; // resonator imag state
    MYFLOAT inputGain_[kNumOsc] = {};
    MYFLOAT envMag_    [kNumOsc]  = {}; // envelope follower per oscillator
    MYFLOAT phaseAccum_[kNumOsc]  = {}; // oscillator phase accumulator
    MYFLOAT phaseInc_  [kNumOsc]  = {}; // oscillator phase increment
};

#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>

// MYFLOAT must be defined before including this header.
// Requires: FFT class with forward(MYFLOAT* in, MYFLOAT* out) method.
//           FFT.forward() must output correctly scaled magnitudes.

// ---------------------------------------------------------------------------
// SpectralReverb
//
// Architecture:
//   - Global envelope follower tracks input amplitude with RT60 decay
//   - FFT computes normalized spectral shape periodically
//   - Oscillator bank driven by envelope * shape[k] per bin
//   - Shape interpolates smoothly between FFT frames (normalized 0..1)
//   - No hop boundary artifacts — shape changes are tiny normalized steps
//
// Usage:
//   SpectralReverb verb(48000.0f, 2.0f);
//   float outL, outR;
//   verb.tick(input, outL, outR);
// ---------------------------------------------------------------------------
class SpectralReverb {
public:
    static constexpr int fftSize  = 2048;
    static constexpr int hopSize  = 512;
    static constexpr int numBins  = fftSize / 2 + 1;
    static constexpr int kNumOsc  = numBins - 2;
    static constexpr int kCosSize = 4096;
    static constexpr int kCosMask = kCosSize - 1;

    SpectralReverb(MYFLOAT sr, MYFLOAT rt60)
            : sr_(sr)
            , fft_(fftSize)
    {
        for (int i = 0; i < kCosSize; i++)
            cosTable_[i] = static_cast<MYFLOAT>(
                    std::cos(2.0 * PI_P * i / kCosSize));

        for (int i = 0; i < fftSize; i++) {
            const MYFLOAT x = 2.0 * PI_P * i / (fftSize - 1);
            window_[i] = static_cast<MYFLOAT>(
                    0.21557895
                    - 0.41663158 * std::cos(x)
                    + 0.27726316 * std::cos(2*x)
                    - 0.08357895 * std::cos(3*x)
                    + 0.00694737 * std::cos(4*x));
        }
        clear();

        constexpr double kPhi=1.6180339887498948;
        for(int k=0;k<kNumOsc;k++){
            double bf=(double)(k+1)*sr_/fftSize;
            // oscillator runs at detuned frequency
            double detune=1.0+(fmod((double)(k+1)*kPhi,1.0)-0.5)*0.001;
            phaseInc_[k]=(uint32_t)((bf*detune/sr_)*4294967296.0);
            // magnitude comes from original bin k+1
            // initial phase
            double t=fmod((double)(k+1)*kPhi,1.0);
            phaseAccum_[k]=(uint32_t)(t*4294967296.0);
        }

        setRT60(rt60);
        // In constructor, after clear():
// Set one oscillator to test amplitude — bin for ~440Hz
// bin = round(440 * fftSize / sr) = round(440 * 2048 / 48000) = round(18.77) = 19
       //magCurrent_[19] = static_cast<MYFLOAT>(1);
    }

    void setRT60(MYFLOAT rt60) {
        rt60_     = rt60;
        hopDecay_ = std::pow(static_cast<MYFLOAT>(10),
                             static_cast<MYFLOAT>(-3)
                             * static_cast<MYFLOAT>(hopSize)
                             / (rt60 * sr_));
        sampleDecay_ = std::pow(10.0, -3.0 / (rt60 * sr_));
    }

    void clear() {
        std::memset(phaseAccum_,  0, sizeof(phaseAccum_));
        std::memset(magCurrent_,  0, sizeof(magCurrent_));
        std::memset(magInc_,      0, sizeof(magInc_));
        std::memset(analysisWin_, 0, sizeof(analysisWin_));
        std::memset(inputBuf_,    0, sizeof(inputBuf_));
        std::memset(fftFrame_,    0, sizeof(fftFrame_));
        inputPos_ = 0;
    }

    inline void tick(MYFLOAT input, MYFLOAT& outL, MYFLOAT& outR)
    {
        inputBuf_[inputPos_++] = input;
        if (inputPos_ >= hopSize) {
            inputPos_ = 0;
            updateMags();
        }

        outL = outR = 0.0;

        for (int k = 0; k < kNumOsc; k++)
        {
            magCurrent_[k] += magInc_[k];
            // Integer increment (overflows naturally at 2^32)
            phaseAccum_[k] += phaseInc_[k];

            // Shift down to get the 12-bit index (4096 table size)
            // 32 - 12 = 20
            const int idx = (phaseAccum_[k] >> 20);

            const MYFLOAT out = magCurrent_[k] * cosTable_[idx];

            // Stereo split
            if (k & 1) outR += out;
            else       outL += out;
        }
    }

    inline MYFLOAT tickMono(MYFLOAT input) {
        MYFLOAT outL, outR;
        tick(input, outL, outR);
        return (outL + outR) * static_cast<MYFLOAT>(0.5);
    }

private:
    void updateMags()
    {
        // 1. Shift analysis window and copy new input
        std::memmove(analysisWin_, analysisWin_ + hopSize, (fftSize - hopSize) * sizeof(MYFLOAT));
        std::memcpy(analysisWin_ + fftSize - hopSize, inputBuf_, hopSize * sizeof(MYFLOAT));

        // 2. Window the data
        for (int i = 0; i < fftSize; i++)
            fftFrame_[i] = analysisWin_[i] * window_[i];

        // 3. Perform FFT
        fft_.forward(fftFrame_, fftFrame_);

        const MYFLOAT norm = static_cast<MYFLOAT>(1.0 / fftSize);


        for (int k = 0; k < kNumOsc; k++) {
            const int     bin = k + 1;
            const MYFLOAT re  = fftFrame_[2 * bin];
            const MYFLOAT im  = fftFrame_[2 * bin + 1];
            const MYFLOAT mag = std::sqrt(re * re + im * im) * norm;

            /*
            MYFLOAT damping    = static_cast<MYFLOAT>(k) / kNumOsc;
            MYFLOAT perBinDecay = hopDecay_ * (static_cast<MYFLOAT>(1)
                                               - damping * static_cast<MYFLOAT>(0.05));
*/
            // nextTarget from wherever magCurrent_ actually is now
            const MYFLOAT nextTarget = magCurrent_[k] * hopDecay_ + mag;

            // Ramp from current actual value to next target
            magInc_[k] = (nextTarget - magCurrent_[k])
                         / static_cast<MYFLOAT>(hopSize);

        }
    }

    MYFLOAT sr_       = static_cast<MYFLOAT>(48000);
    MYFLOAT rt60_     = static_cast<MYFLOAT>(2);
    MYFLOAT hopDecay_ = static_cast<MYFLOAT>(1);
    MYFLOAT sampleDecay_ = static_cast<MYFLOAT>(1);

    FFT fft_;

    MYFLOAT cosTable_   [kCosSize] = {};
    MYFLOAT window_     [fftSize]  = {};
    MYFLOAT analysisWin_[fftSize]  = {};
    MYFLOAT inputBuf_   [hopSize]  = {};
    MYFLOAT fftFrame_   [fftSize]  = {};
    uint32_t phaseAccum_[kNumOsc] = {};
    uint32_t phaseInc_  [kNumOsc] = {};
    MYFLOAT magCurrent_[kNumOsc] = {};
    MYFLOAT magInc_    [kNumOsc] = {};

    int inputPos_ = 0;
};

inline SpectralReverb makeSpectralReverb(
        MYFLOAT rt60       = static_cast<MYFLOAT>(2),
        MYFLOAT sampleRate = static_cast<MYFLOAT>(48000))
{
    return SpectralReverb(sampleRate, rt60);
}

class SpectralReverb2 : private tsl::CircularBuffer<MYFLOAT> {
public:
    static constexpr int fftSize  = 4096;
    static constexpr int hopSize  = 128;
    static constexpr int overlap  = fftSize / hopSize;
    static constexpr int kCosSize  = 4096;
    static constexpr MYFLOAT norm = 1.0 / overlap;
    SpectralReverb2(MYFLOAT sr, MYFLOAT rt60)
            : sr_(sr)
            , fft_(fftSize)
            , tsl::CircularBuffer<MYFLOAT>(fftSize, overlap)
    {

        for (int i = 0; i < fftSize; i++) {
            // Standard Hann Window
            MYFLOAT hann = static_cast<MYFLOAT>(0.5 * (1.0 - std::cos(2.0 * PI_P * i / (fftSize - 1))));

            // For double-windowing (analysis + synthesis), we take the square root
            // to ensure window * window = Hann, which satisfies COLA.
            window_[i] = std::sqrt(hann);
        }

        // Cosine table
        for (int i = 0; i < kCosSize; i++)
            cosTable_[i] = static_cast<MYFLOAT>(
                    std::cos(2.0 * PI_P * i / static_cast<MYFLOAT>(kCosSize)));
        clear();

        for (int k = 0; k < (fftSize / 2); k++) {
            // The frequency of bin 'k' is k * (sr / fftSize)
            // In our 32-bit fixed point (2^32), this is exactly:
            double phasePerSample = static_cast<double>(k) / static_cast<double>(fftSize);
            phaseInc_[k] = static_cast<uint32_t>(phasePerSample * 4294967296.0);

            // Important: Start with a random phase so the bins don't all
            // align at zero (which sounds like a "click" at the start)
            phaseAccum_[k] = static_cast<uint32_t>(std::rand()) % 0xFFFFFFFF;
        }

        setRT60(rt60);

    }

    void setRT60(MYFLOAT rt60) {
        rt60_     = rt60;
        hopDecay_ = std::pow(static_cast<MYFLOAT>(10),
                             static_cast<MYFLOAT>(-3)
                             * static_cast<MYFLOAT>(hopSize)
                             / (rt60 * sr_));
        sampleDecay_ = std::pow(10.0, -3.0 / (rt60 * sr_));
    }

    void clear() {
        std::memset(phaseAccum_,  0, sizeof(phaseAccum_));
        std::memset(magCurrent_,  0, sizeof(magCurrent_));
    }

    inline void tick(MYFLOAT input, MYFLOAT& outL, MYFLOAT& outR)
    {
        auto val = tsl::CircularBuffer<MYFLOAT>::_tick(input);
        outL = outR = val;
    }



private:
    void onBufferReady(MYFLOAT *buf, int s) override{
        for(int i=0;i<fftSize;i++) buf[i] *= window_[i];
        fft_.forward(buf, buf);
        // DC
        MYFLOAT e0 = buf[0]*buf[0];
        magCurrent_[0] = magCurrent_[0] * hopDecay_ + e0 * norm;
        buf[0] = std::sqrt(magCurrent_[0]);

// Nyquist
        MYFLOAT eN = buf[1]*buf[1];
        magCurrent_[fftSize/2] = magCurrent_[fftSize/2] * hopDecay_ + eN * norm;
        buf[1] = std::sqrt(magCurrent_[fftSize/2]);
        for (int k = 1; k < fftSize/2; k++) {
            // 1. Get current magnitude (accumulated reverb tail)
            MYFLOAT energy = buf[k*2]*buf[k*2] + buf[k*2+1]*buf[k*2+1];

            magCurrent_[k] = magCurrent_[k] * hopDecay_ + energy * norm;

// convert back to magnitude
            MYFLOAT magOut = std::sqrt(magCurrent_[k]);

            // 2. Lookup Cosine for Real part
            uint32_t phaseIdx = (phaseAccum_[k] >> 20); // 0 to 4095
            buf[k*2] = cosTable_[phaseIdx] * magOut;

            // 3. Lookup Sine for Imaginary part
            // Subtract 90 degrees (1024) from phase.
            // Mask with (kCosSize - 1) to handle the wrap-around.
            uint32_t sineIdx = (phaseIdx - 1024) & (kCosSize - 1);
            buf[k*2+1] = cosTable_[sineIdx] * magOut;

            // 4. Increment phase for the NEXT hop
            // Since this is OLA, we increment by (phaseInc * hopSize)
            // to jump to where the phase should be for the next block.
            phaseAccum_[k] += phaseInc_[k] * hopSize;
//            phaseAccum_[k] += (std::rand() & 0xFFFF) << 8;
        }
        fft_.backward(buf, buf);
        for(int i=0;i<fftSize;i++) buf[i] *= window_[i];
    }



    MYFLOAT sr_       = static_cast<MYFLOAT>(48000);
    MYFLOAT rt60_     = static_cast<MYFLOAT>(2);
    MYFLOAT hopDecay_ = static_cast<MYFLOAT>(1);
    MYFLOAT sampleDecay_ = static_cast<MYFLOAT>(1);

    FFT fft_;

    MYFLOAT window_     [fftSize]  = {};
    uint32_t phaseAccum_[fftSize] = {};
    uint32_t phaseInc_  [fftSize] = {};
    MYFLOAT magCurrent_[fftSize] = {};
    MYFLOAT cosTable_[kCosSize] = {};
};



class SpectralReverb3 {
public:
    static constexpr int fftSize = 2048;
    static constexpr int hopSize = 512;
    static constexpr int numBins = 256; // HUGE CPU win

    SpectralReverb3(MYFLOAT sr, MYFLOAT rt60)
            : sr_(sr), fft_(fftSize)
    {
        setRT60(rt60);

        // Hann window
        for (int i = 0; i < fftSize; i++) {
            window_[i] = 0.5 * (1.0 - std::cos(2.0 * PI_P * i / (fftSize - 1)));
        }

        // oscillator setup
        for (int k = 0; k < numBins; k++) {
            double w = 2.0 * PI_P * k / fftSize;

            rotRe_[k] = std::cos(w);
            rotIm_[k] = std::sin(w);

            oscRe_[k] = 1.0;
            oscIm_[k] = 0.0;
        }

        clear();
    }

    void setRT60(MYFLOAT rt60) {
        rt60_ = rt60;

        // correct per-sample decay (energy domain!)
        MYFLOAT decayPerSec = std::pow(10.0, -3.0 / rt60_);
        sampleDecay_ = std::pow(decayPerSec, 1.0 / sr_);
    }

    void clear() {
        std::memset(inputBuf_, 0, sizeof(inputBuf_));
        std::memset(env_, 0, sizeof(env_));
        std::memset(targetEnv_, 0, sizeof(targetEnv_));

        writePos_ = hopCounter_ = 0;
    }

    inline void tick(MYFLOAT in, MYFLOAT& outL, MYFLOAT& outR)
    {
        inputBuf_[writePos_] = in;

        // === analysis trigger ===
        hopCounter_++;
        if (hopCounter_ >= hopSize) {
            hopCounter_ = 0;
            analyzeFrame();
        }

        MYFLOAT out = 0;

        // === oscillator bank ===
        for (int k = 1; k < numBins; k++) {

            // smooth envelope toward target
            env_[k] += 0.002f * (targetEnv_[k] - env_[k]);

            // decay (correct RT60 behavior)
            env_[k] *= sampleDecay_;

            // oscillator recursion
            MYFLOAT re = oscRe_[k];
            MYFLOAT im = oscIm_[k];

            oscRe_[k] = re * rotRe_[k] - im * rotIm_[k];
            oscIm_[k] = re * rotIm_[k] + im * rotRe_[k];

            out += std::sqrt(env_[k]) * oscRe_[k];
        }

        writePos_++;
        if (writePos_ >= fftSize) writePos_ = 0;

        // normalize
        out *= (2.0 / numBins);

        outL = outR = out;
    }

private:
    void analyzeFrame()
    {
        // gather frame
        for (int i = 0; i < fftSize; i++) {
            int idx = (writePos_ + i) % fftSize;
            frame_[i] = inputBuf_[idx] * window_[i];
        }

        fft_.forward(frame_, frame_);

        for (int k = 0; k < numBins; k++) {
            MYFLOAT re = frame_[2*k];
            MYFLOAT im = frame_[2*k+1];

            MYFLOAT energy = re*re + im*im;

            // inject scaled energy (critical!)
            targetEnv_[k] += energy * (1.0f / fftSize);
        }
    }

private:
    MYFLOAT sr_ = 48000;
    MYFLOAT rt60_ = 2.0;

    MYFLOAT sampleDecay_ = 0.9999;

    FFT fft_;

    MYFLOAT window_[fftSize] = {};
    MYFLOAT inputBuf_[fftSize] = {};
    MYFLOAT frame_[fftSize] = {};

    // energy domain
    MYFLOAT env_[numBins] = {};
    MYFLOAT targetEnv_[numBins] = {};

    // oscillators
    MYFLOAT oscRe_[numBins] = {};
    MYFLOAT oscIm_[numBins] = {};
    MYFLOAT rotRe_[numBins] = {};
    MYFLOAT rotIm_[numBins] = {};

    int writePos_ = 0;
    int hopCounter_ = 0;
};
/*
static bool initdone = false;
SpectralReverb3 spectral(48000.0f, 10.0f);
*/

void REVERB5::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    MYFLOAT mix, gain = dbToLinear60(*_gain);


    MYFLOAT xt = _xt;
    MYFLOAT yt = _yt;

    /* update delay lines */

    if (_olddamp != *_damp || _oldt60low != *_t60low || _oldt60mid != *_t60mid ||
        _oldxover != *_xover)
        updateFilters();


    if (*_bypass || destroyRequested) {
        mix = 0;
    } else {
        mix = *_mix;
    }

    if (_predelayprev != *_predelay) {
        _predelayprev = *_predelay;
        _predelayL.setDelayMS(_predelayprev, _STATE->sr);
        _predelayR.setDelayMS(_predelayprev, _STATE->sr);
    }


//    spectral.setRT60(LOG2NORMAL(_oldt60low));

    for (int32_t i = 0; i < s; i++) {
  /*
        MYFLOAT inputL = inl[i], inputR = inr[i];
        MYFLOAT outputL{}, outputR{};
// In tick:
        MYFLOAT aL, aR, bL, bR;
        auto input = (inputL + inputR) * .5;
        const MYFLOAT mixsrc1 = 1. - _smooth1;

        spectral.tick(input, aL, aR);
        outl[i] = outl[i] * mixsrc1 + aL * _smooth1 * _gainfact * _smooth2;
        outr[i] = outr[i] * mixsrc1 + aR * _smooth1 * _gainfact * _smooth2;
        smmixgain(mix, gain);
        continue;
*/
        UDD(inl[i]);
        UDD(inr[i]);

        /* calculate "resultant junction pressure" and mix to input signals */

        MYFLOAT ainL = 0.0;
        for (auto &d: _filterstate)
            ainL += d;

        MYFLOAT temp = ainL;
        yt = temp - xt + .995 * yt;
        ainL = yt * jpScale;
        xt = temp;
        MYFLOAT ainR = ainL + (MYFLOAT) _predelayL.tick(inr[i]);
        ainL = ainL + (MYFLOAT) _predelayR.tick(inl[i]);
        /* loop through all delay lines */
        MYFLOAT aoutL = 0.0;
        MYFLOAT aoutR = 0.0;

        for (int32_t n = 0; n < 8; n++) {
            _filterstate[n] = _filt[n].process(rndLine[n].tick((MYFLOAT) ((n & 1u ? ainR : ainL)
                                                                          - _filterstate[n])));

            /* mix to output */
            if (n & 1u)
                aoutR += _filterstate[n];
            else
                aoutL += _filterstate[n];
        }
        const MYFLOAT mixsrc = 1. - _smooth1;
        outl[i] = outl[i] * mixsrc + aoutL * _smooth1 * _gainfact * _smooth2;
        outr[i] = outr[i] * mixsrc + aoutR * _smooth1 * _gainfact * _smooth2;
        smmixgain(mix, gain);
    }

    _xt = xt;
    _yt = yt;
}

void REVERB5::updateFilters() {
    _oldt60low = *_t60low;
    _oldt60mid = *_t60mid;
    _oldxover = *_xover;
    _olddamp = *_damp;
    const MYFLOAT crossover = LOG2NORMALF(_oldxover);
    MYFLOAT chi;
    MYFLOAT wlo = TWOPI_F_P * crossover / _STATE->sr;
    if (LOG2NORMALF(_olddamp) > 0.49 * _STATE->sr) chi = 2;
    else chi = 1 - cos(TWOPI_F_P * LOG2NORMALF(_olddamp) / _STATE->sr);

    for (int32_t i = 0; i < 8; i++) {
        _filt[i].set_params(_tdelay[i], LOG2NORMALF(_oldt60mid), LOG2NORMALF(_oldt60low), wlo,
                            0.5 * LOG2NORMALF(_oldt60mid), chi);
    }

    const MYFLOAT tot = 10.;
    MYFLOAT low = log2(crossover / 20.) / tot *
                  pow(0.001, (_averagedelay) / (LOG2NORMALF(_oldt60low) * _STATE->sr));
    MYFLOAT high = log2(20000. / crossover) / tot * pow(0.001, (_averagedelay) /
                                                               (LOG2NORMALF(
                                                                       _oldt60mid)));//pow(0.001, (3500) / (LOG2NORMAL(_oldt60mid) * _STATE->sr));
    _gainfact = 1. / ((high + low) * 4.);

    // LOGE("%f %f %f", _gainfact, low, high);
}


DaRev::DaRev(TRACK *t) : Effect(t, SPACE_REVERB4, STEREOEFFECT) {
    //_t60 = &_STATE->params[t->index][REV4T60];
    _gain = &_STATE->params[t->index][REV4GAIN];
    _mix = &_STATE->params[t->index][REV4MIX];
    reverbFdn.init(_STATE->sr);
};

SimpleReverb::SimpleReverb(TRACK *track, int32_t channel) : Effect(track, channel,
                                                                   SPACE_GRAINREVERB, GRAINEFFECT) {
    _t60 = &_STATE->params[track->index][GRAINREVERBDECAY];
    _mix = &_STATE->params[track->index][GRAINREVERBMIX];
    _gain = &_STATE->params[track->index][GRAINREVERBGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _bypass = &track->bypass[SPACE_GRAINREVERB];
    init();
    _prvt60 = *_t60;
    setT60(_prvt60 * 10);
    setDamp(1);
}

#include "grainstorm.h"

void SimpleReverb::compute(MYFLOAT *in, int32_t size) {
    auto tindex = _track->index;
    check();


    auto window = _DATA->hanningwin;

    MYFLOAT mix, gain = dbToLinear60(*_gain);
    if (*_bypass || destroyRequested) {
        mix = 0;
    } else {
        mix = *_mix;
    }
    // The grain is windowed on the way INTO the tank (the dry path stays raw),
    // so the raw grain edges cannot ring a click through the diffusers.
    // PHS2INT already scales the phase by TBLMAX (== WINDOW_SIZE) and masks it
    // to 14 bits, so sp must run 0..1 across the grain - it used to be stepped
    // by WINDOW_SIZE/size, i.e. WINDOW_SIZE times too fast, which landed on
    // index 0 (hanningwin[0] == 0, so: silence into the tank) for every
    // power-of-two grain size and scrambled the input level for the rest.
    auto sl = 1. / (MYFLOAT) size;
    MYFLOAT sp = 0.;

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT t = in[i] * window[PHS2INT(sp)];
        sp += sl;
        MYFLOAT x0 = _diff1[0].process(_delay[0].read() + t);
        MYFLOAT x1 = _diff1[1].process(_delay[1].read() + t);
        MYFLOAT x2 = _diff1[2].process(_delay[2].read() - t);
        MYFLOAT x3 = _diff1[3].process(_delay[3].read() - t);

        t = x0 - x1;
        x0 += x1;
        x1 = t;
        t = x2 - x3;
        x2 += x3;
        x3 = t;

        t = x0 - x2;
        x0 += x2;
        x2 = t;
        t = x1 - x3;
        x1 += x3;
        x3 = t;

        in[i] = in[i] * (1.f - _smooth1) + _smooth1 * _gainfact * (x1 + x2) * _smooth2;
        smmixgain(mix, gain);

        _filtstate[0] = (x0 * (1 - _c) + _c * _filtstate[0]);
        _filtstate[1] = (x1 * (1 - _c) + _c * _filtstate[1]);
        _filtstate[2] = (x2 * (1 - _c) + _c * _filtstate[2]);
        _filtstate[3] = (x3 * (1 - _c) + _c * _filtstate[3]);

        _delay[0].write(_feedback[0] * _filtstate[0]);
        _delay[1].write(_feedback[1] * _filtstate[1]);
        _delay[2].write(_feedback[2] * _filtstate[2]);
        _delay[3].write(_feedback[3] * _filtstate[3]);
    }
}

SimpleReverb2::SimpleReverb2(TRACK *track, int32_t channel) : Effect(track, channel,
                                                                     SPACE_GRAINREVERB,
                                                                     GRAINEFFECT) {
    _rvt = &_STATE->params[track->index][GRAINREVERBDECAY];
    _mix = &_STATE->params[track->index][GRAINREVERBMIX];
    _gain = &_STATE->params[track->index][GRAINREVERBGAIN];
    _bypass = &track->bypass[SPACE_GRAINREVERB];
    _prvt = 0.0f;

    int32_t s = compsize();
    buf.resize(s);


    int32_t offset = 0;

    for (int32_t i = 0; i < NUMCOMBS + NUMALLPASS; i++) {
        int32_t lpsiz = MYFLT2LRND(_lpt[i] * _STATE->sr);
        _start[i] = &buf[offset]; //new MYFLOAT[lpsiz]();
        _xp[i] = _start[i];
        _end[i] = _xp[i] + lpsiz;
        _coef[i] = 0.0;
        offset += lpsiz;
    }

    if (offset != s)
        LOGE("SimpleReverb : Terrible error");

    _coef[4] = exp(log001 * _lpt[4] / .1f);
    _coef[5] = exp(log001 * _lpt[5] / .1f);
}

void SimpleReverb2::compute(MYFLOAT *in, int32_t size) {
    if (_bypass->load())
        return;
    MYFLOAT mix = (MYFLOAT) *_mix;
    MYFLOAT mixsrc = 1.0 - mix;
    MYFLOAT rvt = *_rvt * 10.;
    if (rvt != _prvt) {
        _prvt = rvt;
        reset();
    }


    MYFLOAT gain = (MYFLOAT) dbToLinear60(*_gain);
    MYFLOAT sl = (MYFLOAT) (WINDOW_SIZE) / (MYFLOAT) size / (MYFLOAT) WINDOW_SIZE;
    MYFLOAT sp = 0.f;
    auto tindex = _track->index;

    auto window = _track->grainenv[GASENV];

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT inn = in[i] * window[PHS2INT(sp)];
        sp += sl;

        MYFLOAT tmp = *_xp[0];
        *_xp[0] *= _coef[0];
        *_xp[0] += inn;
        MYFLOAT out0 = tmp;
        if (++_xp[0] >= _end[0])
            _xp[0] = _start[0];

        tmp = *_xp[1];
        *_xp[1] *= _coef[1];
        *_xp[1] += inn;
        MYFLOAT out1 = tmp;
        if (++_xp[1] >= _end[1])
            _xp[1] = _start[1];

        tmp = *_xp[2];
        *_xp[2] *= _coef[2];
        *_xp[2] += inn;
        MYFLOAT out2 = tmp;
        if (++_xp[2] >= _end[2])
            _xp[2] = _start[2];

        tmp = *_xp[3];
        *_xp[3] *= _coef[3];
        *_xp[3] += inn;
        MYFLOAT out3 = tmp;
        if (++_xp[3] >= _end[3])
            _xp[3] = _start[3];

        MYFLOAT sum = (_filtstate = _filtstate * .9f + (out0 + out1 + out2 + out3) * .1f) * .5f;

        MYFLOAT y = *_xp[4], z;
        *_xp[4] = z = _coef[4] * y + sum;
        MYFLOAT out4 = y - _coef[4] * z;
        if (++_xp[4] >= _end[4])
            _xp[4] = _start[4];

        y = *_xp[5];
        *_xp[5] = z = _coef[5] * y + out4;
        MYFLOAT res = y - _coef[5] * z;
        if (++_xp[5] >= _end[5])
            _xp[5] = _start[5];

        in[i] = (inn * mixsrc + res * mix) * gain;
    }
}

Freeverb::Freeverb(TRACK *t) : Effect(t, SPACE_REVERB1, STEREOEFFECT), reverb(_STATE->sr) {
    _mix = &_STATE->params[t->index][REV1MIX];
    _gain = &_STATE->params[t->index][REV1GAIN];
    _smooth2 = dbToLinear60(*_gain);
    _room = &_STATE->params[t->index][REV1T60];
    _damp = &_STATE->params[t->index][REV1DAMP];
    _bypass = &t->bypass[SPACE_REVERB1];
}

NRev::NRev(TRACK *t) : Effect(t, SPACE_REVERB2, STEREOEFFECT), reverb(_STATE->sr) {
    _mix = &_STATE->params[t->index][REV2MIX];
    _gain = &_STATE->params[t->index][REV2GAIN];
    _smooth2 = dbToLinear60(*_gain);
    _room = &_STATE->params[t->index][REV2T60];
    _bypass = &t->bypass[SPACE_REVERB2];
    //_damp = &_STATE->params[t->index][REV1DAMP];
}

JCRev::JCRev(TRACK *t) : Effect(t, SPACE_REVERB3, STEREOEFFECT), reverb(_STATE->sr) {
    _mix = &_STATE->params[t->index][REV3MIX];
    _gain = &_STATE->params[t->index][REV3GAIN];
    _room = &_STATE->params[t->index][REV3REF];
    _bypass = &t->bypass[SPACE_REVERB3];
    //_damp = &_STATE->params[t->index][REV1DAMP];
}

PRCRev::PRCRev(TRACK *t) : Effect(t, SPACE_REVERB2, STEREOEFFECT), reverb(_STATE->sr) {
    _mix = &_STATE->params[t->index][REV2MIX];
    _gain = &_STATE->params[t->index][REV2GAIN];
    _room = &_STATE->params[t->index][REV2REF];
    _bypass = &t->bypass[SPACE_REVERB2];
    //_damp = &_STATE->params[t->index][REV1DAMP];
}
