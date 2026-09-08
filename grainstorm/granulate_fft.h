#pragma once
#include <cstdint>
class TRACK;
void compute_fft(TRACK* track, int32_t channel, const bool isMultiThreaded);
