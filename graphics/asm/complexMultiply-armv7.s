//
// Created by pr on 01.11.19.
//


.text
.thumb_func
.fpu neon
.balign 4
.syntax unified
.global complexMultiplyDouble
.global complexMultiplyFloat
.global complexMultiplyAccumDouble

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


@
@ void complexMultiplyFloat(float* result, float* x, float* y, uint32_t count);
@

complexMultiplyFloat:
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

complexMultiplyDouble:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: result
@ r1: x
@ r2: y
@ r3: count
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.loop_mul_d_begin:
vld2.32     {d16-d19}, [r1]!    @ q8 = x[0,1].r, q9 = x[0,1].i
vld2.32     {d24-d27}, [r2]!    @ q12 = y[0,1].r, q13 = y[0,1].i
vld2.32     {d20-d23}, [r1]!    @ q10 = x[2,3].r, q11 = x[2,3].i
vld2.32     {d28-d31}, [r2]!    @ q14 = y[2,3].r, q15 = y[2,3].i

vmul.f32    q0, q8, q12         @ q0 = x[0,1].r * y[0,1].r
vmul.f32    q1, q9, q12         @ q1 = x[0,1].i * y[0,1].r
vmul.f32    q2, q10, q14        @ q2 = x[2,3].r * y[2,3].r
vmul.f32    q3, q11, q14        @ q3 = x[2,3].i * y[2,3].r

vmls.f32    q0, q9, q13         @ q0 = q0 - x[0,1].i * y[0,1].i
vmla.f32    q1, q8, q13         @ q1 = q1 + x[0,1].r * y[0,1].i
vmls.f32    q2, q11, q15        @ q2 = q2 - x[2,3].i * y[2,3].i
vmla.f32    q3, q10, q15        @ q3 = q3 + x[2,3].r * y[2,3].i

@ Thus:
@ q0 = res[0,1].r
@ q1 = res[0,1].i
@ q2 = res[2,3].r
@ q3 = res[2,3].i

vst2.32     {d0-d3}, [r0]!      @ Store res[0,1]
vst2.32     {d4-d7}, [r0]!      @ Store res[2,3]

subs        r3, r3, #4          @ Decrement count (4 complex multiplications per iteration)
bne         .loop_mul_d_begin     @ If result != 0, still elements to process

bx          lr                  @ Return from function

@
@ void complexMultiplyAccumDouble(float* result, float* accum, float* x, float* y, uint32_t count);
@
complexMultiplyAccumDouble:
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
vld2.32     {d0-d3}, [r1]!      @ q0 = accum[0,1].r, q1 = accum[0,1].i
vld2.32     {d4-d7}, [r1]!      @ q2 = accum[2,3].r, q3 = accum[2,3].i

vld2.32     {d16-d19}, [r2]!    @ q8 = x[0,1].r, q9 = x[0,1].i
vld2.32     {d24-d27}, [r3]!    @ q12 = y[0,1].r, q13 = y[0,1].i
vld2.32     {d20-d23}, [r2]!    @ q10 = x[2,3].r, q11 = x[2,3].i
vld2.32     {d28-d31}, [r3]!    @ q14 = y[2,3].r, q15 = y[2,3].i

vmla.f32    q0, q8, q12         @ q0 = q0 + x[0,1].r * y[0,1].r
vmla.f32    q1, q9, q12         @ q1 = q1 + x[0,1].i * y[0,1].r
vmla.f32    q2, q10, q14        @ q2 = q2 + x[2,3].r * y[2,3].r
vmla.f32    q3, q11, q14        @ q3 = q3 + x[2,3].i * y[2,3].r

vmls.f32    q0, q9, q13         @ q0 = q0 - x[0,1].i * y[0,1].i
vmla.f32    q1, q8, q13         @ q1 = q1 + x[0,1].r * y[0,1].i
vmls.f32    q2, q11, q15        @ q2 = q2 - x[2,3].i * y[2,3].i
vmla.f32    q3, q10, q15        @ q3 = q3 + x[2,3].r * y[2,3].i

@ Thus:
@ q0 = res[0,1].r
@ q1 = res[0,1].i
@ q2 = res[2,3].r
@ q3 = res[2,3].i

vst2.32     {d0-d3}, [r0]!      @ Store res[0,1]
vst2.32     {d4-d7}, [r0]!      @ Store res[2,3]

subs        r4, r4, #4          @ Decrement count (4 complex MAC's per iteration)
bne         .loop_mac_begin     @ If result != 0, still elements to process

pop         {r4}                @ Restore r4
bx          lr                  @ Return from function