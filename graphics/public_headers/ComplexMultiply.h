#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

extern void complexMultiplyDouble(double* result, double* x, double* y, uint32_t count);
extern void complexMultiplyFloat(float* result, float* x, float* y, uint32_t count);
extern void complexMultiplyAccumFloat(float* result, float* accum, float* x, float* y, uint32_t count);
extern void complexMultiplyAccumDouble(double* result, double* accum, double* x, double* y, uint32_t count);

#ifdef __cplusplus
}
#endif