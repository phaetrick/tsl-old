//
// Created by pr on 01.11.19.
//

#include "complex_multiply.h"

.text
.balign 4
.syntax unified
.global arm_neon_complex_multiply_float
.global arm_neon_complex_multiply_short
.global arm_neon_complex_multiply_accumulate
.global arm_neon_complex_multiply_int
.global arm_neon_fft_mag
.global arm_neon_fft_psd
.global arm_neon_fft_abs

.macro preserve_caller_vectors
  vpush {d8-d9}
  vpush {d10-d11}
  vpush {d12-d13}
  vpush {d14-d15}
.endm// restore_caller_vectors(): Restore first 64-bits from v8v-15 on stack (sp)
.macro restore_caller_vectors
  vpop {d14-d15}
  vpop {d12-d13}
  vpop {d10-d11}
  vpop {d8-d9}
.endm



@ NOTE: COUNT SHOULD BE A MULTIPLE OF 8!



@
@ void arm_neon_complex_multiply_int(int* result, int* x, int* y, unsigned int count);
@
@ Inspired by: http:

arm_neon_complex_multiply_int:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.loop_mul_i_begin:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d24-d27}, [r2]!    @ q12 = y[0..3].r, q13 = y[0..3].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d28-d31}, [r2]!    @ q14 = y[4..7].r, q15 = y[4..7].i

vmul.s32    q0, q8, q12         @ q0 = x[0..3].r * y[0..3].r
vmul.s32    q1, q9, q12         @ q1 = x[0..3].i * y[0..3].r
vmul.s32    q2, q10, q14        @ q2 = x[4..7].r * y[4..7].r
vmul.s32    q3, q11, q14        @ q3 = x[4..7].i * y[4..7].r

vmls.s32    q0, q9, q13         @ q0 = q0 - x[0..3].i * y[0..3].i
vmla.s32    q1, q8, q13         @ q1 = q1 + x[0..3].r * y[0..3].i
vmls.s32    q2, q11, q15        @ q2 = q2 - x[4..7].i * y[4..7].i
vmla.s32    q3, q10, q15        @ q3 = q3 + x[4..7].r * y[4..7].i

vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]

subs        r3, r3, #8          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_mul_i_begin     @ If result != 0, still elements to process

bx          lr                  @ Return from function

@
@ void arm_neon_complex_multiply_short(int* result, short* x, short* y, unsigned int count);
@
@ Inspired by: http://a-hackers-craic.blogspot.be/2012/10/neon-complex-multiply.html

arm_neon_complex_multiply_short:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.loop_mul_s_begin:
vld2.16     {d16-d19}, [r1]!    @ d16-17 = x[0..7].r, d18-19 = x[0..7].i
vld2.16     {d20-d23}, [r2]!    @ d20-21 = y[0..7].r, d22-23 = y[0..7].i
vld2.16     {d24-d27}, [r1]!    @ d24-25 = x[8..15].r, d26-27 = x[8..15].i
vld2.16     {d28-d31}, [r2]!    @ d28-29 = y[8..15].r, d30-31 = y[8..15].i

vmull.s16    q0, d16, d20         @ q0 = x[0..3].r * y[0..3].r
vmull.s16    q1, d18, d20         @ q1 = x[0..3].i * y[0..3].r
vmull.s16    q2, d17, d21         @ q0 = x[0..3].r * y[0..3].r
vmull.s16    q3, d19, d21         @ q1 = x[0..3].i * y[0..3].r

vmull.s16    q4, d24, d28         @ q0 = x[0..3].r * y[0..3].r
vmull.s16    q5, d26, d28         @ q1 = x[0..3].i * y[0..3].r
vmull.s16    q6, d25, d29         @ q0 = x[0..3].r * y[0..3].r
vmull.s16    q7, d17, d29         @ q1 = x[0..3].i * y[0..3].r

vmlsl.s16    q0, d18, d22         @ q0 = q0 - x[0..3].i * y[0..3].i
vmlal.s16    q1, d16, d22         @ q1 = q1 + x[0..3].r * y[0..3].i
vmlsl.s16    q2, d19, d23         @ q0 = q0 - x[0..3].i * y[0..3].i
vmlal.s16    q3, d17, d23         @ q1 = q1 + x[0..3].r * y[0..3].i

vmlsl.s16    q4, d26, d30         @ q0 = q0 - x[0..3].i * y[0..3].i
vmlal.s16    q5, d24, d30         @ q1 = q1 + x[0..3].r * y[0..3].i
vmlsl.s16    q6, d27, d31         @ q0 = q0 - x[0..3].i * y[0..3].i
vmlal.s16    q7, d24, d31         @ q1 = q1 + x[0..3].r * y[0..3].i


vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]
vst2.32     {d8-d11}, [r0]!      @ Store res[0..3]
vst2.32     {d12-d15}, [r0]!      @ Store res[4..7]

subs        r3, r3, #16          @ Decrement count (16 complex multiplications per iteration)
bne         .loop_mul_s_begin     @ If result != 0, still elements to process

bx          lr                  @ Return from function

@
@ void arm_neon_complex_multiply_float(float* result, float* x, float* y, unsigned int count);
@

arm_neon_complex_multiply_float:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.loop_mul_f_begin:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d24-d27}, [r2]!    @ q12 = y[0..3].r, q13 = y[0..3].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d28-d31}, [r2]!    @ q14 = y[4..7].r, q15 = y[4..7].i

vmul.f32    q0, q8, q12         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q1, q9, q12         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q2, q10, q14        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q3, q11, q14        @ q3 = x[4..7].i * y[4..7].r

vmls.f32    q0, q9, q13         @ q0 = q0 - x[0..3].i * y[0..3].i
vmla.f32    q1, q8, q13         @ q1 = q1 + x[0..3].r * y[0..3].i
vmls.f32    q2, q11, q15        @ q2 = q2 - x[4..7].i * y[4..7].i
vmla.f32    q3, q10, q15        @ q3 = q3 + x[4..7].r * y[4..7].i

@ Thus:
@ q0 = res[0..3].r
@ q1 = res[0..3].i
@ q2 = res[4..7].r
@ q3 = res[4..7].i

vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]

subs        r3, r3, #8          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_mul_f_begin     @ If result != 0, still elements to process

bx          lr                  @ Return from function


@
@ void arm_neon_complex_multiply_accumulate(float* result, float* accum, float* x, float* y, unsigned int count);
@
arm_neon_complex_multiply_accumulate:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: accum
@ r2: x
@ r3: y
@ [sp] : count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

push        {r4}                @ Save r4 (callee save)
ldr         r4, [sp, #4]        @ Load count in r4

.loop_mac_begin:
vld2.32     {d0-d3}, [r1]!      @ q0 = accum[0..3].r, q1 = accum[0..3].i
vld2.32     {d4-d7}, [r1]!      @ q2 = accum[4..7].r, q3 = accum[4..7].i

vld2.32     {d16-d19}, [r2]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d24-d27}, [r3]!    @ q12 = y[0..3].r, q13 = y[0..3].i
vld2.32     {d20-d23}, [r2]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d28-d31}, [r3]!    @ q14 = y[4..7].r, q15 = y[4..7].i

vmla.f32    q0, q8, q12         @ q0 = q0 + x[0..3].r * y[0..3].r
vmla.f32    q1, q9, q12         @ q1 = q1 + x[0..3].i * y[0..3].r
vmla.f32    q2, q10, q14        @ q2 = q2 + x[4..7].r * y[4..7].r
vmla.f32    q3, q11, q14        @ q3 = q3 + x[4..7].i * y[4..7].r

vmls.f32    q0, q9, q13         @ q0 = q0 - x[0..3].i * y[0..3].i
vmla.f32    q1, q8, q13         @ q1 = q1 + x[0..3].r * y[0..3].i
vmls.f32    q2, q11, q15        @ q2 = q2 - x[4..7].i * y[4..7].i
vmla.f32    q3, q10, q15        @ q3 = q3 + x[4..7].r * y[4..7].i

@ Thus:
@ q0 = res[0..3].r
@ q1 = res[0..3].i
@ q2 = res[4..7].r
@ q3 = res[4..7].i

vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]

subs        r4, r4, #8          @ Decrement count (8 complex MAC's per iteration)
bne         .loop_mac_begin     @ If result != 0, still elements to process

pop         {r4}                @ Restore r4
bx          lr                  @ Return from function


@
@ void arm_neon_fft_mag(float* result, float* x, unsigned int count);
@
@ Inspired by: http://a-hackers-craic.blogspot.be/2012/10/neon-complex-multiply.html

arm_neon_fft_mag:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

preserve_caller_vectors

vsub.f32 q1, q1, q1          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q3,q3,q3          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q5,q5,q5          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q7,q7,q7          @ XOR 2 pairs of values (0<-2, 1<-3)


.loop_fft_mag:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d24-d27}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d28-d31}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i

vmul.f32    q8, q8, q8         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q9, q9, q9         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q10, q10, q10        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q11, q11, q11        @ q3 = x[4..7].i * y[4..7].r

vmul.f32    q12, q12, q12         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q13, q13, q13         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q14, q14, q14        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q15, q15, q15        @ q3 = x[4..7].i * y[4..7].r

vadd.f32    q0, q8, q9         @ q0 = x[0..3].r * y[0..3].r
vadd.f32    q2, q10, q11         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q4, q12, q13         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q6, q14, q15         @ q1 = x[0..3].i * y[0..3].r

vrsqrte.f32 q0, q0               @ sqrt
vrsqrte.f32 q2, q2
vrsqrte.f32 q4, q4
vrsqrte.f32 q6, q6

vrecpe.f32    q0, q0         @ sqrt2
vrecpe.f32    q2, q2         @
vrecpe.f32    q4, q4        @
vrecpe.f32    q6, q6        @



vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]
vst2.32     {d8-d11}, [r0]!      @ Store res[0..3]
vst2.32     {d12-d15}, [r0]!      @ Store res[4..7]
subs        r2, r2, #32          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_fft_mag     @ If result != 0, still elements to process

restore_caller_vectors
bx          lr                  @ Return from function


@
@ void arm_neon_fft_psd(float* result, float* x, unsigned int count);
@
@ Inspired by: http://a-hackers-craic.blogspot.be/2012/10/neon-complex-multiply.html

arm_neon_fft_psd:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

preserve_caller_vectors

vsub.f32 q1, q1, q1          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q3,q3,q3          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q5,q5,q5          @ XOR 2 pairs of values (0<-2, 1<-3)
vsub.f32 q7,q7,q7          @ XOR 2 pairs of values (0<-2, 1<-3)


.loop_fft_psd:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d24-d27}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d28-d31}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i

vmul.f32    q8, q8, q8         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q9, q9, q9         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q10, q10, q10        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q11, q11, q11        @ q3 = x[4..7].i * y[4..7].r

vmul.f32    q12, q12, q12         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q13, q13, q13         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q14, q14, q14        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q15, q15, q15        @ q3 = x[4..7].i * y[4..7].r

vadd.f32    q0, q8, q9         @ q0 = x[0..3].r * y[0..3].r
vadd.f32    q2, q10, q11         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q4, q12, q13         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q6, q14, q15         @ q1 = x[0..3].i * y[0..3].r

vrsqrte.f32 q0, q0               @ sqrt
vrsqrte.f32 q2, q2
vrsqrte.f32 q4, q4
vrsqrte.f32 q6, q6

vrecpe.f32    q0, q0         @ sqrt2
vrecpe.f32    q2, q2         @
vrecpe.f32    q4, q4        @
vrecpe.f32    q6, q6        @

vmul.f32    q0, q0, q0         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q2, q2, q2         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q4, q4, q4        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q6, q6, q6        @ q3 = x[4..7].i * y[4..7].r


vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]
vst2.32     {d8-d11}, [r0]!      @ Store res[0..3]
vst2.32     {d12-d15}, [r0]!      @ Store res[4..7]
subs        r2, r2, #32          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_fft_psd     @ If result != 0, still elements to process

restore_caller_vectors
bx          lr                  @ Return from function


@
@ void arm_neon_fft_abs(float* result, float* x, unsigned int count);
@
@ Inspired by: http://a-hackers-craic.blogspot.be/2012/10/neon-complex-multiply.html

arm_neon_fft_abs:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

preserve_caller_vectors

veor.u32 q1, q1, q1          @ XOR 2 pairs of values (0<-2, 1<-3)
veor.u32 q3,q3,q3          @ XOR 2 pairs of values (0<-2, 1<-3)
veor.u32 q5,q5,q5          @ XOR 2 pairs of values (0<-2, 1<-3)
veor.u32 q7,q7,q7          @ XOR 2 pairs of values (0<-2, 1<-3)


.loop_fft_abs:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i
vld2.32     {d24-d27}, [r1]!    @ q8 = x[0..3].r, q9 = x[0..3].i
vld2.32     {d28-d31}, [r1]!    @ q10 = x[4..7].r, q11 = x[4..7].i

vmul.f32    q8, q8, q8         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q9, q9, q9         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q10, q10, q10        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q11, q11, q11        @ q3 = x[4..7].i * y[4..7].r

vmul.f32    q12, q12, q12         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q13, q13, q13         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q14, q14, q14        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q15, q15, q15        @ q3 = x[4..7].i * y[4..7].r

vadd.f32    q0, q8, q9         @ q0 = x[0..3].r * y[0..3].r
vadd.f32    q2, q10, q11         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q4, q12, q13         @ q1 = x[0..3].i * y[0..3].r
vadd.f32    q6, q14, q15         @ q1 = x[0..3].i * y[0..3].r

vst2.32     {d0-d3}, [r0]!      @ Store res[0..3]
vst2.32     {d4-d7}, [r0]!      @ Store res[4..7]
vst2.32     {d8-d11}, [r0]!      @ Store res[0..3]
vst2.32     {d12-d15}, [r0]!      @ Store res[4..7]
subs        r2, r2, #32          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_fft_abs     @ If result != 0, still elements to process

restore_caller_vectors
bx          lr                  @ Return from function
