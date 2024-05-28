#pragma once

#include <math.h>
#include <stdint.h>

#define VKM_PI 3.14159265358979323846f

#define MID_SIMD_TYPE(type, name, count) typedef type name##_simd __attribute__((vector_size(sizeof(type) * count)))
MID_SIMD_TYPE(float, float2, 2);
MID_SIMD_TYPE(uint32_t, int2, 2);
MID_SIMD_TYPE(float, float3, 4);
MID_SIMD_TYPE(uint32_t, int3, 4);
MID_SIMD_TYPE(float, float4, 4);
MID_SIMD_TYPE(uint32_t, int4, 4);
MID_SIMD_TYPE(float, mat4, 16);
#undef MID_SIMD_TYPE
// should I rename simd to vec and get rid of vec_name?
#define MID_MATH_UNION(type, simd_type, name, align, count, vec_name, ...) \
  typedef union __attribute((aligned(align))) name {                       \
    type vec_name[count];                                                  \
    struct {                                                               \
      type __VA_ARGS__;                                                    \
    };                                                                     \
    simd_type simd;                                                        \
  } name;
MID_MATH_UNION(float, float2_simd, vec2, 8, 2, vec, x, y);
MID_MATH_UNION(uint32_t, int2_simd, ivec2, 16, 2, vec, x, y);
MID_MATH_UNION(float, float3_simd, vec3, 16, 3, vec, x, y, z);
MID_MATH_UNION(uint32_t, int3_simd, ivec3, 16, 3, vec, x, y, z);
MID_MATH_UNION(float, float4_simd, vec4, 16, 4, vec, x, y, z, w);
MID_MATH_UNION(uint32_t, int4_simd, ivec4, 16, 4, vec, x, y, z, w);
MID_MATH_UNION(float, float4_simd, mat4_row, 16, 4, row, r0, r1, r2, r3);
MID_MATH_UNION(mat4_row, mat4_simd, mat4, 16, 4, col, c0, c1, c2, c3);
#undef MID_MATH_UNION
typedef vec4 quat;

#define VEC_NIL -1
enum VecElement {
  VEC_X,
  VEC_Y,
  VEC_Z,
  VEC_W,
  VEC_COUNT,
};
enum MatElement {
  MAT_C0_R0,
  MAT_C0_R1,
  MAT_C0_R2,
  MAT_C0_R3,
  MAT_C1_R0,
  MAT_C1_R1,
  MAT_C1_R2,
  MAT_C1_R3,
  MAT_C2_R0,
  MAT_C2_R1,
  MAT_C2_R2,
  MAT_C2_R3,
  MAT_C3_R0,
  MAT_C3_R1,
  MAT_C3_R2,
  MAT_C3_R3,
  MAT_COUNT,
};
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

MATH_INLINE float Float4Sum(float4_simd float4) {
  // appears to make better SIMD assembly than a loop:
  // https://godbolt.org/z/6jWe4Pj5b
  // https://godbolt.org/z/M5Goq57vj
  float4_simd shuf = SHUFFLE(float4, 2, 3, 0, 1);
  float4_simd sums = float4 + shuf;
  shuf = SHUFFLE(sums, 1, 0, 3, 2);
  sums = sums + shuf;
  return sums[0];
}
MATH_INLINE void Mat4Translation(const vec3 v, mat4* pDst) {
  for (int i = 0; i < 3; ++i) pDst->col[3].simd += MAT4_IDENT.col[i].simd * v.simd[i];
}
MATH_INLINE float Vec4Dot(const vec4 l, const vec4 r) {
  float4_simd product = l.simd * r.simd;
  return Float4Sum(product);
}
MATH_INLINE float Vec4Mag(const vec4 v) {
  return sqrtf(Vec4Dot(v, v));
}
MATH_INLINE void QuatToMat4(const quat q, mat4* pDst) {
  const float norm = Vec4Mag(q);
  const float s = norm > 0.0f ? 2.0f / norm : 0.0f;

  const float3_simd xxw = SHUFFLE(q.simd, VEC_X, VEC_X, VEC_W, VEC_NIL);
  const float3_simd xyx = SHUFFLE(q.simd, VEC_X, VEC_Y, VEC_X, VEC_NIL);
  const float3_simd xx_xy_wx = s * xxw * xyx;

  const float3_simd yyw = SHUFFLE(q.simd, VEC_Y, VEC_Y, VEC_W, VEC_NIL);
  const float3_simd yzy = SHUFFLE(q.simd, VEC_Y, VEC_Z, VEC_Y, VEC_NIL);
  const float3_simd yy_yz_wy = s * yyw * yzy;

  const float3_simd zxw = SHUFFLE(q.simd, VEC_Z, VEC_X, VEC_W, VEC_NIL);
  const float3_simd zzz = SHUFFLE(q.simd, VEC_Z, VEC_Z, VEC_Z, VEC_NIL);
  const float3_simd zz_xz_wz = s * zxw * zzz;

  // Setting to specific SIMD indices produces same SIMD assembly as long as target
  // SIMD vec is same size as source vecs https://godbolt.org/z/qqMMa4v3G
  pDst->c0.simd[0] = 1.0f - yy_yz_wy[VEC_X] - zz_xz_wz[VEC_X];
  pDst->c1.simd[1] = 1.0f - xx_xy_wx[VEC_X] - zz_xz_wz[VEC_X];
  pDst->c2.simd[2] = 1.0f - xx_xy_wx[VEC_X] - yy_yz_wy[VEC_X];

  pDst->c0.simd[1] = xx_xy_wx[VEC_Y] + zz_xz_wz[VEC_Z];
  pDst->c1.simd[2] = yy_yz_wy[VEC_Y] + xx_xy_wx[VEC_Z];
  pDst->c2.simd[0] = zz_xz_wz[VEC_Y] + yy_yz_wy[VEC_Z];

  pDst->c1.simd[0] = xx_xy_wx[VEC_Y] - zz_xz_wz[VEC_Z];
  pDst->c2.simd[1] = yy_yz_wy[VEC_Y] - xx_xy_wx[VEC_Z];
  pDst->c0.simd[2] = zz_xz_wz[VEC_Y] - yy_yz_wy[VEC_Z];

  pDst->c0.simd[3] = 0.0f;
  pDst->c1.simd[3] = 0.0f;
  pDst->c2.simd[3] = 0.0f;
  pDst->c3.simd[0] = 0.0f;
  pDst->c3.simd[1] = 0.0f;
  pDst->c3.simd[2] = 0.0f;
  pDst->c3.simd[3] = 1.0f;
}
MATH_INLINE mat4 Mat4Mul(const mat4 l, const mat4 r) {
  return (mat4){.simd = SHUFFLE(l.simd,
                                MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3) *
                            SHUFFLE(r.simd,
                                    MAT_C0_R0, MAT_C0_R0, MAT_C0_R0, MAT_C0_R0,
                                    MAT_C1_R0, MAT_C1_R0, MAT_C1_R0, MAT_C1_R0,
                                    MAT_C2_R0, MAT_C2_R0, MAT_C2_R0, MAT_C2_R0,
                                    MAT_C3_R0, MAT_C3_R0, MAT_C3_R0, MAT_C3_R0) +
                        SHUFFLE(l.simd,
                                MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3) *
                            SHUFFLE(r.simd,
                                    MAT_C0_R1, MAT_C0_R1, MAT_C0_R1, MAT_C0_R1,
                                    MAT_C1_R1, MAT_C1_R1, MAT_C1_R1, MAT_C1_R1,
                                    MAT_C2_R1, MAT_C2_R1, MAT_C2_R1, MAT_C2_R1,
                                    MAT_C3_R1, MAT_C3_R1, MAT_C3_R1, MAT_C3_R1) +
                        SHUFFLE(l.simd,
                                MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3) *
                            SHUFFLE(r.simd,
                                    MAT_C0_R2, MAT_C0_R2, MAT_C0_R2, MAT_C0_R2,
                                    MAT_C1_R2, MAT_C1_R2, MAT_C1_R2, MAT_C1_R2,
                                    MAT_C2_R2, MAT_C2_R2, MAT_C2_R2, MAT_C2_R2,
                                    MAT_C3_R2, MAT_C3_R2, MAT_C3_R2, MAT_C3_R2) +
                        SHUFFLE(l.simd,
                                MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3) *
                            SHUFFLE(r.simd,
                                    MAT_C0_R3, MAT_C0_R3, MAT_C0_R3, MAT_C0_R3,
                                    MAT_C1_R3, MAT_C1_R3, MAT_C1_R3, MAT_C1_R3,
                                    MAT_C2_R3, MAT_C2_R3, MAT_C2_R3, MAT_C2_R3,
                                    MAT_C3_R3, MAT_C3_R3, MAT_C3_R3, MAT_C3_R3)};
}
MATH_INLINE mat4 Mat4Inv(const mat4* pSrc) {
  // todo SIMDIZE
  float t[6];
  float det;
  float a = pSrc->c0.r0, b = pSrc->c0.r1, c = pSrc->c0.r2, d = pSrc->c0.r3,
        e = pSrc->c1.r0, f = pSrc->c1.r1, g = pSrc->c1.r2, h = pSrc->c1.r3,
        i = pSrc->c2.r0, j = pSrc->c2.r1, k = pSrc->c2.r2, l = pSrc->c2.r3,
        m = pSrc->c3.r0, n = pSrc->c3.r1, o = pSrc->c3.r2, p = pSrc->c3.r3;

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

  out.simd *= det;
}
MATH_INLINE mat4 Mat4FromTransform(const vec3 pos, const quat rot) {
  mat4 translationMat4 = MAT4_IDENT;
  Mat4Translation(pos, &translationMat4);
  mat4 rotationMat4 = MAT4_IDENT;
  QuatToMat4(rot, &rotationMat4);
  return Mat4Mul(translationMat4, rotationMat4);
}
// Perspective matrix in Vulkan Reverse Z
MATH_INLINE void vkmMat4Perspective(const float fov, const float aspect, const float zNear, const float zFar, mat4* pDstMat4) {
  const float tanHalfFovy = tan(fov / 2.0f);
  *pDstMat4 = (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfFovy)},
                     .c1 = {.r1 = 1.0f / tanHalfFovy},
                     .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
                     .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}
MATH_INLINE vec3 Vec3Cross(const float3_simd l, const float3_simd r) {
  return (vec3){l[VEC_Y] * r[VEC_Z] - r[VEC_Y] * l[VEC_Z],
                l[VEC_Z] * r[VEC_X] - r[VEC_Z] * l[VEC_X],
                l[VEC_X] * r[VEC_Y] - r[VEC_X] * l[VEC_Y]};
}
MATH_INLINE vec3 Vec3Rot(const quat q, const vec3 v) {
  const vec3 uv = Vec3Cross(q.simd, v.simd);
  const vec3 uuv = Vec3Cross(q.simd, uv.simd);
  vec3       out;
  for (int i = 0; i < 3; ++i) out.simd[i] = v.simd[i] + ((uv.simd[i] * q.simd[VEC_W]) + uuv.simd[i]) * 2.0f;
  return out;
}
MATH_INLINE vec4 Vec4Rot(const quat q, const vec4 v) {
  const vec3 uv = Vec3Cross(q.simd, v.simd);
  const vec3 uuv = Vec3Cross(q.simd, uv.simd);
  vec4       out = {.w = v.w};
  for (int i = 0; i < 3; ++i) out.simd[i] = v.simd[i] + ((uv.simd[i] * q.simd[VEC_W]) + uuv.simd[i]) * 2.0f;
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
  out.w = c.x * c.y * c.z + s.x * s.y * s.z;
  out.x = s.x * c.y * c.z - c.x * s.y * s.z;
  out.y = c.x * s.y * c.z + s.x * c.y * s.z;
  out.z = c.x * c.y * s.z - s.x * s.y * c.z;
  return out;
}
MATH_INLINE void vkmQuatMul(const quat* pSrc, const quat* pMul, quat* pDst) {
  // todo SIMDIZE
  pDst->w = pSrc->w * pMul->w - pSrc->x * pMul->x - pSrc->y * pMul->y - pSrc->z * pMul->z;
  pDst->x = pSrc->w * pMul->x + pSrc->x * pMul->w + pSrc->y * pMul->z - pSrc->z * pMul->y;
  pDst->y = pSrc->w * pMul->y + pSrc->y * pMul->w + pSrc->z * pMul->x - pSrc->x * pMul->z;
  pDst->z = pSrc->w * pMul->z + pSrc->z * pMul->w + pSrc->x * pMul->y - pSrc->y * pMul->x;
}
MATH_INLINE vec4 Vec4MulMat4(const mat4 m, const vec4 v) {
  // todo SIMDIZE
  vec4 out;
  out.simd[0] = m.simd[MAT_C0_R0] * v.simd[0] + m.simd[MAT_C1_R0] * v.simd[1] + m.simd[MAT_C2_R0] * v.simd[2] + m.simd[MAT_C3_R0] * v.simd[3];
  out.simd[1] = m.simd[MAT_C0_R1] * v.simd[0] + m.simd[MAT_C1_R1] * v.simd[1] + m.simd[MAT_C2_R1] * v.simd[2] + m.simd[MAT_C3_R1] * v.simd[3];
  out.simd[2] = m.simd[MAT_C0_R2] * v.simd[0] + m.simd[MAT_C1_R2] * v.simd[1] + m.simd[MAT_C2_R2] * v.simd[2] + m.simd[MAT_C3_R2] * v.simd[3];
  out.simd[3] = m.simd[MAT_C0_R3] * v.simd[0] + m.simd[MAT_C1_R3] * v.simd[1] + m.simd[MAT_C2_R3] * v.simd[2] + m.simd[MAT_C3_R3] * v.simd[3];
  return out;
}
MATH_INLINE vec3 Vec4WDivide(const vec4 v) {
  return (vec3){.simd = v.simd / v.simd[VEC_W]};
}
MATH_INLINE vec2 Vec2FromVec3(const vec3 ndc) {
  return (vec2){.x = ndc.x, .y = ndc.y};
}
MATH_INLINE vec2 UVFromNDC(const vec3 ndc) {
  vec2 out = Vec2FromVec3(ndc);
  out.simd = out.simd * 0.5f + 0.5f;
  return out;
}

#undef MATH_INLINE
#undef SHUFFLE