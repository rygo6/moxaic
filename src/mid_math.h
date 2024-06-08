#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define MID_PI 3.14159265358979323846f

#define SIMD_TYPE(type, name, count) typedef type name##_vec __attribute__((vector_size(sizeof(type) * count)))
SIMD_TYPE(float, float2, 2);
SIMD_TYPE(uint32_t, int2, 2);
SIMD_TYPE(float, float3, 4);
SIMD_TYPE(uint32_t, int3, 4);
SIMD_TYPE(float, float4, 4);
SIMD_TYPE(uint32_t, int4, 4);
SIMD_TYPE(float, mat4, 16);
#undef SIMD_TYPE
// should I rename simd to vec and get rid of vec_name?
#define VEC_UNION(type, simd_type, name, align, count, ...) \
  typedef union __attribute((aligned(align))) name {        \
    simd_type vec;                                          \
    struct {                                                \
      type __VA_ARGS__;                                     \
    };                                                      \
  } name;
VEC_UNION(float, float2_vec, vec2, 8, 2, x, y);
VEC_UNION(uint32_t, int2_vec, ivec2, 8, 2, x, y);
VEC_UNION(float, float3_vec, vec3, 16, 3, x, y, z);
VEC_UNION(uint32_t, int3_vec, ivec3, 16, 3, x, y, z);
VEC_UNION(float, float4_vec, vec4, 16, 4, x, y, z, w);
VEC_UNION(uint32_t, int4_vec, ivec4, 16, 4, x, y, z, w);
#undef VEC_UNION

#define MAT_UNION(type, simd_type, name, align, count, vec_name, ...) \
  typedef union __attribute((aligned(align))) name {                  \
    simd_type vec;                                                    \
    type      vec_name[count];                                        \
    struct {                                                          \
      type __VA_ARGS__;                                               \
    };                                                                \
  } name;
MAT_UNION(float, float4_vec, mat4_row, 16, 4, row, r0, r1, r2, r3);
MAT_UNION(mat4_row, mat4_vec, mat4, 16, 4, col, c0, c1, c2, c3);
#undef MAT_UNION

typedef vec4 quat;

#define VEC_NIL -1
enum VecComponents {
  X,
  Y,
  Z,
  W,
  VEC_COUNT,
};
#define _X .vec[X]
#define _Y .vec[Y]
#define _Z .vec[Z]
#define _W .vec[W]
#define _0 .vec[0]
#define _1 .vec[1]
#define _2 .vec[2]
#define _3 .vec[3]
enum MatComponents {
  C0_R0,
  C0_R1,
  C0_R2,
  C0_R3,
  C1_R0,
  C1_R1,
  C1_R2,
  C1_R3,
  C2_R0,
  C2_R1,
  C2_R2,
  C2_R3,
  C3_R0,
  C3_R1,
  C3_R2,
  C3_R3,
  MAT_COUNT,
};
#define _00 .vec[C0_R0]
#define _01 .vec[C0_R1]
#define _02 .vec[C0_R2]
#define _03 .vec[C0_R3]
#define _10 .vec[C1_R0]
#define _11 .vec[C1_R1]
#define _12 .vec[C1_R2]
#define _13 .vec[C1_R3]
#define _20 .vec[C2_R0]
#define _21 .vec[C2_R1]
#define _22 .vec[C2_R2]
#define _23 .vec[C2_R3]
#define _30 .vec[C3_R0]
#define _31 .vec[C3_R1]
#define _32 .vec[C3_R2]
#define _33 .vec[C3_R3]
static const vec4 VEC4_ZERO = {0.0f, 0.0f, 0.0f, 0.0f};
static const vec4 VEC4_IDENT = {0.0f, 0.0f, 0.0f, 1.0f};
static const quat QUAT_IDENT = {0.0f, 0.0f, 0.0f, 1.0f};
static const mat4 MAT4_IDENT = {
    .c0 = {1.0f, 0.0f, 0.0f, 0.0f},
    .c1 = {0.0f, 1.0f, 0.0f, 0.0f},
    .c2 = {0.0f, 0.0f, 1.0f, 0.0f},
    .c3 = {0.0f, 0.0f, 0.0f, 1.0f},
};

// force inlining appears to produce same assembly for returning or by passing out pointer
// https://godbolt.org/z/rzo8hEaaG
// https://godbolt.org/z/149rjsPG3
#define MATH_INLINE       __attribute__((always_inline)) static inline
#define SHUFFLE(vec, ...) __builtin_shufflevector(vec, vec, __VA_ARGS__)

MATH_INLINE float Float4Sum(float4_vec float4) {
  // appears to make better SIMD assembly than a loop:
  // https://godbolt.org/z/6jWe4Pj5b
  // https://godbolt.org/z/M5Goq57vj
  float4_vec shuf = SHUFFLE(float4, 2, 3, 0, 1);
  float4_vec sums = float4 + shuf;
  shuf = SHUFFLE(sums, 1, 0, 3, 2);
  sums = sums + shuf;
  return sums[0];
}
MATH_INLINE mat4 Mat4Translation(const vec3 v) {
  mat4 out = MAT4_IDENT;
  for (int i = 0; i < 3; ++i) out.col[3].vec += MAT4_IDENT.col[i].vec * v.vec[i];
  return out;
}
MATH_INLINE float Vec4Dot(const vec4 l, const vec4 r) {
  float4_vec product = l.vec * r.vec;
  return Float4Sum(product);
}
MATH_INLINE float Vec4Mag(const vec4 v) {
  return sqrtf(Vec4Dot(v, v));
}
MATH_INLINE mat4 QuatToMat4(const quat q) {
  const float norm = Vec4Mag(q);
  const float s = norm > 0.0f ? 2.0f / norm : 0.0f;

  const float3_vec xxw = SHUFFLE(q.vec, X, X, W, VEC_NIL);
  const float3_vec xyx = SHUFFLE(q.vec, X, Y, X, VEC_NIL);
  const float3_vec xx_xy_wx = s * xxw * xyx;

  const float3_vec yyw = SHUFFLE(q.vec, Y, Y, W, VEC_NIL);
  const float3_vec yzy = SHUFFLE(q.vec, Y, Z, Y, VEC_NIL);
  const float3_vec yy_yz_wy = s * yyw * yzy;

  const float3_vec zxw = SHUFFLE(q.vec, Z, X, W, VEC_NIL);
  const float3_vec zzz = SHUFFLE(q.vec, Z, Z, Z, VEC_NIL);
  const float3_vec zz_xz_wz = s * zxw * zzz;

  // Setting to specific SIMD indices produces same SIMD assembly as long as target
  // SIMD vec is same size as source vecs https://godbolt.org/z/qqMMa4v3G
  mat4 out = MAT4_IDENT;
  out.c0.vec[0] = 1.0f - yy_yz_wy[X] - zz_xz_wz[X];
  out.c1.vec[1] = 1.0f - xx_xy_wx[X] - zz_xz_wz[X];
  out.c2.vec[2] = 1.0f - xx_xy_wx[X] - yy_yz_wy[X];

  out.c0.vec[1] = xx_xy_wx[Y] + zz_xz_wz[Z];
  out.c1.vec[2] = yy_yz_wy[Y] + xx_xy_wx[Z];
  out.c2.vec[0] = zz_xz_wz[Y] + yy_yz_wy[Z];

  out.c1.vec[0] = xx_xy_wx[Y] - zz_xz_wz[Z];
  out.c2.vec[1] = yy_yz_wy[Y] - xx_xy_wx[Z];
  out.c0.vec[2] = zz_xz_wz[Y] - yy_yz_wy[Z];

  out.c0.vec[3] = 0.0f;
  out.c1.vec[3] = 0.0f;
  out.c2.vec[3] = 0.0f;
  out.c3.vec[0] = 0.0f;
  out.c3.vec[1] = 0.0f;
  out.c3.vec[2] = 0.0f;
  out.c3.vec[3] = 1.0f;
  return out;
}

// this isn't actually faster... figure out... https://godbolt.org/z/3zodnr6bj
MATH_INLINE mat4 Mat4Mul(const mat4 l, const mat4 r) {
  return (mat4){.vec = SHUFFLE(l.vec,
                               C0_R0, C0_R1, C0_R2, C0_R3,
                               C0_R0, C0_R1, C0_R2, C0_R3,
                               C0_R0, C0_R1, C0_R2, C0_R3,
                               C0_R0, C0_R1, C0_R2, C0_R3) *
                           SHUFFLE(r.vec,
                                   C0_R0, C0_R0, C0_R0, C0_R0,
                                   C1_R0, C1_R0, C1_R0, C1_R0,
                                   C2_R0, C2_R0, C2_R0, C2_R0,
                                   C3_R0, C3_R0, C3_R0, C3_R0) +
                       SHUFFLE(l.vec,
                               C1_R0, C1_R1, C1_R2, C1_R3,
                               C1_R0, C1_R1, C1_R2, C1_R3,
                               C1_R0, C1_R1, C1_R2, C1_R3,
                               C1_R0, C1_R1, C1_R2, C1_R3) *
                           SHUFFLE(r.vec,
                                   C0_R1, C0_R1, C0_R1, C0_R1,
                                   C1_R1, C1_R1, C1_R1, C1_R1,
                                   C2_R1, C2_R1, C2_R1, C2_R1,
                                   C3_R1, C3_R1, C3_R1, C3_R1) +
                       SHUFFLE(l.vec,
                               C2_R0, C2_R1, C2_R2, C2_R3,
                               C2_R0, C2_R1, C2_R2, C2_R3,
                               C2_R0, C2_R1, C2_R2, C2_R3,
                               C2_R0, C2_R1, C2_R2, C2_R3) *
                           SHUFFLE(r.vec,
                                   C0_R2, C0_R2, C0_R2, C0_R2,
                                   C1_R2, C1_R2, C1_R2, C1_R2,
                                   C2_R2, C2_R2, C2_R2, C2_R2,
                                   C3_R2, C3_R2, C3_R2, C3_R2) +
                       SHUFFLE(l.vec,
                               C3_R0, C3_R1, C3_R2, C3_R3,
                               C3_R0, C3_R1, C3_R2, C3_R3,
                               C3_R0, C3_R1, C3_R2, C3_R3,
                               C3_R0, C3_R1, C3_R2, C3_R3) *
                           SHUFFLE(r.vec,
                                   C0_R3, C0_R3, C0_R3, C0_R3,
                                   C1_R3, C1_R3, C1_R3, C1_R3,
                                   C2_R3, C2_R3, C2_R3, C2_R3,
                                   C3_R3, C3_R3, C3_R3, C3_R3)};
}
MATH_INLINE mat4 Mat4MulNoSimd(const mat4 l, const mat4 r) {
  float a00 = l.col[0].row[0], a01 = l.col[0].row[1], a02 = l.col[0].row[2], a03 = l.col[0].row[3],
        a10 = l.col[1].row[0], a11 = l.col[1].row[1], a12 = l.col[1].row[2], a13 = l.col[1].row[3],
        a20 = l.col[2].row[0], a21 = l.col[2].row[1], a22 = l.col[2].row[2], a23 = l.col[2].row[3],
        a30 = l.col[3].row[0], a31 = l.col[3].row[1], a32 = l.col[3].row[2], a33 = l.col[3].row[3],

        b00 = r.col[0].row[0], b01 = r.col[0].row[1], b02 = r.col[0].row[2], b03 = r.col[0].row[3],
        b10 = r.col[1].row[0], b11 = r.col[1].row[1], b12 = r.col[1].row[2], b13 = r.col[1].row[3],
        b20 = r.col[2].row[0], b21 = r.col[2].row[1], b22 = r.col[2].row[2], b23 = r.col[2].row[3],
        b30 = r.col[3].row[0], b31 = r.col[3].row[1], b32 = r.col[3].row[2], b33 = r.col[3].row[3];

  mat4 dest = MAT4_IDENT;
  dest.col[0].row[0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
  dest.col[0].row[1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
  dest.col[0].row[2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
  dest.col[0].row[3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
  dest.col[1].row[0] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
  dest.col[1].row[1] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
  dest.col[1].row[2] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
  dest.col[1].row[3] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
  dest.col[2].row[0] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
  dest.col[2].row[1] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
  dest.col[2].row[2] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
  dest.col[2].row[3] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
  dest.col[3].row[0] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
  dest.col[3].row[1] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
  dest.col[3].row[2] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
  dest.col[3].row[3] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
  return dest;
}

MATH_INLINE mat4 Mat4Inv(const mat4 src) {
  // todo SIMDIZE
  float t[6];
  float det;
  float a = src.c0.r0, b = src.c0.r1, c = src.c0.r2, d = src.c0.r3,
        e = src.c1.r0, f = src.c1.r1, g = src.c1.r2, h = src.c1.r3,
        i = src.c2.r0, j = src.c2.r1, k = src.c2.r2, l = src.c2.r3,
        m = src.c3.r0, n = src.c3.r1, o = src.c3.r2, p = src.c3.r3;

  t[0] = k * p - o * l;
  t[1] = j * p - n * l;
  t[2] = j * o - n * k;
  t[3] = i * p - m * l;
  t[4] = i * o - m * k;
  t[5] = i * n - m * j;

  mat4 out;

  out.c0.r0 = f * t[0] - g * t[1] + h * t[2];
  out.c1.r0 = -(e * t[0] - g * t[3] + h * t[4]);
  out.c2.r0 = e * t[1] - f * t[3] + h * t[5];
  out.c3.r0 = -(e * t[2] - f * t[4] + g * t[5]);

  out.c0.r1 = -(b * t[0] - c * t[1] + d * t[2]);
  out.c1.r1 = a * t[0] - c * t[3] + d * t[4];
  out.c2.r1 = -(a * t[1] - b * t[3] + d * t[5]);
  out.c3.r1 = a * t[2] - b * t[4] + c * t[5];

  t[0] = g * p - o * h;
  t[1] = f * p - n * h;
  t[2] = f * o - n * g;
  t[3] = e * p - m * h;
  t[4] = e * o - m * g;
  t[5] = e * n - m * f;

  out.c0.r2 = b * t[0] - c * t[1] + d * t[2];
  out.c1.r2 = -(a * t[0] - c * t[3] + d * t[4]);
  out.c2.r2 = a * t[1] - b * t[3] + d * t[5];
  out.c3.r2 = -(a * t[2] - b * t[4] + c * t[5]);

  t[0] = g * l - k * h;
  t[1] = f * l - j * h;
  t[2] = f * k - j * g;
  t[3] = e * l - i * h;
  t[4] = e * k - i * g;
  t[5] = e * j - i * f;

  out.c0.r3 = -(b * t[0] - c * t[1] + d * t[2]);
  out.c1.r3 = a * t[0] - c * t[3] + d * t[4];
  out.c2.r3 = -(a * t[1] - b * t[3] + d * t[5]);
  out.c3.r3 = a * t[2] - b * t[4] + c * t[5];

  det = 1.0f / (a * out.c0.r0 + b * out.c1.r0 + c * out.c2.r0 + d * out.c3.r0);

  out.vec *= det;

  return out;
}
MATH_INLINE mat4 Mat4FromTransform(const vec3 pos, const quat rot) {
  const mat4 translationMat4 = Mat4Translation(pos);
  const mat4 rotationMat4 = QuatToMat4(rot);
  return Mat4Mul(translationMat4, rotationMat4);
}
// should I do this?
typedef struct mat4_proj_packed {
  float c0r0;
  float c1r1;
  float c2r2;
  float c2r3;
  float c3r2;
} mat4_proj_packed;
// Perspective matrix in Vulkan Reverse Z
MATH_INLINE void vkmMat4Perspective(const float fov, const float aspect, const float zNear, const float zFar, mat4* pDstMat4) {
  const float tanHalfFovy = tan(fov / 2.0f);
  *pDstMat4 = (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfFovy)},
                     .c1 = {.r1 = 1.0f / tanHalfFovy},
                     .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
                     .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}
MATH_INLINE vec3 Vec3Cross(const float3_vec l, const float3_vec r) {
  return (vec3){l[Y] * r[Z] - r[Y] * l[Z],
                l[Z] * r[X] - r[Z] * l[X],
                l[X] * r[Y] - r[X] * l[Y]};
}
MATH_INLINE vec3 Vec3Rot(const quat q, const vec3 v) {
  const vec3 uv = Vec3Cross(q.vec, v.vec);
  const vec3 uuv = Vec3Cross(q.vec, uv.vec);
  vec3       out;
  for (int i = 0; i < 3; ++i) out.vec[i] = v.vec[i] + ((uv.vec[i] * q.vec[W]) + uuv.vec[i]) * 2.0f;
  return out;
}
MATH_INLINE vec4 Vec4Rot(const quat q, const vec4 v) {
  const vec3 uv = Vec3Cross(q.vec, v.vec);
  const vec3 uuv = Vec3Cross(q.vec, uv.vec);
  vec4       out = {.w = v.w};
  for (int i = 0; i < 3; ++i) out.vec[i] = v.vec[i] + ((uv.vec[i] * q.vec[W]) + uuv.vec[i]) * 2.0f;
  return out;
}
MATH_INLINE quat QuatFromEuler(const vec3 euler) {
  vec3 c, s;
  for (int i = 0; i < 3; ++i) {
    c.vec[i] = cos(euler.vec[i] * 0.5f);
    s.vec[i] = sin(euler.vec[i] * 0.5f);
  }
  // todo SIMDIZE
  quat out;
  out _W = c _X * c _Y * c _Z + s _X * s _Y * s _Z;
  out _X = s _X * c _Y * c _Z - c _X * s _Y * s _Z;
  out _Y = c _X * s _Y * c _Z + s _X * c _Y * s _Z;
  out _Z = c _X * c _Y * s _Z - s _X * s _Y * c _Z;
  return out;
}
MATH_INLINE void QuatMul(const quat* pSrc, const quat* pMul, quat* pDst) {
  // todo SIMDIZE
  pDst->w = pSrc->w * pMul->w - pSrc->x * pMul->x - pSrc->y * pMul->y - pSrc->z * pMul->z;
  pDst->x = pSrc->w * pMul->x + pSrc->x * pMul->w + pSrc->y * pMul->z - pSrc->z * pMul->y;
  pDst->y = pSrc->w * pMul->y + pSrc->y * pMul->w + pSrc->z * pMul->x - pSrc->x * pMul->z;
  pDst->z = pSrc->w * pMul->z + pSrc->z * pMul->w + pSrc->x * pMul->y - pSrc->y * pMul->x;
}
MATH_INLINE vec4 Vec4MulMat4(const mat4 m, const vec4 v) {
  // todo SIMDIZE
  vec4                   out;
  out.vec[X] = m _00 * v _X + m _10 * v _Y + m _20 * v _Z + m _30 * v _W;
  out.vec[Y] = m _01 * v _X + m _11 * v _Y + m _21 * v _Z + m _31 * v _W;
  out.vec[Z] = m _02 * v _X + m _12 * v _Y + m _22 * v _Z + m _32 * v _W;
  out.vec[W] = m _03 * v _X + m _13 * v _Y + m _23 * v _Z + m _33 * v _W;
  return out;
}
MATH_INLINE vec3 Vec4WDivide(const vec4 v) {
  return (vec3){.vec = v.vec / v.vec[W]};
}
MATH_INLINE vec2 Vec2FromVec3(const vec3 ndc) {
  return (vec2){.x = ndc.x, .y = ndc.y};
}
MATH_INLINE vec2 UVFromNDC(const vec3 ndc) {
  vec2 out = Vec2FromVec3(ndc);
  out.vec = out.vec * 0.5f + 0.5f;
  return out;
}
MATH_INLINE ivec2 iVec2CeiDivide(const ivec2 v, int d) {
  vec2 fv = {.vec = {v.vec[X], v.vec[Y]}};
  fv.vec = fv.vec / (float)d;
  return (ivec2){ceilf(fv.vec[X]), ceilf(fv.vec[Y])};
}
MATH_INLINE ivec2 iVec2ShiftRightCeil(const ivec2 v, const int shift) {
  if (shift <= 0) return v;
  return (ivec2){(v.vec >> shift) + (v.vec % 2)};
}
MATH_INLINE ivec2 iVec2Min(const ivec2 v, const int min) {
  ivec2 out;
  for (int i = 0; i < 2; ++i) out.vec[i] = v.vec[i] < min ? min : v.vec[i];
  return out;
}
MATH_INLINE vec2 Vec2Clamp(const vec2 v, const float min, const float max) {
  const float2_vec minVec = {min, min};
  const float2_vec maxVec = {max, max};
  vec2             clamped = v;
  for (int i = 0; i < 2; ++i) {
    clamped.vec[i] = (v.vec[i] < minVec[i]) ? minVec[i] : v.vec[i];
    clamped.vec[i] = (clamped.vec[i] > maxVec[i]) ? maxVec[i] : clamped.vec[i];
  }
  return clamped;
}
MATH_INLINE void Mat4Print(const mat4 m) {
  printf("mat4:\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n",
         m.c0.r0, m.c1.r0, m.c2.r0, m.c3.r0,
         m.c0.r1, m.c1.r1, m.c2.r1, m.c3.r1,
         m.c0.r2, m.c1.r2, m.c2.r2, m.c3.r2,
         m.c0.r3, m.c1.r3, m.c2.r3, m.c3.r3);
}

#undef MATH_INLINE
#undef SHUFFLE