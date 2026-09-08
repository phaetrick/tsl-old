//
// DRIVE - see GrainDrive.h.
//

#include <cmath>
#include <algorithm>
#include "GrainDrive.h"
#include "track.h"
#include "grainstorm.h"
#include "random.h"
#include "lfo.h"

GrainDrive::GrainDrive(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_GRAINDRIVE, GRAINEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_GRAINDRIVE];
    _type = &_STATE->params[t->index][GRAINDRVTYPE];
    _dmin = &_STATE->params[t->index][GRAINDRVMIN];
    _dmax = &_STATE->params[t->index][GRAINDRVMAX];
    _mix = &_STATE->params[t->index][GRAINDRVMIX];
    _gain = &_STATE->params[t->index][GRAINDRVGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _lfo_drive = &t->lfo[GRAINDRVMIN];
}

void GrainDrive::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(*_gain);
    const MYFLOAT mixTarget = (*_bypass || destroyRequested) ? 0. : (MYFLOAT) *_mix;

    const int type = Effect::limit((int) _type->load(), 0, (int) DRIVE_CHEBY);

    // One drive value per grain, drawn in dB so the range is perceptually even.
    // An LFO on this destination moves the centre of the MIN/MAX window and
    // keeps its width, so the drive swells across the cloud while individual
    // grains still land at different amounts.
    const MYFLOAT dbmin = _dmin->load(), dbmax = _dmax->load();
    MYFLOAT centre = (dbmin + dbmax) * .5;
    const MYFLOAT spread = std::fabs(dbmax - dbmin) * .5;
    const LFO *lfoD = _lfo_drive->load();
    if (lfoD && lfoD->power()) {
        const MYFLOAT a = lfoD->min(GRAINDRVMIN), b = lfoD->max(GRAINDRVMIN);
        centre = a + lfoD->buf[grainLfoIndex()] * (b - a);
    }
    const MYFLOAT d = std::max(1e-3, (MYFLOAT) LOG2NORMALF(
            tsl::random::randomfloat(centre - spread, centre + spread)));

    // Normalisers: shaper(1) at this drive, so full scale in -> full scale out.
    MYFLOAT norm = 1.;
    switch (type) {
        case DRIVE_SOFT:  norm = std::tanh(d); break;
        case DRIVE_ASYM:
            // The two halves clip at different levels - that IS the asymmetry -
            // so normalise by the larger of them. Using only the positive half
            // (the obvious reading of "shaper(1)") lets the negative half reach
            // 2.2x full scale at high drive.
            norm = std::max(std::fabs(std::tanh(d + ASYM_BIAS) - std::tanh(ASYM_BIAS)),
                            std::fabs(std::tanh(-d + ASYM_BIAS) - std::tanh(ASYM_BIAS)));
            break;
        default:          norm = 1.; break;     // HARD/FOLD/CHEBY are bounded by 1
    }
    const MYFLOAT invNorm = norm > 1e-9 ? 1. / norm : 1.;

    // CHEBY: drive picks how far up the harmonic series the weights reach.
    // a is the ratio between successive Chebyshev terms.
    const MYFLOAT a = type == DRIVE_CHEBY ? Effect::limit(std::log10(d) * .5, 0., .95) : 0.;
    const MYFLOAT chebyNorm = 1. / (1. + a + a * a + a * a * a + a * a * a * a);

    for (int32_t i = 0; i < size; i++) {
        const MYFLOAT x = in[i];
        MYFLOAT y;
        switch (type) {
            case DRIVE_HARD:
                y = Effect::limit(d * x, -1., 1.);
                break;
            case DRIVE_FOLD:
                // sine folder: past the first fold the excess turns back on
                // itself instead of flattening
                y = std::sin(PI_P * .5 * d * x);
                break;
            case DRIVE_ASYM:
                y = (std::tanh(d * x + ASYM_BIAS) - std::tanh(ASYM_BIAS)) * invNorm;
                break;
            case DRIVE_CHEBY: {
                // T_k are only the k-th harmonic for |x| <= 1
                const MYFLOAT xc = Effect::limit(d * x, -1., 1.);
                const MYFLOAT x2 = xc * xc;
                const MYFLOAT t2 = 2. * x2 - 1.;
                const MYFLOAT t3 = (4. * x2 - 3.) * xc;
                const MYFLOAT t4 = 8. * x2 * x2 - 8. * x2 + 1.;
                const MYFLOAT t5 = (16. * x2 * x2 - 20. * x2 + 5.) * xc;
                y = (xc + a * t2 + a * a * t3 + a * a * a * t4 + a * a * a * a * t5) * chebyNorm;
                break;
            }
            case DRIVE_SOFT:
            default:
                y = std::tanh(d * x) * invNorm;
                break;
        }

        // DC blocker: ASYM and CHEBY are asymmetric by construction, and a
        // constant offset inside a grain becomes a thump once the grain
        // envelope multiplies it.
        const MYFLOAT dc = y - _dcx + .995 * _dcy;
        _dcx = y;
        _dcy = dc;

        in[i] = x * (1. - smmix()) + dc * smmix() * smgain();
        smmixgain(mixTarget, gain);
    }
    UDD(_dcx);
    UDD(_dcy);
}
