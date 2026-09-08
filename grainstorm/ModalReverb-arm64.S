.text
#if defined(__APPLE__)
.global _phasorfilter
.global _phasorfilterhold
.global _phasorfilter_double
.global _phasorfilterhold_double
#else
.global phasorfilter
.global phasorfilterhold
.global phasorfilter_double
.global phasorfilterhold_double
#endif

// preserve_caller_vectors(): Push first 64-bits of v8-v15 on stack (sp)
.macro preserve_caller_vectors
  stp d8,d9, [sp,#-16]!
  stp d10,d11,[sp,#-16]!
  stp d12,d13,[sp,#-16]!
  stp d14,d15,[sp,#-16]!
.endm// restore_caller_vectors(): Restore first 64-bits from v8v-15 on stack (sp)

.macro restore_caller_vectors
  ldp d14,d15,[sp],#16
  ldp d12,d13,[sp],#16
  ldp d10,d11,[sp],#16
  ldp d8,d9,[sp],#16
.endm

.macro save_registers //x19 - x28 callee saved, x9-x15 corruptible
stp x24, x23, [sp, -0x40]!
stp x22, x21, [sp, 0x10]
stp x20, x19, [sp, 0x20]
stp x29, x30, [sp, 0x30]
add x29, sp, 0x30
.endm

.macro restore_registers
ldp x29, x30, [sp, 0x30]
ldp x20, x19, [sp, 0x20]
ldp x22, x21, [sp, 0x10]
ldp x24, x23, [sp], 0x40
.endm


#if defined(__APPLE__)
_phasorfilter:
#else
phasorfilter:
#endif
preserve_caller_vectors

ldr x9,  [x1], #8     //_pre get array_pointers[0] into r4
ldr x10, [x1], #8 // ym_prev get array_pointers[1] into r5
ldr x11, [x1], #8 // aa get array_pointers[2] into r6
ldr x12, [x1], #8 // post get array_pointers[2] into r6
ldr x13, [x1], #8 // get array_pointers[2] into r6
ldr x14, [x1], #8 // get array_pointers[2] into r6

ld1r { v10.4s }, [x0]

movi           v8.4s, 0x0
movi           v9.4s, 0x0

mov x15, x10

.loop:

ld4.4s {v0-v3}, [x9], 64    // v0 = x[0..3].r, v1 = x[0..3].i, v2 = x[4..7].r, v3 = x[4..7].i
ld4.4s {v4-v7}, [x9], 64    // v4 = x[8..11].r, v5 = x[8..11].i, v6 = x[12..15].r, v7 = x[12...15].i

fmul.4s v0, v0, v10
fmul.4s v1, v1, v10
fmul.4s v2, v2, v10
fmul.4s v3, v3, v10
fmul.4s v4, v4, v10
fmul.4s v5, v5, v10
fmul.4s v6, v6, v10
fmul.4s v7, v7, v10

ld1.4s     {v24-v27}, [x11], 64    // v24 = damp[0..3], v25 = damp[4..7], v26 = damp[8..11], v27 = damp[12..15]

ld4.4s {v16-v19}, [x10], 64    // v16 = y_prev[0..3].r, v17 = y_prev[0..3].i, v18 = y_prev[4..7].r, v19 = y_prev[4..7].i
ld4.4s {v20-v23}, [x10], 64    // v20 = y_prev[8..11].r, y_prev21 = x[8..11].i, v22 = y_prev[12..15].r, v23 = y_prev[12...15].i

fmla.4s    v0, v16, v24         // v0 = v0 + y_prev[0..3].r * damp[0..3]
fmla.4s    v1, v17, v24         // v1 = v1 + y_prev[0..3].i * damp[0..3]
fmla.4s    v2, v18, v25         // v2 = v0 + y_prev[4..7].r * damp[4..7]
fmla.4s    v3, v19, v25         // v3 = v3 + y_prev[4..7].i * damp[4..7]
fmla.4s    v4, v20, v26         // v4 = v4 + y_prev[8..11].r * damp[8..11]
fmla.4s    v5, v21, v26         // v5 = v5 + y_prev[8..11].i * damp[8..11]
fmla.4s    v6, v22, v27         // v6 = v6 + y_prev[12..15].r * damp[12..15]
fmla.4s    v7, v23, v27         // v7 = v7 + y_prev[12..15].i * damp[12..15]

ld4.4s {v16-v19}, [x12], 64    // v16 = post[0..3].r, v17 = post[0..3].i, v18 = post[4..7].r, v19 = post[4..7].i
ld4.4s {v20-v23}, [x12], 64    // v20 = post[8..11].r, v21 = post[8..11].i, v22 = post[12..15].r, v23 = post[12...15].i

fmul.4s    v24, v0, v16         // q0 = x[0..3].r * y[0..3].r
fmul.4s    v25, v1, v16         // q1 = x[0..3].i * y[0..3].r
fmul.4s    v26, v2, v18        // q2 = x[4..7].r * y[4..7].r
fmul.4s    v27, v3, v18        // q3 = x[4..7].i * y[4..7].r
fmul.4s    v28, v4, v20         // q4 = x[8..11].r * y[8..11].r
fmul.4s    v29, v5, v20         // q5 = x[8..11].i * y[8..11].r
fmul.4s    v30, v6, v22        // q6 = x[12..15].r * y[12..15].r
fmul.4s    v31, v7, v22        // q7 = x[12..15].i * y[12..15].r

fmls.4s    v24, v1, v17         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.4s    v25, v0, v17         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.4s    v26, v3, v19        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.4s    v27, v2, v19        // q3 = q3 + x[4..7].r * y[4..7].i
fmls.4s    v28, v5, v21         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.4s    v29, v4, v21         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.4s    v30, v7, v23        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.4s    v31, v6, v23        // q3 = q3 + x[4..7].r * y[4..7].i

st4.4s     {v24-v27}, [x15], 64      //@ Store res[0..3]
st4.4s     {v28-v31}, [x15], 64      //@ Store res[4..7]

fadd.4s v8, v8, v24
fadd.4s v8, v8, v26
fadd.4s v8, v8, v28
fadd.4s v8, v8, v30

fadd.4s v9, v9, v25
fadd.4s v9, v9, v27
fadd.4s v9, v9, v29
fadd.4s v9, v9, v31

subs        x2, x2, 16          //@ Decrement count (8 complex MAC's per iteration)
bne         .loop     //@ If result != 0, still elements to process


mov s3,v8.s[0]
mov s4,v8.s[1]
mov s5,v8.s[2]
mov s6,v8.s[3]

fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6

str s3, [x13]

mov s3,v9.s[0]
mov s4,v9.s[1]
mov s5,v9.s[2]
mov s6,v9.s[3]

fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6

str s3, [x14]

/*
addv s8, v8.4s
addv s9, v9.4s

str s8, [x13]
str s9, [x14]
*/

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_phasorfilterhold:
#else
phasorfilterhold:
#endif
preserve_caller_vectors

ldr x9,  [x1], #8     //_pre get array_pointers[0] into r4
ldr x10, [x1], #8 // ym_prev get array_pointers[1] into r5
ldr x11, [x1], #8 // aa get array_pointers[2] into r6
ldr x12, [x1], #8 // post get array_pointers[2] into r6
ldr x13, [x1], #8 // get array_pointers[2] into r6
ldr x14, [x1], #8 // get array_pointers[2] into r6

movi           v8.4s, 0x0
movi           v9.4s, 0x0

mov x15, x10

.hold_begin:
ld4.4s {v0-v3}, [x10], 64    // v16 = y_prev[0..3].r, v17 = y_prev[0..3].i, v18 = y_prev[4..7].r, v19 = y_prev[4..7].i
ld4.4s {v4-v7}, [x10], 64    // v20 = y_prev[8..11].r, y_prev21 = x[8..11].i, v22 = y_prev[12..15].r, v23 = y_prev[12...15].i

ld4.4s {v16-v19}, [x12], 64    // v16 = post[0..3].r, v17 = post[0..3].i, v18 = post[4..7].r, v19 = post[4..7].i
ld4.4s {v20-v23}, [x12], 64    // v20 = post[8..11].r, v21 = post[8..11].i, v22 = post[12..15].r, v23 = post[12...15].i

fmul.4s    v24, v0, v16         // q0 = x[0..3].r * y[0..3].r
fmul.4s    v25, v1, v16         // q1 = x[0..3].i * y[0..3].r
fmul.4s    v26, v2, v18        // q2 = x[4..7].r * y[4..7].r
fmul.4s    v27, v3, v18        // q3 = x[4..7].i * y[4..7].r
fmul.4s    v28, v4, v20         // q4 = x[8..11].r * y[8..11].r
fmul.4s    v29, v5, v20         // q5 = x[8..11].i * y[8..11].r
fmul.4s    v30, v6, v22        // q6 = x[12..15].r * y[12..15].r
fmul.4s    v31, v7, v22        // q7 = x[12..15].i * y[12..15].r

fmls.4s    v24, v1, v17         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.4s    v25, v0, v17         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.4s    v26, v3, v19        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.4s    v27, v2, v19        // q3 = q3 + x[4..7].r * y[4..7].i
fmls.4s    v28, v5, v21         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.4s    v29, v4, v21         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.4s    v30, v7, v23        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.4s    v31, v6, v23        // q3 = q3 + x[4..7].r * y[4..7].i

st4.4s     {v24-v27}, [x15], 64      //@ Store res[0..3]
st4.4s     {v28-v31}, [x15], 64      //@ Store res[4..7]

fadd.4s v8, v8, v24
fadd.4s v8, v8, v26
fadd.4s v8, v8, v28
fadd.4s v8, v8, v30

fadd.4s v9, v9, v25
fadd.4s v9, v9, v27
fadd.4s v9, v9, v29
fadd.4s v9, v9, v31

subs        x2, x2, 16          //@ Decrement count (8 complex MAC's per iteration)
bne         .hold_begin     //@ If result != 0, still elements to process

mov s3,v8.s[0]
mov s4,v8.s[1]
mov s5,v8.s[2]
mov s6,v8.s[3]

fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6

str s3, [x13]

mov s3,v9.s[0]
mov s4,v9.s[1]
mov s5,v9.s[2]
mov s6,v9.s[3]

fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6

str s3, [x14]

restore_caller_vectors
ret lr


#if defined(__APPLE__)
_phasorfilter_double:
#else
phasorfilter_double:
#endif
preserve_caller_vectors

ldr x9,  [x1], #8     //_pre get array_pointers[0] into r4
ldr x10, [x1], #8 // ym_prev get array_pointers[1] into r5
ldr x11, [x1], #8 // aa get array_pointers[2] into r6
ldr x12, [x1], #8 // post get array_pointers[2] into r6
ldr x13, [x1], #8 // get array_pointers[2] into r6
ldr x14, [x1], #8 // get array_pointers[2] into r6

ld1r { v10.2d }, [x0]

movi           v8.2d, 0x0
movi           v9.2d, 0x0

mov x15, x10

.loopd:

ld4.2d {v0-v3}, [x9], 64    // v0 = x[0..3].r, v1 = x[0..3].i, v2 = x[4..7].r, v3 = x[4..7].i
ld4.2d {v4-v7}, [x9], 64    // v4 = x[8..11].r, v5 = x[8..11].i, v6 = x[12..15].r, v7 = x[12...15].i

fmul.2d v0, v0, v10
fmul.2d v1, v1, v10
fmul.2d v2, v2, v10
fmul.2d v3, v3, v10
fmul.2d v4, v4, v10
fmul.2d v5, v5, v10
fmul.2d v6, v6, v10
fmul.2d v7, v7, v10

ld1.2d     {v24-v27}, [x11], 64    // v24 = damp[0..3], v25 = damp[4..7], v26 = damp[8..11], v27 = damp[12..15]

ld4.2d {v16-v19}, [x10], 64    // v16 = y_prev[0..3].r, v17 = y_prev[0..3].i, v18 = y_prev[4..7].r, v19 = y_prev[4..7].i
ld4.2d {v20-v23}, [x10], 64    // v20 = y_prev[8..11].r, y_prev21 = x[8..11].i, v22 = y_prev[12..15].r, v23 = y_prev[12...15].i

fmla.2d    v0, v16, v24         // v0 = v0 + y_prev[0..3].r * damp[0..3]
fmla.2d    v1, v17, v24         // v1 = v1 + y_prev[0..3].i * damp[0..3]
fmla.2d    v2, v18, v25         // v2 = v0 + y_prev[4..7].r * damp[4..7]
fmla.2d    v3, v19, v25         // v3 = v3 + y_prev[4..7].i * damp[4..7]
fmla.2d    v4, v20, v26         // v4 = v4 + y_prev[8..11].r * damp[8..11]
fmla.2d    v5, v21, v26         // v5 = v5 + y_prev[8..11].i * damp[8..11]
fmla.2d    v6, v22, v27         // v6 = v6 + y_prev[12..15].r * damp[12..15]
fmla.2d    v7, v23, v27         // v7 = v7 + y_prev[12..15].i * damp[12..15]

ld4.2d {v16-v19}, [x12], 64    // v16 = post[0..3].r, v17 = post[0..3].i, v18 = post[4..7].r, v19 = post[4..7].i
ld4.2d {v20-v23}, [x12], 64    // v20 = post[8..11].r, v21 = post[8..11].i, v22 = post[12..15].r, v23 = post[12...15].i

fmul.2d    v24, v0, v16         // q0 = x[0..3].r * y[0..3].r
fmul.2d    v25, v1, v16         // q1 = x[0..3].i * y[0..3].r
fmul.2d    v26, v2, v18        // q2 = x[4..7].r * y[4..7].r
fmul.2d    v27, v3, v18        // q3 = x[4..7].i * y[4..7].r
fmul.2d    v28, v4, v20         // q4 = x[8..11].r * y[8..11].r
fmul.2d    v29, v5, v20         // q5 = x[8..11].i * y[8..11].r
fmul.2d    v30, v6, v22        // q6 = x[12..15].r * y[12..15].r
fmul.2d    v31, v7, v22        // q7 = x[12..15].i * y[12..15].r

fmls.2d    v24, v1, v17         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.2d    v25, v0, v17         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.2d    v26, v3, v19        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.2d    v27, v2, v19        // q3 = q3 + x[4..7].r * y[4..7].i
fmls.2d    v28, v5, v21         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.2d    v29, v4, v21         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.2d    v30, v7, v23        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.2d    v31, v6, v23        // q3 = q3 + x[4..7].r * y[4..7].i

st4.2d     {v24-v27}, [x15], 64      //@ Store res[0..3]
st4.2d     {v28-v31}, [x15], 64      //@ Store res[4..7]

fadd.2d v8, v8, v24
fadd.2d v8, v8, v26
fadd.2d v8, v8, v28
fadd.2d v8, v8, v30

fadd.2d v9, v9, v25
fadd.2d v9, v9, v27
fadd.2d v9, v9, v29
fadd.2d v9, v9, v31

subs        x2, x2, 8          //@ Decrement count (8 complex MAC's per iteration)
bne         .loopd     //@ If result != 0, still elements to process


mov d3,v8.d[0]
mov d4,v8.d[1]
fadd d3, d3, d4

str d3, [x13]

mov d3,v9.d[0]
mov d4,v9.d[1]
fadd d3, d3, d4

str d3, [x14]

/*
addv s8, v8.4s
addv s9, v9.4s

str s8, [x13]
str s9, [x14]
*/

restore_caller_vectors
ret lr


#if defined(__APPLE__)
_phasorfilterhold_double:
#else
phasorfilterhold_double:
#endif

preserve_caller_vectors

ldr x9,  [x1], #8     //_pre get array_pointers[0] into r4
ldr x10, [x1], #8 // ym_prev get array_pointers[1] into r5
ldr x11, [x1], #8 // aa get array_pointers[2] into r6
ldr x12, [x1], #8 // post get array_pointers[2] into r6
ldr x13, [x1], #8 // get array_pointers[2] into r6
ldr x14, [x1], #8 // get array_pointers[2] into r6

movi           v8.2d, 0x0
movi           v9.2d, 0x0

mov x15, x10

.holdd_begin:
ld4.2d {v0-v3}, [x10], 64    // v16 = y_prev[0..3].r, v17 = y_prev[0..3].i, v18 = y_prev[4..7].r, v19 = y_prev[4..7].i
ld4.2d {v4-v7}, [x10], 64    // v20 = y_prev[8..11].r, y_prev21 = x[8..11].i, v22 = y_prev[12..15].r, v23 = y_prev[12...15].i

ld4.2d {v16-v19}, [x12], 64    // v16 = post[0..3].r, v17 = post[0..3].i, v18 = post[4..7].r, v19 = post[4..7].i
ld4.2d {v20-v23}, [x12], 64    // v20 = post[8..11].r, v21 = post[8..11].i, v22 = post[12..15].r, v23 = post[12...15].i

fmul.2d    v24, v0, v16         // q0 = x[0..3].r * y[0..3].r
fmul.2d    v25, v1, v16         // q1 = x[0..3].i * y[0..3].r
fmul.2d    v26, v2, v18        // q2 = x[4..7].r * y[4..7].r
fmul.2d    v27, v3, v18        // q3 = x[4..7].i * y[4..7].r
fmul.2d    v28, v4, v20         // q4 = x[8..11].r * y[8..11].r
fmul.2d    v29, v5, v20         // q5 = x[8..11].i * y[8..11].r
fmul.2d    v30, v6, v22        // q6 = x[12..15].r * y[12..15].r
fmul.2d    v31, v7, v22        // q7 = x[12..15].i * y[12..15].r

fmls.2d    v24, v1, v17         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.2d    v25, v0, v17         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.2d    v26, v3, v19        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.2d    v27, v2, v19        // q3 = q3 + x[4..7].r * y[4..7].i
fmls.2d    v28, v5, v21         // q0 = q0 - x[0..3].i * y[0..3].i
fmla.2d    v29, v4, v21         // q1 = q1 + x[0..3].r * y[0..3].i
fmls.2d    v30, v7, v23        // q2 = q2 - x[4..7].i * y[4..7].i
fmla.2d    v31, v6, v23        // q3 = q3 + x[4..7].r * y[4..7].i

st4.2d     {v24-v27}, [x15], 64      //@ Store res[0..3]
st4.2d     {v28-v31}, [x15], 64      //@ Store res[4..7]

fadd.2d v8, v8, v24
fadd.2d v8, v8, v26
fadd.2d v8, v8, v28
fadd.2d v8, v8, v30

fadd.2d v9, v9, v25
fadd.2d v9, v9, v27
fadd.2d v9, v9, v29
fadd.2d v9, v9, v31

subs        x2, x2, 8          //@ Decrement count (8 complex MAC's per iteration)
bne         .holdd_begin     //@ If result != 0, still elements to process

mov d3,v8.d[0]
mov d4,v8.d[1]

fadd d3, d3, d4

str d3, [x13]

mov d3,v9.d[0]
mov d4,v9.d[1]

fadd d3, d3, d4

str d3, [x14]

restore_caller_vectors
ret lr