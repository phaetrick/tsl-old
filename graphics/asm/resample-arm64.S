.text
#if defined(__APPLE__)
.global _arm_neon_dotproduct32_double
.global _arm_neon_dotproduct16_double
.global _arm_neon_rs_readframe_sinc32
.global _arm_neon_dotproduct64_single
.global _arm_neon_dotproduct128_single
.global _arm_neon_rs_readframe_sinc64_float
.global _arm_neon_rs2x_32_double
.global _arm_neon_rs4x_32_double
.global _arm_test
.global _arm_neon_interpolate_cubic_32
#else
.global arm_neon_dotproduct32_double
.global arm_neon_dotproduct16_double
.global arm_neon_rs_readframe_sinc32
.global arm_neon_dotproduct64_single
.global arm_neon_dotproduct128_single
.global arm_neon_rs_readframe_sinc64_float
.global arm_neon_rs2x_32_double
.global arm_neon_rs4x_32_double
.global arm_test
.global arm_neon_interpolate_cubic_32
#endif

.macro preserve_caller_vectors
  stp d8,d9,[sp,#-16]!
  stp d10,d11,[sp,#-16]!
  stp d12,d13,[sp,#-16]!
  stp d14,d15,[sp,#-16]!
.endm
.macro restore_caller_vectors
  ldp d14,d15,[sp],#16
  ldp d12,d13,[sp],#16
  ldp d10,d11,[sp],#16
  ldp d8,d9,[sp],#16
.endm

#if defined(__APPLE__)
_arm_neon_rs2x_32_double:
#else
arm_neon_rs2x_32_double:
#endif
preserve_caller_vectors

movi           v24.2d, 0x0

ld1 {v0.2d, v1.2d, v2.2d, v3.2d},[x1], 64
ld1 {v4.2d, v5.2d, v6.2d, v7.2d},[x1], 64
ld1 {v8.2d, v9.2d, v10.2d, v11.2d},[x1], 64
ld1 {v12.2d, v13.2d, v14.2d, v15.2d},[x1], 64
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0],8

movi           v24.2d, 0x0

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0]

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_rs4x_32_double:
#else
arm_neon_rs4x_32_double:
#endif
preserve_caller_vectors

movi           v24.2d, 0x0

ld1 {v0.2d, v1.2d, v2.2d, v3.2d},[x1], 64
ld1 {v4.2d, v5.2d, v6.2d, v7.2d},[x1], 64
ld1 {v8.2d, v9.2d, v10.2d, v11.2d},[x1], 64
ld1 {v12.2d, v13.2d, v14.2d, v15.2d},[x1], 64
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0],8

movi           v24.2d, 0x0

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0],8

movi           v24.2d, 0x0

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0],8

movi           v24.2d, 0x0

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x2], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x2], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d26,v24.d[0]
mov d27,v24.d[1]
fadd d26, d26, d27
str d26, [x0],8

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_dotproduct16_double:
#else
arm_neon_dotproduct16_double:
#endif
preserve_caller_vectors

movi           v24.2d, 0x0

ld1 {v0.2d, v1.2d, v2.2d, v3.2d},[x0], 64
ld1 {v4.2d, v5.2d, v6.2d, v7.2d},[x0], 64
ld1 {v8.2d, v9.2d, v10.2d, v11.2d},[x1], 64
ld1 {v12.2d, v13.2d, v14.2d, v15.2d},[x1], 64

fmla v24.2d, v0.2d, v8.2d
fmla v24.2d, v1.2d, v9.2d
fmla v24.2d, v2.2d, v10.2d
fmla v24.2d, v3.2d, v11.2d
fmla v24.2d, v4.2d, v12.2d
fmla v24.2d, v5.2d, v13.2d
fmla v24.2d, v6.2d, v14.2d
fmla v24.2d, v7.2d, v15.2d

mov d0,v24.d[0]
mov d1,v24.d[1]
fadd d0, d0, d1

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_dotproduct32_double:
#else
arm_neon_dotproduct32_double:
#endif
preserve_caller_vectors

movi           v24.2d, 0x0

ld1 {v0.2d, v1.2d, v2.2d, v3.2d},[x0], 64
ld1 {v4.2d, v5.2d, v6.2d, v7.2d},[x0], 64
ld1 {v8.2d, v9.2d, v10.2d, v11.2d},[x0], 64
ld1 {v12.2d, v13.2d, v14.2d, v15.2d},[x0], 64
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x1], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x1], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x1], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x1], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

mov d0,v24.d[0]
mov d1,v24.d[1]
fadd d0, d0, d1

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_interpolate_cubic_32:
#else
arm_neon_interpolate_cubic_32:
#endif
preserve_caller_vectors
movi           v24.2d, 0x0
movi           v25.2d, 0x0
movi           v26.2d, 0x0
movi           v27.2d, 0x0
ld1 {v0.2d, v1.2d, v2.2d, v3.2d},[x1], 64
ld1 {v4.2d, v5.2d, v6.2d, v7.2d},[x1], 64
ld1 {v8.2d, v9.2d, v10.2d, v11.2d},[x1], 64
ld1 {v12.2d, v13.2d, v14.2d, v15.2d},[x1], 64

ldr x3, [x2],8
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v24.2d, v0.2d, v16.2d
fmla v24.2d, v1.2d, v17.2d
fmla v24.2d, v2.2d, v18.2d
fmla v24.2d, v3.2d, v19.2d
fmla v24.2d, v4.2d, v20.2d
fmla v24.2d, v5.2d, v21.2d
fmla v24.2d, v6.2d, v22.2d
fmla v24.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v24.2d, v8.2d, v16.2d
fmla v24.2d, v9.2d, v17.2d
fmla v24.2d, v10.2d, v18.2d
fmla v24.2d, v11.2d, v19.2d
fmla v24.2d, v12.2d, v20.2d
fmla v24.2d, v13.2d, v21.2d
fmla v24.2d, v14.2d, v22.2d
fmla v24.2d, v15.2d, v23.2d

ldr x3, [x2],8
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v25.2d, v0.2d, v16.2d
fmla v25.2d, v1.2d, v17.2d
fmla v25.2d, v2.2d, v18.2d
fmla v25.2d, v3.2d, v19.2d
fmla v25.2d, v4.2d, v20.2d
fmla v25.2d, v5.2d, v21.2d
fmla v25.2d, v6.2d, v22.2d
fmla v25.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v25.2d, v8.2d, v16.2d
fmla v25.2d, v9.2d, v17.2d
fmla v25.2d, v10.2d, v18.2d
fmla v25.2d, v11.2d, v19.2d
fmla v25.2d, v12.2d, v20.2d
fmla v25.2d, v13.2d, v21.2d
fmla v25.2d, v14.2d, v22.2d
fmla v25.2d, v15.2d, v23.2d

ldr x3, [x2],8
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v26.2d, v0.2d, v16.2d
fmla v26.2d, v1.2d, v17.2d
fmla v26.2d, v2.2d, v18.2d
fmla v26.2d, v3.2d, v19.2d
fmla v26.2d, v4.2d, v20.2d
fmla v26.2d, v5.2d, v21.2d
fmla v26.2d, v6.2d, v22.2d
fmla v26.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v26.2d, v8.2d, v16.2d
fmla v26.2d, v9.2d, v17.2d
fmla v26.2d, v10.2d, v18.2d
fmla v26.2d, v11.2d, v19.2d
fmla v26.2d, v12.2d, v20.2d
fmla v26.2d, v13.2d, v21.2d
fmla v26.2d, v14.2d, v22.2d
fmla v26.2d, v15.2d, v23.2d

ldr x3, [x2],8
ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v27.2d, v0.2d, v16.2d
fmla v27.2d, v1.2d, v17.2d
fmla v27.2d, v2.2d, v18.2d
fmla v27.2d, v3.2d, v19.2d
fmla v27.2d, v4.2d, v20.2d
fmla v27.2d, v5.2d, v21.2d
fmla v27.2d, v6.2d, v22.2d
fmla v27.2d, v7.2d, v23.2d

ld1 {v16.2d, v17.2d, v18.2d, v19.2d},[x3], 64
ld1 {v20.2d, v21.2d, v22.2d, v23.2d},[x3], 64

fmla v27.2d, v8.2d, v16.2d
fmla v27.2d, v9.2d, v17.2d
fmla v27.2d, v10.2d, v18.2d
fmla v27.2d, v11.2d, v19.2d
fmla v27.2d, v12.2d, v20.2d
fmla v27.2d, v13.2d, v21.2d
fmla v27.2d, v14.2d, v22.2d
fmla v27.2d, v15.2d, v23.2d

mov d3,v24.d[0]
mov d4,v24.d[1]
fadd d3, d3, d4
str d3, [x0],8

mov d3,v25.d[0]
mov d4,v25.d[1]
fadd d3, d3, d4
str d3, [x0],8

mov d3,v26.d[0]
mov d4,v26.d[1]
fadd d3, d3, d4
str d3, [x0],8

mov d3,v27.d[0]
mov d4,v27.d[1]
fadd d3, d3, d4
str d3, [x0]

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_dotproduct64_single:
#else
arm_neon_dotproduct64_single:
#endif
preserve_caller_vectors

movi           v24.4s, 0x0

ld1 {v0.4s, v1.4s, v2.4s, v3.4s},[x0], 64
ld1 {v4.4s, v5.4s, v6.4s, v7.4s},[x0], 64
ld1 {v8.4s, v9.4s, v10.4s, v11.4s},[x0], 64
ld1 {v12.4s, v13.4s, v14.4s, v15.4s},[x0], 64
ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v0.4s, v16.4s
fmla v24.4s, v1.4s, v17.4s
fmla v24.4s, v2.4s, v18.4s
fmla v24.4s, v3.4s, v19.4s
fmla v24.4s, v4.4s, v20.4s
fmla v24.4s, v5.4s, v21.4s
fmla v24.4s, v6.4s, v22.4s
fmla v24.4s, v7.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v8.4s, v16.4s
fmla v24.4s, v9.4s, v17.4s
fmla v24.4s, v10.4s, v18.4s
fmla v24.4s, v11.4s, v19.4s
fmla v24.4s, v12.4s, v20.4s
fmla v24.4s, v13.4s, v21.4s
fmla v24.4s, v14.4s, v22.4s
fmla v24.4s, v15.4s, v23.4s

mov s0,v24.s[0]
mov s1,v24.s[1]
mov s2,v24.s[2]
mov s3,v24.s[3]
fadd s0, s0, s1
fadd s0, s0, s2
fadd s0, s0, s3

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_dotproduct128_single:
#else
arm_neon_dotproduct128_single:
#endif
preserve_caller_vectors

movi           v24.4s, 0x0

ld1 {v0.4s, v1.4s, v2.4s, v3.4s},[x0], 64
ld1 {v4.4s, v5.4s, v6.4s, v7.4s},[x0], 64
ld1 {v8.4s, v9.4s, v10.4s, v11.4s},[x0], 64
ld1 {v12.4s, v13.4s, v14.4s, v15.4s},[x0], 64
ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v0.4s, v16.4s
fmla v24.4s, v1.4s, v17.4s
fmla v24.4s, v2.4s, v18.4s
fmla v24.4s, v3.4s, v19.4s
fmla v24.4s, v4.4s, v20.4s
fmla v24.4s, v5.4s, v21.4s
fmla v24.4s, v6.4s, v22.4s
fmla v24.4s, v7.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v8.4s, v16.4s
fmla v24.4s, v9.4s, v17.4s
fmla v24.4s, v10.4s, v18.4s
fmla v24.4s, v11.4s, v19.4s
fmla v24.4s, v12.4s, v20.4s
fmla v24.4s, v13.4s, v21.4s
fmla v24.4s, v14.4s, v22.4s
fmla v24.4s, v15.4s, v23.4s

ld1 {v0.4s, v1.4s, v2.4s, v3.4s},[x0], 64
ld1 {v4.4s, v5.4s, v6.4s, v7.4s},[x0], 64
ld1 {v8.4s, v9.4s, v10.4s, v11.4s},[x0], 64
ld1 {v12.4s, v13.4s, v14.4s, v15.4s},[x0], 64
ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v0.4s, v16.4s
fmla v24.4s, v1.4s, v17.4s
fmla v24.4s, v2.4s, v18.4s
fmla v24.4s, v3.4s, v19.4s
fmla v24.4s, v4.4s, v20.4s
fmla v24.4s, v5.4s, v21.4s
fmla v24.4s, v6.4s, v22.4s
fmla v24.4s, v7.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x1], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x1], 64

fmla v24.4s, v8.4s, v16.4s
fmla v24.4s, v9.4s, v17.4s
fmla v24.4s, v10.4s, v18.4s
fmla v24.4s, v11.4s, v19.4s
fmla v24.4s, v12.4s, v20.4s
fmla v24.4s, v13.4s, v21.4s
fmla v24.4s, v14.4s, v22.4s
fmla v24.4s, v15.4s, v23.4s

mov s0,v24.s[0]
mov s1,v24.s[1]
mov s2,v24.s[2]
mov s3,v24.s[3]
fadd s0, s0, s1
fadd s0, s0, s2
fadd s0, s0, s3

restore_caller_vectors
ret lr

#if defined(__APPLE__)
_arm_neon_rs_readframe_sinc64_float:
#else
arm_neon_rs_readframe_sinc64_float:
#endif
preserve_caller_vectors
movi           v24.4s, 0x0
movi           v25.4s, 0x0
ld1 {v0.4s, v1.4s, v2.4s, v3.4s},[x2], 64
ld1 {v4.4s, v5.4s, v6.4s, v7.4s},[x2], 64
ld1 {v8.4s, v9.4s, v10.4s, v11.4s},[x2], 64
ld1 {v12.4s, v13.4s, v14.4s, v15.4s},[x2], 64
ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x3], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x3], 64

fmla v24.4s, v0.4s, v16.4s
fmla v24.4s, v1.4s, v17.4s
fmla v24.4s, v2.4s, v18.4s
fmla v24.4s, v3.4s, v19.4s
fmla v24.4s, v4.4s, v20.4s
fmla v24.4s, v5.4s, v21.4s
fmla v24.4s, v6.4s, v22.4s
fmla v24.4s, v7.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x3], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x3], 64

fmla v24.4s, v8.4s, v16.4s
fmla v24.4s, v9.4s, v17.4s
fmla v24.4s, v10.4s, v18.4s
fmla v24.4s, v11.4s, v19.4s
fmla v24.4s, v12.4s, v20.4s
fmla v24.4s, v13.4s, v21.4s
fmla v24.4s, v14.4s, v22.4s
fmla v24.4s, v15.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x4], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x4], 64

fmla v25.4s, v0.4s, v16.4s
fmla v25.4s, v1.4s, v17.4s
fmla v25.4s, v2.4s, v18.4s
fmla v25.4s, v3.4s, v19.4s
fmla v25.4s, v4.4s, v20.4s
fmla v25.4s, v5.4s, v21.4s
fmla v25.4s, v6.4s, v22.4s
fmla v25.4s, v7.4s, v23.4s

ld1 {v16.4s, v17.4s, v18.4s, v19.4s},[x4], 64
ld1 {v20.4s, v21.4s, v22.4s, v23.4s},[x4], 64

fmla v25.4s, v8.4s, v16.4s
fmla v25.4s, v9.4s, v17.4s
fmla v25.4s, v10.4s, v18.4s
fmla v25.4s, v11.4s, v19.4s
fmla v25.4s, v12.4s, v20.4s
fmla v25.4s, v13.4s, v21.4s
fmla v25.4s, v14.4s, v22.4s
fmla v25.4s, v15.4s, v23.4s

mov s3,v24.s[0]
mov s4,v24.s[1]
mov s5,v24.s[2]
mov s6,v24.s[3]
fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6
str s3, [x0]

mov s3,v25.s[0]
mov s4,v25.s[1]
mov s5,v25.s[2]
mov s6,v25.s[3]
fadd s3, s3, s4
fadd s3, s3, s5
fadd s3, s3, s6
str s3, [x1]

restore_caller_vectors
ret lr