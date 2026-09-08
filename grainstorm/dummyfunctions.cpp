//
// Created by pr on 08.08.22.
//
#ifdef __cplusplus
extern "C" {
#endif

void phasorfilter2(float in, float **array, uint32_t M){};
void phasorfilterhold(float in, float **array, uint32_t M){};
void arm_neon_complex_multiply_float(float* result, float* x, float* y, uint32_t count){};

void arm_neon_complex_multiply_short(int* result, short* x, short* y, uint32_t count){};
void arm_neon_complex_multiply_accumulate(float* result, float* accum, float* x, float* y, uint32_t count){};


void arm_neon_complex_multiply_int(int* result, int* x, int* y, uint32_t count){};

void arm_neon_fft_mag(float* result, float* x, uint32_t count){};

void arm_neon_fft_psd(float* result, float* x, uint32_t count){};
void arm_neon_fft_abs(float* result, float* x, uint32_t count){};
#ifdef __cplusplus
}
#endif
