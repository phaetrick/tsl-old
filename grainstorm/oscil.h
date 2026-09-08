#pragma once
//
// Created by pr on 29.12.17.
//

#ifndef GRAINSTORM_OSCIL_H
#define GRAINSTORM_OSCIL_H


#include <atomic>
#include <vector>
#include <array>
#include "types.h"
#include "defines.h"
#include "tools/aligned_memalloc.h"
#include "base.h"


class RINGG : public Effect {
public:
    RINGG(TRACK *t, int32_t chan);

    RINGG(const RINGG &o) : RINGG(o._track, o._chan) {};

    void setup();

    void compute(MYFLOAT *in, int32_t size)override ;

private:
    MYFLOAT phase{}, inc{}, mix{};
};

class GrainRingMod : public GrainEffect{
public:
    void prepare(TRACK *t, int32_t chan);

    MYFLOAT tick(MYFLOAT in, int32_t offset, int size) override ;

private:
    MYFLOAT phase{}, inc{}, mix{}, makeup{1.f};
    const MYFLOAT *_sine{};
};


class ModFM {
public:
    ModFM(int32_t sr_, std::atomic<MYFLOAT> *frm_, std::atomic<MYFLOAT> *frc_, std::atomic<MYFLOAT> *I_,
          std::atomic<MYFLOAT> *g_, std::atomic<MYFLOAT> *bw_) {
        gain = g_;
        sr = sr_;
        fm = frm_;
        oldfm = fm->load();
        fc = frc_;
        oldfc = fc->load();
        I = I_;
        bw = bw_;
        oldI = I->load();
        scal = computescal(oldI);
        incm = computephaseinc(LOG2NORMALF(oldfm), sr);
        incc = computephaseinc(LOG2NORMALF(oldfc), sr);
        phasem = phasec = 0;
        computescalandindexonbw();

    }

    void compute(MYFLOAT *buf, int32_t size) {
        MYFLOAT index = I->load();
        if (index != oldI) {
            scal = computescal(index);
            oldI = index;
        }
        MYFLOAT frm = fm->load();
        if (frm != oldfm) {
            oldfm = frm;
            incm = computephaseinc(LOG2NORMALF(frm), sr);
        }
        MYFLOAT frc = fc->load();
        if (frc != oldfc) {
            oldfc = frm;
            incc = computephaseinc(LOG2NORMALF(frc), sr);
        }

        MYFLOAT g = scal * LOG2NORMALF(gain->load());
        for (int32_t i = 0; i < size; i++) {
            buf[i] = g * exp(index * sin(phasem)) * cos(phasec);
            phasem += incm;
            phasem = fmod(phasem, TWOPI_F_P);
            phasec += incc;
            phasec = fmod(phasec, TWOPI_F_P);
        }

    }

    void computeRes(MYFLOAT *buf, int32_t size) {

        MYFLOAT frm = fm->load();
        if (frm != oldfm) {
            oldfm = frm;
            incm = computephaseinc(LOG2NORMALF(frm), sr);
            computescalandindexonbw();
        }
        MYFLOAT frc = fc->load();
        if (frc != oldfc) {
            oldfc = frm;
            incc = computephaseinc(LOG2NORMALF(frc), sr);
        }

        MYFLOAT bw_ = bw->load();
        if (bw_ != oldbw) {
            oldbw = bw_;
            computescalandindexonbw();
        }

        MYFLOAT g = scal * LOG2NORMALF(gain->load());
        for (int32_t i = 0; i < size; i++) {
            buf[i] = g * exp(oldI * sin(phasem)) * cos(phasec);
            phasem += incm;
            phasem = fmod(phasem, TWOPI_F_P);
            phasec += incc;
            phasec = fmod(phasec, TWOPI_F_P);
        }

    }

    static void Compute(ModFM *xx, MYFLOAT *in, int32_t s) {
        xx->computeRes(in, s);
    }

    static MYFLOAT computephaseinc(MYFLOAT fr, MYFLOAT samplerate) {
        return TWOPI_F_P * fr / samplerate;
    }

    static MYFLOAT computescal(MYFLOAT index) {
        return 1.f / exp(index);
    }

    void computescalandindexonbw() {
        MYFLOAT g = exp(-LOG2NORMALF(fm->load()) / (.29 * LOG2NORMALF(bw->load())));
        MYFLOAT g2 = 2 * sqrtf(g) / (1.f - g);
        oldI = g2 * g2 / 2;
        scal = 1.f / (exp(oldI));


    }


private:
    std::atomic<MYFLOAT> *I, *fm, *fc, *gain, *bw;
    MYFLOAT incm, incc, scal, sr;
    double oldI, oldfm, oldfc, oldbw;
    MYFLOAT phasem, phasec;
};


class VCO {
public:
    VCO(int32_t sr_, std::atomic<MYFLOAT> *freq_, std::atomic<MYFLOAT> *pw_,
        std::atomic<MYFLOAT> *waveform_, MYFLOAT *sintable, std::atomic<MYFLOAT> *gain_ = nullptr,
        MYFLOAT iphs = 0, MYFLOAT iskip = 0) {
        gain = gain_;
        ftp = sintable;
        sr = sr_;
        freq = freq_;
        pulsewidth = pw_;
        waveform = waveform_;
        /* Number of bytes in the delay */
        ndel = sr;

        if (iphs >= 0.0f)
            phase = (int32_t) (iphs * 0.5f * FMAXLEN);
            /* Does it need this? */
        else {
            phase = 0;
        }

        ampcod = 0;//ampbuf != nullptr ? 1 : 0;
        cpscod = 0;//cpsbuf != nullptr ? 1 : 0;

        if (iskip == 0.0f) {
            ynm1 = (waveform_->load() == 1.0f) ? -0.5f : 0.0f;
            ynm2 = 0.0f;
        }

        /* finished setting up buzz now set up internal vdelay */

        if (ndel == 0) ndel = 1;    /* fix due to Troxler */

        delaybuf.resize(ndel + 1, 0);
        delayindex = 0;


        nyq = .5;

        lenmask = (int32_t) (WINDOW_SIZE - 1);

        int32_t bits = 0;
        for (int32_t i = WINDOW_SIZE; i < (int) MAXLEN; i <<= 1) {
            bits++;

        };
        lobits = bits;
        phaseinc = (MYFLOAT) (MAXLEN - 1) / (MYFLOAT) sr;
    }

    void compute(MYFLOAT *out, int32_t size) {
        int32_t inc, dwnphs, tnp1;
        MYFLOAT leaky, /*rtfqc,*/ amp, fqc;
        MYFLOAT /*sicvt2, over2n, */scal, num, denom, pulse = 0.0f, saw = 0.0f;
        MYFLOAT sqr = 0.0f, tri = 0.0f;
        int32_t knh;

        /* VDelay Inserted here */
        MYFLOAT fv1, out1;
        int32_t v1, v2;
        /* Save recalculation and also round */

        fqc = freq->load();
        //rtfqc = SQRT(fqc);
        knh = (int32_t) (sr * nyq / fqc);
        tnp1 = knh + knh + 1;           /* calc 2n + 1 */
        MYFLOAT over2n = 0.5f / knh;

        amp = LOG2NORMALF(gain->load());
        scal = over2n;
        inc = (int32_t) (fqc * phaseinc * 0.5f);
        int32_t wave = waveform->load();
        leaky = (wave == 3.0) ? 0.995f : 0.999f;

/*-----------------------------------------------------*/
/* PWM Wave                                            */
/*-----------------------------------------------------*/
        if (wave == 2) {
            MYFLOAT pw = pulsewidth->load();
            for (int32_t n = 0; n < size; n++) {
                dwnphs = phase >> lobits;
                denom = *(ftp + dwnphs);
                if (denom > 0.00001f || denom < -0.00001f) {
                    num = *(ftp + (dwnphs * tnp1 & lenmask));
                    pulse = (num / denom - 1.0f) * scal;
                } else pulse = 1.0f;
                phase += inc;
                phase &= PHMASK;
                if (ampcod) {
                    amp = xamp[n];
                    //scal = over2n;        /* Why is this needed?? */
                }
                if (cpscod) {
                    fqc = xcps[n];
                    inc = (int32_t) (fqc * phaseinc * .5f);
                }

                /* VDelay inserted here */
                delaybuf[delayindex] = pulse;
                fv1 = (MYFLOAT) delayindex - sr * pw / fqc;

                v1 = (int32_t) fv1;
                if (fv1 < 0.0f) v1--;
                fv1 -= (MYFLOAT) v1;
                /* Make sure Inside the buffer */
                while (v1 >= ndel)
                    v1 -= ndel;
                while (v1 < 0)
                    v1 += ndel;
                /* Find next sample for interpolation      */
                v2 = (v1 < (ndel - 1) ? v1 + 1 : 0);
                out1 = delaybuf[v1] + fv1 * (delaybuf[v2] - delaybuf[v1]);

                if (++delayindex == ndel) delayindex = 0;             /* Advance current pointer */
                /* End of VDelay */

                sqr = pulse - out1 + leaky * ynm1;
                ynm1 = sqr;
                out[n] += ((sqr + pw - 0.5f) * 1.9f * amp);
            }
        }

            /*-----------------------------------------------------*/
            /* Triangle Wave                                       */
            /*-----------------------------------------------------*/
        else if (wave == 3) {
            MYFLOAT pw = pulsewidth->load();
            for (int32_t n = 0; n < size; n++) {
                dwnphs = phase >> lobits;
                denom = *(ftp + dwnphs);
                if (denom > 0.0002f || denom < -0.0002f) {
                    num = *(ftp + (dwnphs * tnp1 & lenmask));
                    pulse = (num / denom - 1.0f) * scal;
                }
                    /* else pulse = *ampp; */
                else pulse = 1.0f;
                phase += inc;
                phase &= PHMASK;

                /*
                if (ampcod) {
                    amp = xamp[n];
                }
                if (cpscod) {
                    fqc = xcps[n];
                    inc = (int32_t) (fqc * phaseinc * .5f);
                }
                */
                /* VDelay inserted here */
                delaybuf[delayindex] = pulse;
                fv1 = (MYFLOAT) delayindex - sr * pw / fqc;

                v1 = (int32_t) fv1;
                if (fv1 < 0.0f) v1--;
                fv1 -= (MYFLOAT) v1;
                /* Make sure Inside the buffer */
                while (v1 >= ndel)
                    v1 -= ndel;
                while (v1 < 0)
                    v1 += ndel;
                /* Find next sample for interpolation      */
                v2 = (v1 < (ndel - 1) ? v1 + 1 : 0);
                out1 = delaybuf[v1] + fv1 * (delaybuf[v2] - delaybuf[v1]);

                if (++delayindex == ndel) delayindex = 0;  /* Advance current pointer */
                /* End of VDelay */

                /* Integrate twice and ouput */
                sqr = pulse - out1 + leaky * ynm1;
                tri = sqr + leaky * ynm2;
                ynm1 = sqr;
                ynm2 = tri;
                out[n] += (tri * amp * fqc
                           / (sr * 0.42f * (0.05f + pw - pw * pw)));
            }
        }
            /*-----------------------------------------------------*/
            /* Sawtooth Wave                                       */
            /*-----------------------------------------------------*/
        else {
            for (int32_t n = 0; n < size; n++) {
                dwnphs = phase >> lobits;
                denom = *(ftp + dwnphs);
                if (denom > 0.0002f || denom < -0.0002f) {
                    num = *(ftp + (dwnphs * tnp1 & lenmask));
                    pulse = (num / denom - 1.0f) * scal;
                }
                    /* else pulse = *ampp; */
                else pulse = 1.0f;
                phase += inc;
                phase &= PHMASK;
                /*
                if (ampcod) {
                    amp = xamp[n];
                }
                if (cpscod) {
                    fqc = xcps[n];
                    inc = (int32) (fqc * sicvt2);
                }
*/
                /* Leaky Integration */
                saw = pulse + leaky * ynm1;
                ynm1 = saw;
                out[n] += (saw * 1.5f * amp);
            }
        }
    };

private:
    // OPDS    h;
    MYFLOAT *xamp, *xcps;
    MYFLOAT ynm1, ynm2, nyq;
    int32_t ampcod, cpscod;
    int32_t phase, ndel;
    MYFLOAT *ftp;
    /* Insert VDelay here */
    //AUXCH   aux;
    /* AUXCH   auxd; */
    /* End VDelay insert  */
    std::vector<MYFLOAT> delaybuf;
    int32_t delayindex;
    int32_t sr;
    int32_t lenmask, lobits;
    MYFLOAT phaseinc;
    std::atomic<MYFLOAT> *freq, *pulsewidth, *waveform, *gain;
};


class TableI {
public:
    TableI(int32_t N_, int type = 0) {
        N = N_;
        len = N - 1;
        table = (MYFLOAT *) aligned_malloc(sizeof(MYFLOAT) * (N_ + 2));
        if (type == 0) {
            for (int32_t i = 0; i < N; i++)
                table[i] = sinf(TWOPI_F_P * i / (MYFLOAT) N);
        } else {
            for (int32_t i = 0; i < N; i++)
                table[i] = cosf(TWOPI_F_P * i / (MYFLOAT) N);
        }
        table[N] = table[N - 1];
    }

    MYFLOAT tickI(MYFLOAT *phase) {
        if (*phase > 1.0)
            *phase -= 1.0;
        MYFLOAT fv1 = len * *phase; /* Broken*/
        int32_t v1 = (int) fv1;
        int32_t v2 = v1 + 1; /*Find next sample for interpolation*/
        return (table[v1] + (fv1 - v1) * (table[v2] - table[v1]));
        return table[v1];
    }

    MYFLOAT tick(MYFLOAT *phase) {
        if (*phase >= 1.0)
            *phase -= 1.0;
        return table[(int) (len * *phase)];
    }

    ~TableI() {
        aligned_free(table);
    }

    MYFLOAT *getTable() { return table; }

private:
    int32_t N;
    int32_t len;
    MYFLOAT *table;
};


class BlitSaw2 {
public:
    BlitSaw2(int32_t sr_, MYFLOAT *sintable_) {
        sr = sr_;
        sintable = sintable_;
        nHarmonics_ = 0;
        this->reset();
        this->setFrequency(80.f);
    }

    void reset() {
        phase_ = 0.0f;
        state_ = 0.0;
    }

    void setFrequency(MYFLOAT frequency) {
        p_ = sr / frequency;
        C2_ = 1 / p_;
        rate_ = PI_F_P * C2_;
        this->updateHarmonics();
    }

    void setHarmonics(uint32_t _nHarmonics) {
        nHarmonics_ = _nHarmonics;
        this->updateHarmonics();

        // I found that the initial DC offset could be minimized with an^M
        // initial state setting as given below.  This initialization should^M
        // only happen before starting the oscillator for the first time^M
        // (but after setting the frequency and number of harmonics).  I^M
        // struggled a bit to decide where best to put this and finally^M
        // settled on here.  In general, the user shouldn't be messing with^M
        // the number of harmonics once the oscillator is running because^M
        // this is automatically taken care of in the setFrequency()^M
        // function.  (GPS - 1 October 2005)^M
        state_ = -0.5f * a_;
    }

    void updateHarmonics(void) {
        if (nHarmonics_ <= 0) {
            uint32_t maxHarmonics = (unsigned int) floor(0.5 * p_);
            m_ = 2 * maxHarmonics + 1;
        } else
            m_ = 2 * nHarmonics_ + 1;
        a_ = m_ / p_;
    }


    void compute(MYFLOAT *out, int32_t size) {
        // The code below implements the BLIT algorithm of Stilson and
        // Smith, followed by a summation and filtering operation to produce
        // a sawtooth waveform.  After experimenting with various approaches
        // to calculate the average value of the BLIT over one period, I
        // found that an estimate of C2_ = 1.0 / period (in samples) worked
        // most consistently.  A "leaky integrator" is then applied to the
        // difference of the BLIT output and C2_. (GPS - 1 October 2005)

        // A fully  optimized version of this code would replace the two sin
        // calls with a pair of fast sin oscillators, for which stable fast
        // two-multiply algorithms are well known. In the spirit of STK,
        // which favors clarity over performance, the optimization has
        // not been made here.

        // Avoid a divide by zero, or use of a denormalized divisor
        // at the sinc peak, which has a limiting value of m_ / p_.
        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp, denominator = sin(phase_);
            if (fabs(denominator) <= std::numeric_limits<MYFLOAT>::epsilon())
                tmp = a_;
            else {
                tmp = sin(m_ * phase_);
                tmp /= p_ * denominator;
            }

            tmp += state_ - C2_;
            state_ = tmp * 0.995f;

            phase_ += rate_;
            if (phase_ >= PI_F_P) phase_ -= PI_F_P;
            out[i] = tmp;
        }
    }

protected:
    uint32_t nHarmonics_;
    uint32_t m_;
    MYFLOAT rate_;
    MYFLOAT phase_;
    MYFLOAT p_;
    MYFLOAT C2_;
    MYFLOAT a_;
    MYFLOAT state_;
    int32_t sr;
    MYFLOAT *sintable;
};


class Partials {
public:
    Partials(MYFLOAT *cosine, int32_t sr, MYFLOAT *freq,
             std::atomic<MYFLOAT> *numpart, std::atomic<MYFLOAT> *partmul,
             std::atomic<MYFLOAT> *partoffset,
             MYFLOAT *_cpsfact = nullptr, MYFLOAT *_portamento = nullptr) {
        ft = cosine;
        portamento = _portamento;
        kfreq = freq;
        prevfreq = 0;
        kparts = numpart;
        kpartoffset = partoffset;
        kr = partmul;
        cpsfact = _cpsfact;
        phaseinc = (WINDOW_SIZE - 1) / (MYFLOAT) sr;
        lenmask = (int32_t) (WINDOW_SIZE - 1);
        reported = 0;
        last = 1.0f;
        prvr = 0.0f;
        prvn = 0L;
    }

    MYFLOAT tick();

    void SetPhase(MYFLOAT ph) {
        phase = (int32_t) (ph * (WINDOW_SIZE - 1));
        prvr = 0.0f;
    }

    void setup(int32_t size);

    void compute(MYFLOAT *out, int32_t size) ;

    template<typename Iterator>
    void compute2(Iterator start, Iterator end);

    //Spline afreq;
    double prevfreq;
    MYFLOAT *cpsfact, *portamento;

private:
    int16_t prvn;
    MYFLOAT prvr, twor, rsqp1, rtn, rtnp1, rsumr;
    int32_t phase;
    int32_t reported;
    MYFLOAT last;
    int32_t lenmask;
    MYFLOAT phaseinc;
    std::atomic<MYFLOAT>*kparts, *kr, *kpartoffset;
    MYFLOAT *kfreq;
    MYFLOAT *ft;
    MYFLOAT scal, inc, incinc, r;
    int32_t k, km1, kpn, kpnm1;
};

class GrainPart : public Effect {
public:
    GrainPart(TRACK *t, int32_t chan);

    GrainPart(const GrainPart &o) : GrainPart(o._track, o._chan) {}

    void compute(MYFLOAT *in, int32_t s)override ;

    void setup(int, const MYFLOAT *in = nullptr);

    MYFLOAT tick(MYFLOAT in);

private:
    MYFLOAT gg;
    static MYFLOAT prevfreq[8];
    MYFLOAT __freq;
    Partials partials;
    MYFLOAT wet, dry;
};


class GrainBuzz : public GrainEffect{
public:
    void prepare(TRACK *t, int32_t chan, int grainsize);

    MYFLOAT tick(MYFLOAT in, int, int)override ;

private:
    void SetPhase(MYFLOAT ph) {
        phase = (int32_t) (ph * (WINDOW_SIZE));
    }

    MYFLOAT twor, rsqp1, rtn, rtnp1;
    int32_t phase;
    MYFLOAT last;
    MYFLOAT *ft;
    MYFLOAT scal, inc, incinc, r;
    int32_t k, km1, kpn, kpnm1;
    MYFLOAT wet, dry;
    static constexpr int32_t lenmask{WINDOW_SIZE - 1};
};


#endif //GRAINSTORM_OSCIL_H
