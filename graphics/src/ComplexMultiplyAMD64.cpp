#include <iostream>
#include <immintrin.h>
//#include <emmintrin.h> // Include SSE intrinsics header

// Function to perform SIMD-based complex number multiplication on interleaved arrays
void complexMultiplyFloat(float* result, float* a, float* b, uint32_t size) {
	// Ensure size is a multiple of 4 (to process 4 complex numbers at a time with SSE)
	int num_simd_iterations = size / 8;

	for (int i = 0; i < num_simd_iterations; ++i) {
		// Load 4 complex numbers (interleaved) from arrays 'a' and 'b' into SSE registers
		__m128 xmm_a = _mm_loadu_ps(&a[8 * i]); // Load 8 floats (4 complex numbers) from 'a' into xmm_a
		__m128 xmm_b = _mm_loadu_ps(&b[8 * i]); // Load 8 floats (4 complex numbers) from 'b' into xmm_b

		// Split the interleaved data into real and imaginary parts
		__m128 xmm_a_real = _mm_shuffle_ps(xmm_a, xmm_a, _MM_SHUFFLE(2, 0, 2, 0)); // Real parts of a
		__m128 xmm_a_imag = _mm_shuffle_ps(xmm_a, xmm_a, _MM_SHUFFLE(3, 1, 3, 1)); // Imaginary parts of a
		__m128 xmm_b_real = _mm_shuffle_ps(xmm_b, xmm_b, _MM_SHUFFLE(2, 0, 2, 0)); // Real parts of b
		__m128 xmm_b_imag = _mm_shuffle_ps(xmm_b, xmm_b, _MM_SHUFFLE(3, 1, 3, 1)); // Imaginary parts of b

		// Multiply complex numbers using SIMD instructions
		// a * b = (a.real*b.real - a.imag*b.imag) + i*(a.real*b.imag + a.imag*b.real)
		__m128 xmm_result_real = _mm_sub_ps(_mm_mul_ps(xmm_a_real, xmm_b_real), _mm_mul_ps(xmm_a_imag, xmm_b_imag)); // Real parts
		__m128 xmm_result_imag = _mm_add_ps(_mm_mul_ps(xmm_a_real, xmm_b_imag), _mm_mul_ps(xmm_a_imag, xmm_b_real)); // Imaginary parts

		// Store the result back to the 'result' array (interleaved)
		_mm_storeu_ps(&result[8 * i], xmm_result_real); // Store real parts
		_mm_storeu_ps(&result[8 * i + 4], xmm_result_imag); // Store imaginary parts
	}

	// Process any remaining elements (less than 4) using scalar operations
	for (int i = num_simd_iterations * 8; i < size; i += 2) {
		// Calculate index for interleaved array access
		// Perform complex number multiplication (a.real*b.real - a.imag*b.imag) + i*(a.real*b.imag + a.imag*b.real)
		result[i] = a[i] * b[i] - a[i + 1] * b[i + 1]; // Real part
		result[i + 1] = a[i] * b[i + 1] + a[i + 1] * b[i]; // Imaginary part
	}
}

// Function to perform SIMD-based complex number multiplication on interleaved arrays (double precision)
void complexMultiplyDouble(double* result, double* a, double* b, uint32_t size) {
	int numPairs = size / 2; // Calculate the number of complex number pairs (each pair requires 2 doubles)

	for (int i = 0; i < numPairs; ++i) {
		// Load complex number pairs from arrays 'a' and 'b' into SSE registers
		__m128d a_real_imag = _mm_loadu_pd(&a[2 * i * 2]); // Load 2 complex number pairs (4 doubles) from array 'a'
		__m128d b_real_imag = _mm_loadu_pd(&b[2 * i * 2]); // Load 2 complex number pairs (4 doubles) from array 'b'

		// Unpack complex number parts
		__m128d a_real = a_real_imag; // a[i].real in both parts of xmm register
		__m128d a_imag = _mm_shuffle_pd(a_real_imag, a_real_imag, 0b01); // a[i].imag in both parts of xmm register
		__m128d b_real = b_real_imag; // b[i].real in both parts of xmm register
		__m128d b_imag = _mm_shuffle_pd(b_real_imag, b_real_imag, 0b01); // b[i].imag in both parts of xmm register

		// Perform complex number multiplication
		__m128d result_real = _mm_sub_pd(_mm_mul_pd(a_real, b_real), _mm_mul_pd(a_imag, b_imag)); // (a[i].real * b[i].real) - (a[i].imag * b[i].imag)
		__m128d result_imag = _mm_add_pd(_mm_mul_pd(a_real, b_imag), _mm_mul_pd(a_imag, b_real)); // (a[i].real * b[i].imag) + (a[i].imag * b[i].real)

		// Store the result back to memory
		_mm_storeu_pd(&result[2 * i * 2], result_real); // Store the real part of the result
		_mm_storeu_pd(&result[2 * i * 2 + 2], result_imag); // Store the imaginary part of the result
	}
}
// Function to perform complex number multiplication followed by accumulation using SIMD (SSE2) intrinsics
void complexMultiplyAccumDouble(double *result, double* c, double* a, double* b, uint32_t size) {
	int numPairs = size / 2; // Calculate the number of complex number pairs

	for (int i = 0; i < numPairs; ++i) {
		// Load complex number pairs from arrays 'a' and 'b' into SSE registers
		__m128d a_real_imag = _mm_loadu_pd(&a[2 * i]); // Load a[i].real and a[i].imag into the lower half of a xmm register
		__m128d b_real_imag = _mm_loadu_pd(&b[2 * i]); // Load b[i].real and b[i].imag into the lower half of another xmm register

		// Unpack and shuffle complex number parts for multiplication
		__m128d a_real = a_real_imag;               // a[i].real in both parts of xmm register
		__m128d a_imag = _mm_shuffle_pd(a_real_imag, a_real_imag, 1); // a[i].imag in both parts of xmm register
		__m128d b_real = b_real_imag;               // b[i].real in both parts of xmm register
		__m128d b_imag = _mm_shuffle_pd(b_real_imag, b_real_imag, 1); // b[i].imag in both parts of xmm register

		// Perform complex number multiplication
		__m128d result_real = _mm_sub_pd(_mm_mul_pd(a_real, b_real), _mm_mul_pd(a_imag, b_imag)); // (a[i].real * b[i].real) - (a[i].imag * b[i].imag)
		__m128d result_imag = _mm_add_pd(_mm_mul_pd(a_real, b_imag), _mm_mul_pd(a_imag, b_real)); // (a[i].real * b[i].imag) + (a[i].imag * b[i].real)

		// Load current complex number 'c' from memory
		__m128d current_c = _mm_loadu_pd(&c[2 * i]); // Load c[i].real and c[i].imag into the lower half of a xmm register

		// Accumulate the result into 'c'
		__m128d updated_c_real = _mm_add_pd(current_c, result_real); // c[i].real + (a[i].real * b[i].real - a[i].imag * b[i].imag)
		__m128d updated_c_imag = _mm_add_pd(_mm_loadu_pd(&c[2 * i + 2]), result_imag); // c[i].imag + (a[i].real * b[i].imag + a[i].imag * b[i].real)

		// Store the updated complex number 'c' back to memory
		_mm_storeu_pd(&c[2 * i], updated_c_real); // Store the updated real part
		_mm_storeu_pd(&c[2 * i + 2], updated_c_imag); // Store the updated imaginary part
	}
}

