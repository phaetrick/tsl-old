//
// Created by pr on 15.08.20.
//

#include "correlation.h"
#include "logger.h"
#include "vocoder.h"

void compute_welch_window(float *w_data, int32_t s, float *inverse) {
    int32_t i;
    float w;
    float c;

    const int32_t len = s;
    const int32_t n2 = (len >> 1);
    c = 2.0f / (len - 1.0f);

    if (len & 1) {
        for (i = 0; i < n2; i++) {
            w = c - i - 1.0f;
            w = 1.0f - (w * w);
            w_data[i] = w;
            w_data[len - 1 - i] = w;

            if (inverse) {
                inverse[i] = (1.f - w) / s;
                inverse[len - 1 - i] = (1.f - w) / s;
            }
        }
        return;
    }

    w_data += n2;
    for (i = 0; i < n2; i++) {
        w = c - n2 + i;
        w = 1.0f - (w * w);
        w_data[-i - 1] = w;
        w_data[+i] = w;

        if (inverse) {
            inverse[-i - 1] = (1.f - w) / s;
            inverse[+i] = (1.f - w) / s;
        }
    }
}

