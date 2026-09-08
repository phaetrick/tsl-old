//
// Created by pr on 06.11.21.
//
.text
.thumb_func
.fpu neon
.balign 4
.syntax unified
.global phasorfilter2
.global phasorfilter3
.global phasorfilter
.global phasorfilterhold
.global test

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

phasorfilter:

// r0: *x
//r1: size
// r2: pointers
// r3: M
push {r4-r11,lr}        // 6 regs are pushed
                       // SP is decremented by 6*4 = 24 bytes
preserve_caller_vectors

ldr r10, [r0]
ldr r11, [r0, #4]

vdup.32 q14,  r3
vrecpe.f32 q14, q14
vmov.f32 q15, #.5


.outerloop:

//LOAD y
ldr r4, [r10]
vdup.32 q4,  r4
ldr r4, [r11]
vdup.32 q5,  r4
vadd.f32 q4, q4, q5
vmul.f32 q4, q4, q15

//vst1.32 d8[0], [r10]!
//vst1.32 d8[0], [r11]!


ldr r4, [r2]     //_pre get array_pointers[0] into r4
ldr r5, [r2, #4] // ym_prev get array_pointers[1] into r5
ldr r6, [r2, #8] // aa get array_pointers[2] into r6
ldr r7, [r2, #12] // post get array_pointers[2] into r7

mov r8, r5
mov r9, r3

vmov.f32 q12, #0.
vmov.f32 q13, #0.

//veor q12, q12
//veor q13, q13

.innerloop:
vld2.32     {q0-q1}, [r4]!    // q0 = x[0..3].r, q1 = x[0..3].i
vld2.32     {q2-q3}, [r4]!    // q2 = x[4..7].r, q3 = x[4..7].i
vmul.f32    q0, q0, q4       // q0 = x[0..3].r * y[0..3].r
vmul.f32    q1, q1, q4       // q1 = x[0..3].i * y[0..3].r
vmul.f32    q2, q2, q4       // q2 = x[4..7].r * y[4..7].r
vmul.f32    q3, q3, q4       // q3 = x[4..7].i * y[4..7].r


vld2.32     {q4-q5}, [r5]!    // q4 = y[0..3].r, q5 = y[0..3].i
vld2.32     {q6-q7}, [r5]!    // q6 = y[4..7].r, q7 = y[4..7].i

vld1.32     {q8-q9}, [r6]!    // q8 = damp[0..3], q9 = damp[4..7]

vmla.f32    q0, q4, q8         // q0 = x[0..3].r * y[0..3].r
vmla.f32    q1, q5, q8         // q1 = x[0..3].i * y[0..3].r
vmla.f32    q2, q6, q9        // q2 = x[4..7].r * y[4..7].r
vmla.f32    q3, q7, q9        // q3 = x[4..7].i * y[4..7].r

vld2.32     {q4-q5}, [r7]!    @ q4 = y[0..3].r, q5 = y[0..3].i
vld2.32     {q6-q7}, [r7]!    @ q6 = y[4..7].r, q7 = y[4..7].i


vmul.f32    q8, q0, q4         @ q8 = x[0..3].r * y[0..3].r
vmul.f32    q9, q1, q4         @ q9 = x[0..3].i * y[0..3].r
vmul.f32    q10, q2, q6        @ q10 = x[4..7].r * y[4..7].r
vmul.f32    q11, q3, q6        @ q11 = x[4..7].i * y[4..7].r

vmls.f32    q8, q1, q5         @ q8 = q8 - x[0..3].i * y[0..3].i
vmla.f32    q9, q0, q5         @ q9 = q9 + x[0..3].r * y[0..3].i
vmls.f32    q10, q3, q7        @ q10 = q10 - x[4..7].i * y[4..7].i
vmla.f32    q11, q2, q7        @ q11 = q11 + x[4..7].r * y[4..7].i

@ Thus:
@ q8 = res[0..3].r
@ q9 = res[0..3].i
@ q10 = res[4..7].r
@ q11 = res[4..7].i

vst2.32     {q8-q9}, [r8]!      @ Store res[0..3]
vst2.32     {q10-q11}, [r8]!      @ Store res[4..7]

vadd.f32 q12, q12, q8
vadd.f32 q12, q12, q10

vadd.f32 q13, q13, q9
vadd.f32 q13, q13, q11

subs        r9, r9, #8          @ Decrement count (8 complex multiplications per iteration)
bne         .innerloop     @ If result != 0, still elements to process

vmul.f32 q0, q12, q14
vpadd.f32 d0, d0, d1
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r10]!

vmul.f32 q0, q13, q14
vpadd.f32 d0, d0, d1
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r11]!

subs r1, r1, #1
bne .outerloop

restore_caller_vectors

pop {r4-r11,lr}

bx          lr                  @ Return from function




test:

// r0: *x
//r1: size
// r2: pointers
// r3: M



push {r4-r11,lr}        // 6 regs are pushed
                       // SP is decremented by 6*4 = 24 bytes
preserve_caller_vectors

ldr r10, [r0]
ldr r11, [r0, #4]


.testloop:

//LOAD y
ldr r4, [r10]
vdup.32 q4,  r4
ldr r4, [r11]
vdup.32 q5,  r4
vadd.f32     q4, q4, q5
vmov.f32 q5, #4.
vmul.f32 q0, q4, q5
vst1.32 d0[0], [r10]!
vst1.32 d0[0], [r11]!

subs r1, r1, #1
bne .testloop

restore_caller_vectors

pop {r4-r11,lr}

bx          lr                  @ Return from function






@ NOTE: COUNT SHOULD BE A MULTIPLE OF 8!

@
@ float phasorfilter2(float in, float M, float **array);
@

phasorfilter2:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: y
@ r1: pointers
@ r2: y
@ r5: count
@ r6 : result real
@ r7 : result imag
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



push {r4-r11,lr}        // 6 regs are pushed
                       // SP is decremented by 6*4 = 24 bytes
preserve_caller_vectors

ldr r4, [r1]     //_pre get array_pointers[0] into r4
ldr r5, [r1, #4] // ym_prev get array_pointers[1] into r5
ldr r6, [r1, #8] // aa get array_pointers[2] into r6
ldr r7, [r1, #12] // post get array_pointers[2] into r6
ldr r8, [r1, #16] // get array_pointers[2] into r6
ldr r9, [r1, #20] // get array_pointers[2] into r6
mov r10, r5
vdup.32     q15,  r0 @ q15 = y

vmov.f32 q12, #0.
vmov.f32 q13, #0.


//    float *tester[6] = {reinterpret_cast<float*>(_pre), reinterpret_cast<float*>(ym_prev), reinterpret_cast<float*>(aa), reinterpret_cast<float*>(_post), &res1, &res2};


.loop_begin:
vld2.32     {q0-q1}, [r4]!    // q0 = x[0..3].r, q1 = x[0..3].i
vld2.32     {q2-q3}, [r4]!    // q2 = x[4..7].r, q3 = x[4..7].i
vmul.f32    q0, q0, q15       // q0 = x[0..3].r * y[0..3].r
vmul.f32    q1, q1, q15       // q1 = x[0..3].i * y[0..3].r
vmul.f32    q2, q2, q15       // q2 = x[4..7].r * y[4..7].r
vmul.f32    q3, q3, q15       // q3 = x[4..7].i * y[4..7].r


vld2.32     {q4-q5}, [r5]!    // q4 = y[0..3].r, q5 = y[0..3].i
vld2.32     {q6-q7}, [r5]!    // q6 = y[4..7].r, q7 = y[4..7].i

vld1.32     {q8-q9}, [r6]!    // q8 = damp[0..3], q9 = damp[4..7]

vmla.f32    q0, q4, q8         // q0 = x[0..3].r * y[0..3].r
vmla.f32    q1, q5, q8         // q1 = x[0..3].i * y[0..3].r
vmla.f32    q2, q6, q9        // q2 = x[4..7].r * y[4..7].r
vmla.f32    q3, q7, q9        // q3 = x[4..7].i * y[4..7].r

vld2.32     {q4-q5}, [r7]!    @ q4 = x[0..3].r, q5 = x[0..3].i
vld2.32     {q6-q7}, [r7]!    @ q6 = x[4..7].r, q7 = x[4..7].i


vmul.f32    q8, q0, q4         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q9, q1, q4         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q10, q2, q6        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q11, q3, q6        @ q3 = x[4..7].i * y[4..7].r

vmls.f32    q8, q1, q5         @ q0 = q0 - x[0..3].i * y[0..3].i
vmla.f32    q9, q0, q5         @ q1 = q1 + x[0..3].r * y[0..3].i
vmls.f32    q10, q3, q7        @ q2 = q2 - x[4..7].i * y[4..7].i
vmla.f32    q11, q2, q7        @ q3 = q3 + x[4..7].r * y[4..7].i

@ Thus:
@ q8 = res[0..3].r
@ q9 = res[0..3].i
@ q10 = res[4..7].r
@ q11 = res[4..7].i


vadd.f32 q12, q12, q8
vadd.f32 q12, q12, q10

vadd.f32 q13, q13, q9
vadd.f32 q13, q13, q11


vst2.32     {q8-q9}, [r10]!      @ Store res[0..3]
vst2.32     {q10-q11}, [r10]!      @ Store res[4..7]

subs        r2, r2, #8          @ Decrement count (8 complex multiplications per iteration)
bne         .loop_begin     @ If result != 0, still elements to process

vpadd.f32 d0, d24, d25
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r8]

vpadd.f32 d0, d26, d27
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r9]



restore_caller_vectors

pop {r4-r11,lr}

bx          lr                  @ Return from function


phasorfilterhold:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Arguments:
@ r0: y
@ r1: pointers
@ r2: y
@ r5: count
@ r6 : result real
@ r7 : result imag
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



push {r4-r11,lr}        // 6 regs are pushed
                       // SP is decremented by 6*4 = 24 bytes
preserve_caller_vectors

ldr r4, [r1]     //_pre get array_pointers[0] into r4
ldr r5, [r1, #4] // ym_prev get array_pointers[1] into r5
ldr r6, [r1, #8] // aa get array_pointers[2] into r6
ldr r7, [r1, #12] // post get array_pointers[2] into r6
ldr r8, [r1, #16] // get array_pointers[2] into r6
ldr r9, [r1, #20] // get array_pointers[2] into r6
mov r10, r5
vdup.32     q15,  r0 @ q15 = y

vmov.f32 q12, #0.
vmov.f32 q13, #0.


//    float *tester[6] = {reinterpret_cast<float*>(_pre), reinterpret_cast<float*>(ym_prev), reinterpret_cast<float*>(aa), reinterpret_cast<float*>(_post), &res1, &res2};


.hold_begin:
vld2.32     {q0-q1}, [r5]!    // q4 = y[0..3].r, q5 = y[0..3].i
vld2.32     {q2-q3}, [r5]!    // q6 = y[4..7].r, q7 = y[4..7].i

vld2.32     {q4-q5}, [r7]!    @ q4 = x[0..3].r, q5 = x[0..3].i
vld2.32     {q6-q7}, [r7]!    @ q6 = x[4..7].r, q7 = x[4..7].i


vmul.f32    q8, q0, q4         @ q0 = x[0..3].r * y[0..3].r
vmul.f32    q9, q1, q4         @ q1 = x[0..3].i * y[0..3].r
vmul.f32    q10, q2, q6        @ q2 = x[4..7].r * y[4..7].r
vmul.f32    q11, q3, q6        @ q3 = x[4..7].i * y[4..7].r

vmls.f32    q8, q1, q5         @ q0 = q0 - x[0..3].i * y[0..3].i
vmla.f32    q9, q0, q5         @ q1 = q1 + x[0..3].r * y[0..3].i
vmls.f32    q10, q3, q7        @ q2 = q2 - x[4..7].i * y[4..7].i
vmla.f32    q11, q2, q7        @ q3 = q3 + x[4..7].r * y[4..7].i

@ Thus:
@ q8 = res[0..3].r
@ q9 = res[0..3].i
@ q10 = res[4..7].r
@ q11 = res[4..7].i


vadd.f32 q12, q12, q8
vadd.f32 q12, q12, q10

vadd.f32 q13, q13, q9
vadd.f32 q13, q13, q11


vst2.32     {q8-q9}, [r10]!      @ Store res[0..3]
vst2.32     {q10-q11}, [r10]!      @ Store res[4..7]

subs        r2, r2, #8          @ Decrement count (8 complex multiplications per iteration)
bne         .hold_begin     @ If result != 0, still elements to process

vpadd.f32 d0, d24, d25
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r8]

vpadd.f32 d0, d26, d27
vadd.f32 s0, s0, s1
vst1.32 d0[0], [r9]

restore_caller_vectors

pop {r4-r11,lr}

bx          lr                  @ Return from function
