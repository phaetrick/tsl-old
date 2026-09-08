//
// Created by pr on 31.08.20.
//

#ifndef GRAINSTORM_REVERBPROGENITOR_H
#define GRAINSTORM_REVERBPROGENITOR_H


#include "Allpass.h"
#include "defines.h"


template<typename T>
class DCBlocker {
public:
    void clear() { _z = _output = 0; }

    T process(T input) {
        _output = input - _z + _b * _output;
        _z = input;
        return _output;
    }

private:
    T _output{};
    T _b{.9999};
    T _z{};
};

template<typename T>
class OnePole2 {
public:
    void setB0(T b0) { _b0 = b0; };

    //! Set the a[1] coefficient value.
    void setA1(T a1) { _a1 = a1; };

    void setCoefficients(T b0, T a1, bool clearState = false) {
        if (std::abs(a1) >= 1.0) {
            return;
        }

        _b0 = b0;
        _a1 = a1;

        if (clearState) _state = 0;
    }

    void setPole(T thePole) {
        if (std::abs(thePole) >= 1.0) {
            return;
        }

        // Normalize coefficients for peak unity gain.
        if (thePole > 0.0)
            _b0 = (T) (1.0 - thePole);
        else
            _b0 = (T) (1.0 + thePole);

        _a1 = -thePole;
    }

    T tick(T input) {
        return _state = (_b0 * input - _a1 * _state);
    }

    void undenormalize() {
        UNDENORMAL(_state);
    }

private:
    T _a1{}, _b0{}, _state{};
};

template<typename T>
class ReverbButter1 {
public:
    ReverbButter1() = default;
    ReverbButter1(T sr, T hpcut = 18, T lpcut = 20000) {
        init(sr, hpcut, lpcut);
    }

    void init(T sr, T hpcut = 18, T lpcut = 20000) {
        pidsr = PI_P / (T) sr;
        currenthp = nexthp = hpcut;
        currentlp = nextlp = lpcut;
        updateLp();
        updateHP();
        reset();
    }

    void reset() {
        y1_h  = 0.0;
        y1_l = 0.0;
    }

    inline T tick(T in) {
        if (nextlp != currentlp) {
            if (DISTANCE(nextlp, currentlp) < 3) {
                currentlp = nextlp;
            } else if (currentlp < nextlp)
                currentlp += 3;
            else
                currentlp -= 3;
            updateLp();
        }

        if (nexthp != currenthp) {
            if (DISTANCE(currenthp, nexthp) < 3) {
                currenthp = nexthp;
            } else if (currenthp < nexthp)
                currenthp += 3;
            else
                currenthp -= 3;
            updateHP();
        }
        T tmp = in * b1_h + y1_h;
        y1_h = tmp * a2_h + in * b2_h;
        T output = tmp * b1_l + y1_l;
        y1_l = output * a2_l + tmp * b2_l;
        return output;
    }

    inline T ticklphp(T in) {
        T tmp = in * b1_h + y1_h;
        y1_h = tmp * a2_h + in * b2_h;
        T output = tmp * b1_l + y1_l;
        y1_l = output * a2_l + tmp * b2_l;
        return output;
    }

    inline T ticklp(T in) {
        T output = in * b1_l + y1_l;
        y1_l = output * a2_l + in * b2_l;
        return output;
    }

    void setNextLp(T lp) {
        nextlp = lp;
    };

    void setNextHp(T hp) {
        nexthp = hp;
    }

    void setLp(T lp) {
        currentlp = nextlp = lp;
        updateLp();
    };

    void setHp(T hp) {
        currenthp = nexthp = hp;
        updateHP();
    }

    void undenormalize() {
        UNDENORMAL(y1_h);
        UNDENORMAL(y1_l);
    }

private:

    inline void updateHP() {
        T omega_2 = currenthp * pidsr;
        T tan_omega_2 = std::tan(omega_2);
        b1_h = 1 / (1 + tan_omega_2);
        b2_h = -1 * b1_h;
        a2_h = (1 - tan_omega_2) / (1 + tan_omega_2);
    }

    inline void updateLp() {
        T omega_2 = currentlp * pidsr;
        T tan_omega_2 = std::tan(omega_2);
        b1_l = b2_l = tan_omega_2 / (1 + tan_omega_2);
        a2_l = (1 - tan_omega_2) / (1 + tan_omega_2);
    }

    T nexthp{}, currenthp{}, a2_h{}, b1_h{}, b2_h{}, y1_h{};
    T currentlp{}, nextlp{}, a2_l{}, b1_l{}, b2_l{}, y1_l{};
    T pidsr;
};


template<typename T>
class ReverbButter {
public:
    ReverbButter(T sr, T hpcut = 18, T lpcut = 20000) {
        pidsr = PI_P / (T) sr;
        currenthp = nexthp = hpcut;
        currentlp = nextlp = lpcut;
        updateLp();
        updateHP();
        reset();
    }

    void reset() {
        y1_h = y2_h = 0.0;
        y1_l = y2_l = 0.0;
    }

    inline T tick(T in) {
        if (nextlp != currentlp) {
            if (DISTANCE(nextlp, currentlp) < 3) {
                currentlp = nextlp;
            } else if (currentlp < nextlp)
                currentlp += 3;
            else
                currentlp -= 3;
            updateLp();
        }

        if (nexthp != currenthp) {
            if (DISTANCE(currenthp, nexthp) < 3) {
                currenthp = nexthp;
            } else if (currenthp < nexthp)
                currenthp += 3;
            else
                currenthp -= 3;
            updateHP();
        }

        T y0 = in + b1_h * y1_h + b2_h * y2_h;
        T tmp = a0_h * (y0 - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0;
        y0 = tmp + b1_l * y1_l + b2_l * y2_l;
        tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    inline T ticklphp(T in) {
        T y0 = in + b1_h * y1_h + b2_h * y2_h;
        T tmp = a0_h * (y0 - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0;
        y0 = tmp + b1_l * y1_l + b2_l * y2_l;
        tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    inline T ticklp(T in) {
        T y0 = in + b1_l * y1_l + b2_l * y2_l;
        T tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    void setNextLp(T lp) {
        nextlp = lp;
    };

    void setNextHp(T hp) {
        nexthp = hp;
    }

    void setLp(T lp) {
        currentlp = nextlp = lp;
        updateLp();
    };

    void setHp(T hp) {
        currenthp = nexthp = hp;
        updateHP();
    }

    void undenormalize() {
        UNDENORMAL(y1_h);
        UNDENORMAL(y2_h);
        UNDENORMAL(y1_l);
        UNDENORMAL(y2_l);
    }

private:

    inline void updateHP() {
        double pfreq = currenthp * pidsr;
        double C = tan(pfreq);
        double C2 = C * C;
        double sqrt2C = C * ROOT2;
        a0_h = 1.f / (1.f + sqrt2C + C2);
        b1_h = 2.f * (1.f - C2) * a0_h;
        b2_h = -(1.f - sqrt2C + C2) * a0_h;
    }

    inline void updateLp() {
        double pfreq = currentlp * pidsr;
        double C = 1.f / tan(pfreq);
        double C2 = C * C;
        double sqrt2C = C * ROOT2;
        a0_l = 1.f / (1.f + sqrt2C + C2);
        b1_l = -2.f * (1.f - C2) * a0_l;
        b2_l = -(1.f - sqrt2C + C2) * a0_l;
    }

    T nexthp{}, currenthp{}, a0_h{}, b1_h{}, b2_h{}, y1_h{}, y2_h{};
    T currentlp{}, nextlp{}, a0_l{}, b1_l{}, b2_l{}, y1_l{}, y2_l{};
    T pidsr;
};

template<typename T>
class Filt1 {
public:
    Filt1(void) : _slo(0), _shi(0) {}

    ~Filt1(void) {}


    void set_params(T del, T tmf, T tlo, T wlo, T thi, T chi) {
        T g, t;
        _gmf = pow(0.001, del / tmf);
        _glo = pow(0.001, del / tlo) / _gmf - 1.0;
        _wlo = wlo;
        g = pow(0.001, del / thi) / _gmf;
        t = (1 - g * g) / (2 * g * g * chi);
        _whi = (sqrt(1 + 4 * t) - 1) / (2 * t);
    }

    T process(T x) {
        _slo += _wlo * (x - _slo) + 1e-10;
        x += _glo * _slo;
        _shi += _whi * (x - _shi);
        return _gmf * _shi;
    }

    void reset() {
        _slo = _shi = 0;
    }

private:
    T _gmf;
    T _glo;
    T _wlo;
    T _whi;
    T _slo;
    T _shi;
};

class Pareq {
public:

    Pareq(void);

    ~Pareq(void);

    void setfsamp(MYFLOAT fsamp);

    void setparam(MYFLOAT f, MYFLOAT g) {
        _f0 = f;
        _g0 = powf(10.0f, 0.05f * g);
        _touch0++;
    }

    void reset(void);

    void prepare(int nsamp);

    void process(int nsamp, int nchan, MYFLOAT *data[]) {
        if (_state != 0) process1(nsamp, nchan, data);
    }

private:

    enum {
        BYPASS, STATIC, SMOOTH, MAXCH = 4
    };

    void calcpar1(int nsamp, MYFLOAT g, MYFLOAT f);

    void process1(int nsamp, int nchan, MYFLOAT *data[]);

    volatile int16_t _touch0;
    volatile int16_t _touch1;
    bool _bypass;
    int _state;
    MYFLOAT _fsamp;

    MYFLOAT _g0, _g1;
    MYFLOAT _f0, _f1;
    MYFLOAT _c1, _dc1;
    MYFLOAT _c2, _dc2;
    MYFLOAT _gg, _dgg;

    MYFLOAT _z1[MAXCH];
    MYFLOAT _z2[MAXCH];
};

template<typename T>
class Delay {
public:
    void init(int size) {
        _line.resize(size, 0);
        _size = _line.size();
    }

    int getDelay() { return _size; };

    T read(void) {
        return _line[_i];
    }

    void write(MYFLOAT x) {
        _line[_i++] = x;
        if (_i == _size) _i = 0;
    }

    T readwrite(T x) {
        T z = _line[_i];
        _line[_i] = x;
        if (++_i == _size) _i = 0;
        return z;
    }

    T tap(int pos) {
        int offset = _i - pos;
        if (offset < 0)
            offset += _size;
        return _line[offset];
    }

    void reset() {
        std::fill(_line.begin(), _line.end(), 0);
        _i = 0;
    }

private:
    int _i{};
    int _size{};
    std::vector<T> _line;
};


template<typename T>
class OnePoleLp {
public:
    void setCoeff(T c) {
        _c = c;
    }

    T tickbw(T in) {
        return _state = (in * _c + (1 - _c) * _state);
    }

    T tickdamping(T in) {
        return _state = (in * (1 - _c) + _c * _state);
    }

    void undenormalize() {
        UNDENORMAL(_state);
    }

    void reset() {
        _state = 0;
    }

private:
    T _c{};
    T _state{};
};

template<typename T>
class Vdelay {
public:

    ~Vdelay(void) {
        fini();
    }


    void init(int size) {
        _size = size;
        _line = new T[size];
        memset(_line, 0, size * sizeof(T));
        _ir = 0;
        _iw = 0;
    }


    void fini(void) {
        delete[] _line;
        _size = 0;
        _line = 0;
    }


    void set_delay(int del) {
        reset();
        _ir = _iw - del;
        if (_ir < 0) _ir += _size;
    }

    inline T read(void) {
        T x = _line[_ir++];
        if (_ir == _size) _ir = 0;
        return x;
    }

    inline void write(T x) {
        _line[_iw++] = x;
        if (_iw == _size) _iw = 0;
    }

    inline T readwrite(T in) {
        write(in);
        return read();
    }

    inline T tap(int delay) {
        int32_t offset = _iw - delay;
        if (offset < 0)
            offset += _size;
        return _line[offset];
    }


    void reset() {
        memset(_line, 0, sizeof(T) * _size);
    }

private:
    int _ir;
    int _iw;
    int _size;
    T *_line;
};


template<typename T>
class SimpleDelay2 {
public:
    void init(int32_t maxdelay, int32_t initdelay = 0) {
        delayline.resize(maxdelay, 0);
        delay = initdelay;
    }

    inline void write(T val) {
        delayline[index] = val;
        if (++index >= delay)
            index = 0;
    }

    inline T read() {
        return delayline[index];
    }


    inline T tick(T val) {
        delayline.data()[index] = val * _fadein;
        _fadein += _phincin;
        if (_fadein > 1.0) {
            _phincin = 0;
            _fadein = 1.0;
        }
        _fadeout -= _phincout;
        if (_fadeout < 0) {
            setDelay(_nextdelay);
        }
        if (++index >= delay)
            index = 0;
        return delayline[index] * _fadeout;
    }

    inline T tickn(T val) {
        delayline.data()[index] = val;
        if (++index >= delay)
            index = 0;
        return delayline[index];
    }


    inline void reset() {
        std::fill(delayline.begin(), delayline.end(), 0);
    }

    void setDelayms(MYFLOAT ms, int sr) {
        int _delay = (int) (sr * ms * .001);
        _nextdelay = _delay;
        _phincout = _fadeconst;
        _phincin = 0;
    }

    void setDelay(int32_t delaysmpls) {

        if (delaysmpls > delayline.size())
            delayline.resize(delaysmpls);
        reset();
        delay = delaysmpls;
        index = 0;
        _phincin = _fadeconst;
        _fadein = 0;
        _phincout = 0;
        _fadeout = 1;
    }

private:
    int32_t delay, _nextdelay;
    std::vector<T> delayline;
    int index{};
    const T _fadeconst = 2. / 48000.;
    T _phincin{0}, _phincout{0};
    T _fadein{1}, _fadeout{1};
};


#include <app.h>
template<typename T>
class Progenitor1  {
public:
    Progenitor1(tsl::AppState* _appState) {
        _sr = _sr;
        _scale = _sr / _internalsr;
        _internalsr = _sr;
        _decay = &_appState->params[0][REV3REF];
        _lpcutold = *(_lpcut = &_appState->params[0][REV4LPCUT]);
        _hpcutold = *(_hpcut = &_appState->params[0][REV4HPCUT]);
        _mix = &_appState->params[0][REV3MIX];
        _gain = &_appState->params[0][REV4GAIN];
        _predelay = &_appState->params[0][REV4PREDELAY];
        _predelayprev = *_predelay;
        _predelayL.init(_sr * 1.05, _predelayprev * _sr * 0.001);
        _predelayR.init(_sr * 1.05, _predelayprev * _sr * 0.001);

        for (int i = 0; i < 8; i++)
            _outtaps[i] = (int) (_outtaps[i] * _scale);

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
        //  void init(int size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

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
        //  void init(int size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

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


        lpfL_in_59_60.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
        lpfR_in_64_65.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

        lpfLdamp_11_12.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
        lpfRdamp_13_14.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

        lpfL_9_10.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
        lpfR_7_8.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

        out1_lpf.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));
        out2_lpf.init(_sr, LOG2NORMALF(_hpcutold), LOG2NORMALF(_lpcutold));

        delayL_16.init(2 * _scale);
        delayL_23.init(findNextPrime(1055 * _scale));
        delayL_31.init(findNextPrime(344 * _scale));
        delayL_37.init(findNextPrime(1572 * _scale) + 5);
        delayR_40.init(findNextPrime(625 * _scale) + 5);
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

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int s) {
        check();
        const MYFLOAT decay = .6f + _decay->load() * .399f;
        MYFLOAT gain = LOG2NORMALF(*_gain);
        MYFLOAT mix = *_mix;
        MYFLOAT mixsrc = (1.0f - mix) * gain;
        mix *= gain;

        UDD(_lastL);
        UDD(_lastR);

        for (int i = 0; i < s; i++) {
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

            MYFLOAT tapL = delayR_49.tap(_outtaps[0]) * .438 + delayR_40.tap(_outtaps[1]) * .938 -
                         delayL_31.tap(_outtaps[2]) * .438 + delayR_58.tap(_outtaps[3]) * .125;
            MYFLOAT tapR = delayL_31.tap(_outtaps[4]) * .438 + delayL_23.tap(_outtaps[5]) * .938 -
                         delayR_49.tap(_outtaps[6]) * .438 + delayL_37.tap(_outtaps[7]) * .125;

            outl[i] *= mixsrc;
            outl[i] += tapL * mix;
            //outl[i + 1] *= mixsrc;
            //outl[i + 1] += tapL * mix;
            outr[i] *= mixsrc;
            outr[i] += tapR * mix;
            //outr[i + 1] *= mixsrc;
            //outr[i + 1] += tapR * mix;

        }
    }

    void tick(MYFLOAT inl, MYFLOAT inr, MYFLOAT *outl, MYFLOAT *outr) {
        //const double decay = _loopdecay; //_decay->load() * .999;


        UDD(_lastL);
        UDD(_lastR);

        MYFLOAT lastL = out1_lpf.tick(delayL_37.readwrite(allpass3L_34_37.tick3(
                delayL_31.readwrite(allpass2L_25_27.tick2(delayL_23.readwrite(
                        allpassmL_17_18.tick1(
                                delayL_16.readwrite(allpassmL_15_16.tick1(
                                        lpfLdamp_11_12.tick(
                                                (lpfL_in_59_60.tick(
                                                        _predelayL.tick(inl * .5f)) +
                                                 _loopdecay * _lastR)))))))))));

        MYFLOAT lastR = out2_lpf.tick(delayR_58.readwrite(allpass3R_52_55.tick3(
                delayR_49.readwrite(allpass2R_43_45.tick2(delayR_41.readwrite(
                        delayR_40.readwrite(
                                allpassmR_21_22.tick1(allpassmR_19_20.tick1(
                                        lpfRdamp_13_14.tick(
                                                (lpfR_in_64_65.tick(
                                                        _predelayR.tick(inr * .5f)) +
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
        *outl = delayR_49.tap(_outtaps[0]) * .438f + delayR_40.tap(_outtaps[1]) * .938f -
                delayL_31.tap(_outtaps[2]) * .438f + delayR_58.tap(_outtaps[3]) * .125f;
        *outr = delayL_31.tap(_outtaps[4]) * .438f + delayL_23.tap(_outtaps[5]) * .938f -
                delayR_49.tap(_outtaps[6]) * .438f + delayL_37.tap(_outtaps[7]) * .125f;

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
            _predelayL.setDelay(_predelayprev * _sr * 0.001);
            _predelayR.setDelay(_predelayprev * _sr * 0.001);
        }
    }


private:
    // int _outtaps[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572,};
    int _outtaps[8] = {468, 625, 312, 8, 40, 0, 192, 1572};

    T _sr{48000}, _internalsr = 34125.0;

    T _scale{}, _totaldelay{}, _loopdecay{.5f};
    T _lastL{}, _lastR{};
    ReverbButter1<T> lpfL_in_59_60, lpfR_in_64_65, lpfLdamp_11_12, lpfRdamp_13_14;
    ReverbButter1<T> lpfL_9_10, lpfR_7_8, out1_lpf, out2_lpf;
    Delay<T> delayL_16, delayL_23, delayL_31, delayL_37;
    Delay<T> delayR_49, delayR_ts, delayR_40, delayR_41, delayR_58;

    ApModRnd<T> allpassmL_15_16, allpassmL_17_18, allpassmR_19_20, allpassmR_21_22;
    ApModRnd<T> allpass2L_25_27, allpass2R_43_45;
    ApModRnd<T> allpass3L_34_37, allpass3R_52_55;
    std::atomic<T> *_decay, *_lpcut, *_hpcut, *_mix, *_gain, *_predelay;
    T _olddamp{-1}, _olddec{-1}, _predelayprev{}, _lpcutold, _hpcutold;
    SimpleDelay2<T> _predelayL, _predelayR;
    DCBlocker<T> dcl, dcr;
};

template<typename T>
class Progenitor1Dark{
public:
    Progenitor1Dark(tsl::AppState* _appState) {
        _sr = _appState->sr;
        _scale = (_sr / 2.) / _internalsr;
        _internalsr = _sr / 2.;
        _decay = &_appState->params[0][REV3REF];
        _mix = &_appState->params[0][REV3MIX];
        _gain = &_appState->params[0][REV3GAIN];

        _damp = &_appState->params[0][REV3DAMP];

        for (int i = 0; i < 8; i++)
            _outtaps[i] = _outtaps[i] * _scale;

//        MYFLOAT sr, int delay, T c, MYFLOAT rate, int depth, MYFLOAT randfact = 0.0, MYFLOAT phase = 0.0,
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
        //  void init(int size1, T c1, T decay1, int size2, T c2, T decay2, MYFLOAT randfact = 0.0, T _sr = 1.0, int depth = 0, MYFLOAT rate1 = 0, MYFLOAT phase1 = 0, MYFLOAT rate2 = 0, MYFLOAT phase2 = 0) {

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

    void compute(T *inl, T *inr, T *outl, T *outr, int s) {
        //earlyrefl.processreplace(inl, inr, outl, outr, _size);
        //return;
        check();
        //const double decay = _loopdecay; //_decay->load() * .999;
        MYFLOAT gain = LOG2NORMALF(*_gain);
        MYFLOAT mix = *_mix;
        MYFLOAT mixsrc = (1.0 - mix) * gain;
        mix *= gain;
        _loopdecay = .6 + _decay->load() * .399;

        for (int i = 0; i < s; i += 2) {
            //delayL_37.readwrite(inl[i]);
            //outl[i] = outr[i] = delayL_37.tap(_outtaps[10]);
            //continue;
            // _rsl[i] = _lastR = allpass3L_34_37.tick2(.5 * rsl[i] + _lastR);
            // _rsr[i] = _lastL = allpass3R_52_55.tick2(.5 * rsr[i] + _lastR);

            T lastL = dccutL.process(out1_lpf.tickbw(delayL_37.readwrite(allpass3L_34_37.tick3(
                    delayL_31.readwrite(allpass2L_25_27.tick2(delayL_23.readwrite(
                            allpassmL_17_18.tick1(
                                    delayL_16.readwrite(allpassmL_15_16.tick1(
                                            lpfLdamp_11_12.tickdamping(
                                                    (lpfL_in_59_60.tickbw(inl[i] * .5) +
                                                     _loopdecay * _lastR))))))))))));

            T lastR = dccutR.process(out2_lpf.tickbw(delayR_58.readwrite(allpass3R_52_55.tick3(
                    delayR_49.readwrite(allpass2R_43_45.tick2(delayR_41.readwrite(
                            delayR_40.readwrite(
                                    allpassmR_21_22.tick1(allpassmR_19_20.tick1(
                                            lpfRdamp_13_14.tickdamping(
                                                    (lpfR_in_64_65.tickbw(inr[i] * .5) +
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
            T tapL = (delayR_49.tap(_outtaps[0]) * .438 + delayR_40.tap(_outtaps[1]) * .938 -
                         delayL_31.tap(_outtaps[2]) * .438 + delayR_58.tap(_outtaps[3]) * .125) * mix;
            T tapR = (delayL_31.tap(_outtaps[4]) * .438 + delayL_23.tap(_outtaps[5]) * .938 -
                         delayR_49.tap(_outtaps[6]) * .438 + delayL_37.tap(_outtaps[7]) * .125) * mix;
            outl[i] = inl[i] * mixsrc + tapL;
            outl[i + 1] = inl[i + 1] * mixsrc + tapL;
            outr[i] = inr[i] * mixsrc + tapR;
            outr[i + 1] = inr[i + 1] * mixsrc + tapR;

        }
    }

    void check() {
        if (_olddamp != _damp->load()) {
            _olddamp = _damp->load();

            lpfL_in_59_60.setCoeff(1.0 - _damp->load() * .9);
            lpfR_in_64_65.setCoeff(1.0 - _damp->load() * .9);
            out1_lpf.setCoeff(1.0 - _damp->load() * .9);
            out2_lpf.setCoeff(1.0 - _damp->load() * .9);

            lpfLdamp_11_12.setCoeff(_damp->load() * .9);
            lpfRdamp_13_14.setCoeff(_damp->load() * .9);
            lpfL_9_10.setCoeff(_damp->load() * .9);
            lpfR_7_8.setCoeff(_damp->load() * .9);
        }
        if (_olddec != _decay->load()) {
            _olddec = _decay->load();
            resetdecay();
        }
    }

    void resetdecay() {
        T T60 = LOG2NORMALF(_olddec);
        _loopdecay = pow(0.001, (_totaldelay) / (T60 * _internalsr));
    }

    // Clears all internal delay-line/filter memory so re-enabling the reverb
    // after it's been switched off doesn't dump a stale, frozen tail back in.
    void reset() {
        _lastL = _lastR = 0;
        dccutL.clear(); dccutR.clear();
        lpfL_in_59_60.reset(); lpfR_in_64_65.reset();
        lpfLdamp_11_12.reset(); lpfRdamp_13_14.reset();
        lpfL_9_10.reset(); lpfR_7_8.reset();
        out1_lpf.reset(); out2_lpf.reset();
        delayL_16.reset(); delayL_23.reset(); delayL_31.reset(); delayL_37.reset();
        delayR_49.reset(); delayR_ts.reset(); delayR_40.reset(); delayR_41.reset(); delayR_58.reset();
        allpassmL_15_16.reset(); allpassmL_17_18.reset(); allpassmR_19_20.reset(); allpassmR_21_22.reset();
        allpass2L_25_27.reset(); allpass2R_43_45.reset();
        allpass3L_34_37.reset(); allpass3R_52_55.reset();
    }

private:
    int _outtaps[8] = {468, 625, 312, 8, 40, 0, 192, 1572};

    //int _outtaps[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572,};
    T _sr{48000}, _internalsr = 34125.0;

    T _scale{}, _totaldelay{}, _loopdecay{.5f};
    T _lastL{}, _lastR{};
    DCBlocker<T> dccutL, dccutR;
    OnePoleLp<T> lpfL_in_59_60, lpfR_in_64_65, lpfLdamp_11_12, lpfRdamp_13_14;
    OnePoleLp<T> lpfL_9_10, lpfR_7_8, out1_lpf, out2_lpf;
    Delay<T> delayL_16, delayL_23, delayL_31, delayL_37;
    Delay<T> delayR_49, delayR_ts, delayR_40, delayR_41, delayR_58;

    ApModRnd<T> allpassmL_15_16, allpassmL_17_18, allpassmR_19_20, allpassmR_21_22;
    ApModRnd<T> allpass2L_25_27, allpass2R_43_45;
    ApModRnd<T> allpass3L_34_37, allpass3R_52_55;
    std::atomic<T> *_decay, *_damp, *_mix, *_gain;
    T _olddamp{-1}, _olddec{-1};
};

template<typename T>
class Datorro1 {
public:
    Datorro1(tsl::AppState* _appState) :
                         prelp(_appState->sr, LOG2NORMALF(_appState->params[0][REV3HPCUT]),
                                          LOG2NORMALF(_appState->params[0][REV3LPCUT])),
                         damping1(_appState->sr, LOG2NORMALF(_appState->params[0][REV3HPCUT]),
                                  LOG2NORMALF(_appState->params[0][REV3LPCUT])),
                         damping2(_appState->sr, LOG2NORMALF(_appState->params[0][REV3HPCUT]),
                                  LOG2NORMALF(_appState->params[0][REV3LPCUT])) {
        _sr = _appState->sr;
        _scale = _sr / _dattorroSampleRate;
        _lpcutold = *(_lpcut = &_appState->params[0][REV3LPCUT]);
        _hpcutold = *(_hpcut = &_appState->params[0][REV3HPCUT]);
        _mix = &_appState->params[0][REV3MIX];
        _gain = &_appState->params[0][REV3GAIN];
        _decay = &_appState->params[0][REV3REF];
        _damp = &_appState->params[0][REV3DAMP];
        _predelay = &_appState->params[0][REV3PREDELAY];
        _predelayprev = *_predelay;
        predelay.init(_sr * 1.05, _predelayprev * _sr * 0.001);
        //  prelp.setCoeff(1.0f - _damp->load() * .9f);
        //   _olddamp = _damp->load();
        //   prelp.setLPF_BW(0.05 + .45 - _olddamp * .45, 1);
        //damping1.setCoeff(_damp->load() * .9f);
        //damping2.setCoeff(_damp->load() * .9f);
        decaydiff11.init(_sr, findNextPrime(673 * _scale) / _sr, 0.0005);
        decaydiff11.setRndRate(3.7334);
        decaydiff11.setDiff(-.7);
        decaydiff12.init(_sr, findNextPrime(907 * _scale) / _sr, 0.0007);
        decaydiff12.setRndRate(3.4234);
        decaydiff12.setDiff(-.7);
        decaydiff21.init(_sr, findNextPrime(1801 * _scale) / _sr, 0.001);
        decaydiff21.setRndRate(2.0234);
        decaydiff21.setDiff(.5);
        decaydiff22.init(_sr, findNextPrime(2657 * _scale) / _sr, 0.0015);
        decaydiff22.setRndRate(1.07453);
        decaydiff22.setDiff(.5);

        /*
        decaydiff11.init(_sr, 672 * _scale, _sr * 0.0017, 1., 0, 1300 * _scale, -.7);
        decaydiff12.init(_sr, 908 * _scale, _sr * 0.0017, 1., .25, 1300 * _scale, -.7);
        decaydiff21.init(_sr, 1800 * _scale, _sr * 0.0017, 1., .5, 3000 * _scale, .5);
        decaydiff22.init(_sr, 2656 * _scale, _sr * 0.0017, 1., .75, 3000 * _scale, .5);
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

    void compute(T *inl, T *inr, T *outl, T *outr, int s) {
        check();
        T mix = _mix->load();
        const T mixsrc = 1. - mix;
        mix *= .6;
        const T gain = LOG2NORMALF(_gain->load());
        const T decay = .6 + _decay->load() * .399;
        for (int i = 0; i < s; i++) {
            // outl[i] = outr[i] = decaydiff11.tick((inl[i] + inr[i]) * .5f);

            T pre = inputdiff22.process(inputdiff21.process(inputdiff12.process(
                    inputdiff11.process(
                            prelp.tick(predelay.tick((inl[i] + inr[i]) * .5f))))));
            T stateL = dcBlocker1.process(
                    delay2.readwrite(decaydiff21.tickok(decay * damping1.tick(
                            delay1.readwrite(
                                    decaydiff11.tickok(
                                            decay * _stateR +
                                            pre))))));
            T stateR = dcBlocker2.process(
                    delay4.readwrite(decaydiff22.tickok(decay * damping2.tick(
                            delay3.readwrite(
                                    decaydiff12.tickok(
                                            decay * _stateL +
                                            pre))))));
            _stateL = stateL;
            _stateR = stateR;


            T left = delay3.tap(tapl1);
            left += delay3.tap(tapl2);
            left -= decaydiff22.tap(tapl3);
            left += delay4.tap(tapl4);
            left -= delay1.tap(tapl5);
            left -= decaydiff21.tap(tapl6);
            left -= delay2.tap(tapl7);
            T right = delay1.tap(tapr1);
            right += delay1.tap(tapr2);
            right -= decaydiff21.tap(tapr3);
            right += delay2.tap(tapr4);
            right -= delay3.tap(tapr5);
            right -= decaydiff22.tap(tapr6);
            right -= delay4.tap(tapr7);
            outl[i] = (left * mix + inl[i] * mixsrc) * gain;
            outr[i] = (right * mix + inr[i] * mixsrc) * gain;
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
            predelay.setDelay(_predelayprev * _sr * 0.001);
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
    DCBlocker<T> dcBlocker1, dcBlocker2;
    SimpleDelay2<T> predelay;
    ReverbButter1<T> prelp;
    Ap<T> inputdiff11, inputdiff12, inputdiff21, inputdiff22;
    Delay<T> delay1, delay2, delay3, delay4;
    Ap1<T> decaydiff11, decaydiff12;
    Ap1<T> decaydiff21, decaydiff22;
    ReverbButter1<T> damping1, damping2;
    T _stateL{}, _stateR{};
    T _sr{48000}, _scale{}, _totaldelay{}, _loopdecay{.5};
    const T _dattorroSampleRate = 29761.0;
    std::atomic<T> *_mix, *_gain, *_decay, *_damp, *_predelay, *_lpcut, *_hpcut;
    T _olddamp, _olddec{-1000000}, _predelayprev, _lpcutold, _hpcutold;
    int tapl1;
    int tapl2;
    int tapl3;
    int tapl4;
    int tapl5;
    int tapl6;
    int tapl7;
    int tapr1;
    int tapr2;
    int tapr3;
    int tapr4;
    int tapr5;
    int tapr6;
    int tapr7;
};


#endif //GRAINSTORM_REVERBPROGENITOR_H
