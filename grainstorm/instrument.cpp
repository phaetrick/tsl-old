//
// Created by pr on 25.02.20.
//

#include "instrument.h"


static float *sinetable = nullptr;

float *getSine() {
    if (sinetable == nullptr) {
        sinetable = (float *) calloc(1, sizeof(float) * (WINDOW_SIZE + 1));
        for (int32_t j = 0; j < WINDOW_SIZE; j++) {
            sinetable[j] = (float) sin(TWOPI_P * j / (double) WINDOW_SIZE);
        }
    }
    return sinetable;
}
