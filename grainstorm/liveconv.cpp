//
// Created by pr on 15.05.23.
//

#include "liveconv.h"

void LiveConvolver::compute(MYFLOAT *in, int32_t size) {
    const auto csize = (int) (_STATE->params[_track->index][LIVECONVSIZE].load() * _STATE->sr);
    auto mod = _DATA->tracks[(int)_STATE->params[_track->index][LIVECONVUPDATE].load()]->envf_buffer[_chan];
    auto it = _modbuf.begin() + _cnt2;
    const auto dec = _STATE->params[_track->index][LIVECONVDECAY].load() * .99;

    for (int32_t i = 0; i < size; i++) {
        *it = mod[i];
        if (++_cnt >= csize || _oldsize != csize) {
            if (true) {
                auto a = LOG2NORMAL(_STATE->params[_track->index][LIVECONV1].load()), b = LOG2NORMAL(
                        _STATE->params[_track->index][LIVECONV2].load());
                if (a > b)std::swap(a, b);
                Butterworth<double> butterworth{_appState};
                butterworth.Reset();
                butterworth.SetHp(a);
                butterworth.SetLp(b);
                auto it2 = it - csize;
                if (it2 < _modbuf.begin())
                    it2 += _modbuf.size();
                for (int32_t z = 0; z < csize; z++) {
                    auto l = z /(double) csize;
                    _tmpbuf[z] = butterworth.tickLpHp6(*it2) * pow(1-dec,l);
                    if (++it2 == _modbuf.end())
                        it2 = _modbuf.begin();
                }
                convolverNonUniform.loadIR(_tmpbuf.begin(), _tmpbuf.begin() + csize);
            }
            _cnt = 0;
            _oldsize = csize;
        }
        if (++it == _modbuf.end()) {
            _cnt2 = 0;
            it = _modbuf.begin();
        } else ++_cnt2;
    }

    MYFLOAT wet, dry;
    if (_track->bypass[SPACE_LIVECONV].load() || destroyRequested) {
        wet = 0.f;
        dry = 1.f;
    } else {
        dry = LOG2NORMAL(
                _STATE->params[_track->index][LIVECONVDRY].load());
    wet = LOG2NORMAL(_STATE->params[_track->index][LIVECONVWET].load());    }

    for (int32_t i = 0; i < size; i++) {
        in[i] = in[i] * _smooth2 + convolverNonUniform.tick(in[i]) * _smooth1;
        sm1(wet);
        sm2(dry);
    }


}
