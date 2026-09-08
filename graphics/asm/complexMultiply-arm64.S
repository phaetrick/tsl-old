.text
#if defined(__APPLE__)
.global _complexMultiplyFloat
.global _complexMultiplyAccumFloat
.global _complexMultiplyDouble
.global _complexMultiplyAccumDouble
#else
.global complexMultiplyFloat
.global complexMultiplyAccumFloat
.global complexMultiplyDouble
.global complexMultiplyAccumDouble
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
_complexMultiplyAccumDouble:
#else
complexMultiplyAccumDouble:
#endif
preserve_caller_vectors
.loop_macd_begin:
ld4.2d {v0-v3},[x1], 64
ld4.2d {v4-v7},[x1], 64
ld4.2d {v8-v11}, [x2], 64
ld4.2d {v12-v15}, [x3], 64
ld4.2d {v16-v19}, [x2], 64
ld4.2d {v20-v23}, [x3], 64
fmla.2d    v0, v8, v12
fmla.2d    v1, v9, v12
fmla.2d    v2, v10, v14
fmla.2d    v3, v11, v14
fmla.2d    v4, v16, v20
fmla.2d    v5, v17, v20
fmla.2d    v6, v18, v22
fmla.2d    v7, v19, v22
fmls.2d    v0, v9, v13
fmla.2d    v1, v8, v13
fmls.2d    v2, v11, v15
fmla.2d    v3, v10, v15
fmls.2d    v4, v17, v21
fmla.2d    v5, v16, v21
fmls.2d    v6, v19, v23
fmla.2d    v7, v18, v23
st4.2d     {v0-v3}, [x0], 64
st4.2d     {v4-v7}, [x0], 64
subs        x4, x4, 8
bne         .loop_macd_begin
restore_caller_vectors
ret lr

#if defined(__APPLE__)
_complexMultiplyDouble:
#else
complexMultiplyDouble:
#endif
preserve_caller_vectors
.loop_md_begin:
ld4.2d {v8-v11}, [x1], 64
ld4.2d {v12-v15}, [x2], 64
ld4.2d {v16-v19}, [x1], 64
ld4.2d {v20-v23}, [x2], 64
fmul.2d    v0, v8, v12
fmul.2d    v1, v9, v12
fmul.2d    v2, v10, v14
fmul.2d    v3, v11, v14
fmul.2d    v4, v16, v20
fmul.2d    v5, v17, v20
fmul.2d    v6, v18, v22
fmul.2d    v7, v19, v22
fmls.2d    v0, v9, v13
fmla.2d    v1, v8, v13
fmls.2d    v2, v11, v15
fmla.2d    v3, v10, v15
fmls.2d    v4, v17, v21
fmla.2d    v5, v16, v21
fmls.2d    v6, v19, v23
fmla.2d    v7, v18, v23
st4.2d     {v0-v3}, [x0], 64
st4.2d     {v4-v7}, [x0], 64
subs        x3, x3, 8
bne         .loop_md_begin
restore_caller_vectors
ret lr

#if defined(__APPLE__)
_complexMultiplyFloat:
#else
complexMultiplyFloat:
#endif
preserve_caller_vectors
.loop_mf_begin:
ld4.4s {v8-v11}, [x1], 64
ld4.4s {v12-v15}, [x2], 64
ld4.4s {v16-v19}, [x1], 64
ld4.4s {v20-v23}, [x2], 64
fmul.4s    v0, v8, v12
fmul.4s    v1, v9, v12
fmul.4s    v2, v10, v14
fmul.4s    v3, v11, v14
fmul.4s    v4, v16, v20
fmul.4s    v5, v17, v20
fmul.4s    v6, v18, v22
fmul.4s    v7, v19, v22
fmls.4s    v0, v9, v13
fmla.4s    v1, v8, v13
fmls.4s    v2, v11, v15
fmla.4s    v3, v10, v15
fmls.4s    v4, v17, v21
fmla.4s    v5, v16, v21
fmls.4s    v6, v19, v23
fmla.4s    v7, v18, v23
st4.4s     {v0-v3}, [x0], 64
st4.4s     {v4-v7}, [x0], 64
subs        x3, x3, 16
bne         .loop_mf_begin
restore_caller_vectors
ret lr

#if defined(__APPLE__)
_complexMultiplyAccumFloat:
#else
complexMultiplyAccumFloat:
#endif
preserve_caller_vectors
.loop_macf_begin:
ld4.4s {v0-v3},[x1], 64
ld4.4s {v4-v7},[x1], 64
ld4.4s {v8-v11}, [x2], 64
ld4.4s {v12-v15}, [x3], 64
ld4.4s {v16-v19}, [x2], 64
ld4.4s {v20-v23}, [x3], 64
fmla.4s    v0, v8, v12
fmla.4s    v1, v9, v12
fmla.4s    v2, v10, v14
fmla.4s    v3, v11, v14
fmla.4s    v4, v16, v20
fmla.4s    v5, v17, v20
fmla.4s    v6, v18, v22
fmla.4s    v7, v19, v22
fmls.4s    v0, v9, v13
fmla.4s    v1, v8, v13
fmls.4s    v2, v11, v15
fmla.4s    v3, v10, v15
fmls.4s    v4, v17, v21
fmla.4s    v5, v16, v21
fmls.4s    v6, v19, v23
fmla.4s    v7, v18, v23
st4.4s     {v0-v3}, [x0], 64
st4.4s     {v4-v7}, [x0], 64
subs        x4, x4, 16
bne         .loop_macf_begin
restore_caller_vectors
ret lr