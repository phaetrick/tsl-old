//
// Created by pr on 27.03.26.
//

#pragma once
#include "base.h"
#include <limits>

namespace tsl{
    static constexpr int DEGRADATION_STEREO_ID = std::numeric_limits<int>::max();

    class degradation : public Effect {
    public:
        degradation(TRACK *t) : Effect(t, DEGRADATION_STEREO_ID, STEREOEFFECT) {
        };

        void
        compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t size) override;
    };

}