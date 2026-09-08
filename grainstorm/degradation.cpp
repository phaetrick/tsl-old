//
// Created by pr on 27.03.26.
//

#include <player.h>
#include "degradation.h"

void
tsl::degradation::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t size) {
    MYFLOAT mix = 1.0, gain = 1.0;
    double step = 0.5;
    double dc_offset = 0.05;
    for (int i = 0; i < size; i++) {
        // Synthesis: Calculate the clean samples first
        double left_clean = inl[i];
        double right_clean = inr[i];

        // Apply Bit-Crushing (quantization) to the double precision samples
        // This effectively "downgrades" the signal quality
        double left_poisoned = round(left_clean / step) * step;
        double right_poisoned = round(right_clean / step) * step;

        // Apply DC Offset (adds tension/strain to the output)
        left_poisoned += dc_offset;
        right_poisoned += dc_offset;

        outl[i] = inl[i] * (1.f - _smooth1) + _smooth1 * _smooth2 * left_poisoned;
        outr[i] = inr[i] * (1.f - _smooth1) + _smooth1 * _smooth2 * right_poisoned;
        smmixgain(mix, gain);

    }

}