////////////////////
//// Mid Math Header
////////////////////
#pragma once

#include "mid_common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define PI                 3.14159265358979323846f
#define RAD_FROM_DEG(_deg) (_deg * (PI / 180.0f))
#define DEG_FROM_RAD(_rad) (_rad * (180.0f / PI))

#define SIMD_TYPE(type, name, count) typedef type name##_vec __attribute__((vector_size(sizeof(type) * count)))
SIMD_TYPE(float, float2, 2);
SIMD_TYPE(uint32_t, int2, 2);
SIMD_TYPE(float, float3, 4);
SIMD_TYPE(uint32_t, int3, 4);
SIMD_TYPE(float, float4, 4);
SIMD_TYPE(uint32_t, int4, 4);
// basing it off a 64 byte simd type can offer small perf gain
// with no negative when accessing 16 byte rows
// https://godbolt.org/z/Gbssve8rf
SIMD_TYPE(float, mat4, 16);
#undef SIMD_TYPE
// should I rename simd to vec and get rid of vec_name?
#define VEC_UNION(name, type, simd_type, align, count, vec_name, ...) \
	typedef union __attribute((aligned(align))) name {                \
		simd_type vec_name;                                           \
		struct {                                                      \
			type __VA_ARGS__;                                         \
		};                                                            \
	} name;
VEC_UNION(vec2, float, float2_vec, 8, 2, vec, x, y)
VEC_UNION(ivec2, uint32_t, int2_vec, 8, 2, vec, x, y)
VEC_UNION(vec3, float, float3_vec, 16, 3, vec, x, y, z)
VEC_UNION(ivec3, uint32_t, int3_vec, 16, 3, vec, x, y, z)
VEC_UNION(vec4, float, float4_vec, 16, 4, vec, x, y, z, w)
VEC_UNION(ivec4, uint32_t, int4_vec, 16, 4, vec, x, y, z, w)
VEC_UNION(mat4_row, float, float4_vec, 16, 4, row, r0, r1, r2, r3)
#undef VEC_UNION
#define VEC2(x, y) (vec2) {{x, y}}
#define IVEC2(x, y) (ivec2) {{x, y}}
#define VEC3(x, y, z) (vec3) {{x, y, z}}
#define IVEC3(x, y, z) (ivec3) {{x, y, z}}
#define VEC4(x, y, z, w) (vec4) {{x, y, z, w}}
#define IVEC4(x, y, z, w) (ivec4) {{x, y, z, w}}

#define MAT_UNION(name, type, simd_type, align, count, vec_name, ...) \
	typedef union __attribute((aligned(align))) name {                \
		simd_type mat;                                                \
		type      vec_name[count];                                    \
		struct {                                                      \
			type __VA_ARGS__;                                         \
		};                                                            \
	} name;
MAT_UNION(mat4, mat4_row, mat4_vec, 16, 4, col, c0, c1, c2, c3)
#undef MAT_UNION
#define MAT4(m00, m01, m02, m03, \
		     m10, m11, m12, m13, \
		     m20, m21, m22, m23, \
		     m30, m31, m32, m33) \
	{{	m00, m01, m02, m03,      \
		m10, m11, m12, m13,      \
		m20, m21, m22, m23,      \
		m30, m31, m32, m33 }};

typedef vec4 quat;

#define VEC_NIL -1
enum VecComponents {
	X,
	Y,
	Z,
	W,
	VEC_COUNT,
};
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
#define VEC3_ZERO = VEC3(0.0f, 0.0f, 0.0f);
#define VEC4_ZERO = VEC4(0.0f, 0.0f, 0.0f, 0.0f);
static const vec4 VEC4_IDENT = {{0.0f, 0.0f, 0.0f, 0.0f}};
static const quat QUAT_IDENT = {{0.0f, 0.0f, 0.0f, 1.0f}};
static const mat4 MAT4_IDENT = {{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
}};

// force inlining appears to produce same assembly for returning or by passing out pointer
// https://godbolt.org/z/rzo8hEaaG
// https://godbolt.org/z/149rjsPG3
#define MATH_INLINE       __attribute__((always_inline)) static inline
#define SHUFFLE(vec, ...) __builtin_shufflevector(vec, vec, __VA_ARGS__)

MATH_INLINE float Float4Sum(float4_vec float4)
{
	// appears to make better SIMD assembly than a loop:
	// https://godbolt.org/z/6jWe4Pj5b
	// https://godbolt.org/z/M5Goq57vj
	float4_vec shuf = SHUFFLE(float4, 2, 3, 0, 1);
	float4_vec sums = float4 + shuf;
	shuf = SHUFFLE(sums, 1, 0, 3, 2);
	sums = sums + shuf;
	return sums[0];
}
MATH_INLINE mat4 Mat4Translation(vec3 v)
{
	mat4 out = MAT4_IDENT;
	for (int i = 0; i < 3; ++i) out.col[3].row += MAT4_IDENT.col[i].row * v.vec[i];
	return out;
}
MATH_INLINE float Vec4Dot(vec4 l, vec4 r)
{
	float4_vec product = l.vec * r.vec;
	return Float4Sum(product);
}
MATH_INLINE float Vec4Mag(vec4 v)
{
	return sqrtf(Vec4Dot(v, v));
}
MATH_INLINE vec3 PosFromMat4(mat4 m)
{
	float w = m.c3.r3;
	return (vec3){{m.c3.r0 / w, m.c3.r1 / w, m.c3.r2 / w}};
}
MATH_INLINE mat4 Mat4YInvert(mat4 m)
{
	m.c0.r1 = -m.c0.r1;
	m.c1.r1 = -m.c1.r1;
	m.c2.r1 = -m.c2.r1;
	m.c3.r1 = -m.c3.r1;
	return m;
}
MATH_INLINE mat4 Mat4XInvert(mat4 m)
{
	m.c0.r0 = -m.c0.r0;
	m.c1.r0 = -m.c1.r0;
	m.c2.r0 = -m.c2.r0;
	m.c3.r0 = -m.c3.r0;
	return m;
}
MATH_INLINE quat RotFromMat4(mat4 m)
{
	quat q;
	float trace = m.c0.r0 + m.c1.r1 + m.c2.r2;
	if (trace > 0) {
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (m.c1.r2 - m.c2.r1) * s;
		q.y = (m.c2.r0 - m.c0.r2) * s;
		q.z = (m.c0.r1 - m.c1.r0) * s;
	}
	else if (m.c0.r0 > m.c1.r1 && m.c0.r0 > m.c2.r2) {
		float s = 2.0f * sqrtf(1.0f + m.c0.r0 - m.c1.r1 - m.c2.r2);
		q.w = (m.c1.r2 - m.c2.r1) / s;
		q.x = 0.25f * s;
		q.y = (m.c1.r0 + m.c0.r1) / s;
		q.z = (m.c2.r0 + m.c0.r2) / s;
	}
	else if (m.c1.r1 > m.c2.r2) {
		float s = 2.0f * sqrtf(1.0f + m.c1.r1 - m.c0.r0 - m.c2.r2);
		q.w = (m.c2.r0 - m.c0.r2) / s;
		q.x = (m.c1.r0 + m.c0.r1) / s;
		q.y = 0.25f * s;
		q.z = (m.c2.r1 + m.c1.r2) / s;
	}
	else {
		float s = 2.0f * sqrtf(1.0f + m.c2.r2 - m.c0.r0 - m.c1.r1);
		q.w = (m.c0.r1 - m.c1.r0) / s;
		q.x = (m.c2.r0 + m.c0.r2) / s;
		q.y = (m.c2.r1 + m.c1.r2) / s;
		q.z = 0.25f * s;
	}
	return q;
}
MATH_INLINE mat4 QuatToMat4(quat q)
{
	float norm = Vec4Mag(q);
	float s = norm > 0.0f ? 2.0f / norm : 0.0f;

	float3_vec xxw = SHUFFLE(q.vec, X, X, W, VEC_NIL);
	float3_vec xyx = SHUFFLE(q.vec, X, Y, X, VEC_NIL);
	float3_vec xx_xy_wx = s * xxw * xyx;

	float3_vec yyw = SHUFFLE(q.vec, Y, Y, W, VEC_NIL);
	float3_vec yzy = SHUFFLE(q.vec, Y, Z, Y, VEC_NIL);
	float3_vec yy_yz_wy = s * yyw * yzy;

	float3_vec zxw = SHUFFLE(q.vec, Z, X, W, VEC_NIL);
	float3_vec zzz = SHUFFLE(q.vec, Z, Z, Z, VEC_NIL);
	float3_vec zz_xz_wz = s * zxw * zzz;

	// Setting to specific SIMD indices produces same SIMD assembly as long as target
	// SIMD vec is same size as source vecs https://godbolt.org/z/qqMMa4v3G
	mat4 out = MAT4_IDENT;
	out.c0.r0 = 1.0f - yy_yz_wy[X] - zz_xz_wz[X];
	out.c1.r1 = 1.0f - xx_xy_wx[X] - zz_xz_wz[X];
	out.c2.r2 = 1.0f - xx_xy_wx[X] - yy_yz_wy[X];

	out.c0.r1 = xx_xy_wx[Y] + zz_xz_wz[Z];
	out.c1.r2 = yy_yz_wy[Y] + xx_xy_wx[Z];
	out.c2.r0 = zz_xz_wz[Y] + yy_yz_wy[Z];

	out.c1.r0 = xx_xy_wx[Y] - zz_xz_wz[Z];
	out.c2.r1 = yy_yz_wy[Y] - xx_xy_wx[Z];
	out.c0.r2 = zz_xz_wz[Y] - yy_yz_wy[Z];

	out.c0.r3 = 0.0f;
	out.c1.r3 = 0.0f;
	out.c2.r3 = 0.0f;
	out.c3.r0 = 0.0f;
	out.c3.r1 = 0.0f;
	out.c3.r2 = 0.0f;
	out.c3.r3 = 1.0f;
	return out;
}
// Turns out this is the fastest way. Write out the multiplication on the 16 byte vecs and GCC will produce the best SIMD
// Accessing a simd through a union of a different type will apply simd optimizations
// https://godbolt.org/z/Wb1hP5dv7
MATH_INLINE mat4 Mat4Mul(mat4 l, mat4 r)
{
	/// verify if refing these not through the simd type actually breaks the gcc simd optimization
	const float a00 = l.c0.r0, a01 = l.c0.r1, a02 = l.c0.r2, a03 = l.c0.r3,
				a10 = l.c1.r0, a11 = l.c1.r1, a12 = l.c1.r2, a13 = l.c1.r3,
				a20 = l.c2.r0, a21 = l.c2.r1, a22 = l.c2.r2, a23 = l.c2.r3,
				a30 = l.c3.r0, a31 = l.c3.r1, a32 = l.c3.r2, a33 = l.c3.r3,
				b00 = r.c0.r0, b01 = r.c0.r1, b02 = r.c0.r2, b03 = r.c0.r3,
				b10 = r.c1.r0, b11 = r.c1.r1, b12 = r.c1.r2, b13 = r.c1.r3,
				b20 = r.c2.r0, b21 = r.c2.r1, b22 = r.c2.r2, b23 = r.c2.r3,
				b30 = r.c3.r0, b31 = r.c3.r1, b32 = r.c3.r2, b33 = r.c3.r3;

	mat4 dest = MAT4_IDENT;
	dest.c0.r0 = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
	dest.c0.r1 = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
	dest.c0.r2 = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
	dest.c0.r3 = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
	dest.c1.r0 = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
	dest.c1.r1 = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
	dest.c1.r2 = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
	dest.c1.r3 = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
	dest.c2.r0 = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
	dest.c2.r1 = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
	dest.c2.r2 = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
	dest.c2.r3 = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
	dest.c3.r0 = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
	dest.c3.r1 = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
	dest.c3.r2 = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
	dest.c3.r3 = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
	return dest;
}
MATH_INLINE mat4 Mat4Inv(mat4 src)
{
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

	out.mat *= det;

	return out;
}
MATH_INLINE mat4 Mat4ZRot(mat4 m)
{
	mat4 zRot = {{
		-1, 0, 0, 0,
		0, -1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	}};
	return Mat4Mul(m, zRot);
}
MATH_INLINE mat4 Mat4FromPosRot(vec3 pos, quat rot)
{
	mat4 translationMat4 = Mat4Translation(pos);
	mat4 rotationMat4 = QuatToMat4(rot);
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
MATH_INLINE mat4 Mat4PerspectiveVulkanReverseZ(float yFovRad, float aspect, float zNear, float zFar)
{
	const float tanHalfYFov = tan(yFovRad / 2.0f);
	return (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfYFov)},
				  .c1 = {.r1 = 1.0f / tanHalfYFov},
				  .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
				  .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}

MATH_INLINE vec3 Vec3Cross(float3_vec l, float3_vec r)
{
	return (vec3){{l[Y] * r[Z] - r[Y] * l[Z],
				  l[Z] * r[X] - r[Z] * l[X],
				  l[X] * r[Y] - r[X] * l[Y]}};
}
MATH_INLINE vec3 Vec3Rot(quat q, vec3 v)
{
	vec3 uv = Vec3Cross(q.vec, v.vec);
	vec3 uuv = Vec3Cross(q.vec, uv.vec);
	vec3 out;
	for (int i = 0; i < 3; ++i) out.vec[i] = v.vec[i] + ((uv.vec[i] * q.vec[W]) + uuv.vec[i]) * 2.0f;
	return out;
}
MATH_INLINE vec4 Vec4Rot(quat q, vec4 v)
{
	vec3 uv = Vec3Cross(q.vec, v.vec);
	vec3 uuv = Vec3Cross(q.vec, uv.vec);
	vec4 out = {.w = v.w};
	for (int i = 0; i < 3; ++i) out.vec[i] = v.vec[i] + ((uv.vec[i] * q.vec[W]) + uuv.vec[i]) * 2.0f;
	return out;
}
MATH_INLINE quat QuatFromEuler(vec3 euler)
{
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
MATH_INLINE quat QuatNegate(quat src)
{
	return (quat){.x = -src.x, .y = -src.y, .z = -src.z, .w = src.w};
}
MATH_INLINE quat QuatMul(quat src, quat mul)
{
	return (quat){
		.x = src.w * mul.x + src.x * mul.w + src.y * mul.z - src.z * mul.y,
		.y = src.w * mul.y + src.y * mul.w + src.z * mul.x - src.x * mul.z,
		.z = src.w * mul.z + src.z * mul.w + src.x * mul.y - src.y * mul.x,
		.w = src.w * mul.w - src.x * mul.x - src.y * mul.y - src.z * mul.z,
	};
}
MATH_INLINE quat QuatInverse(quat q)
{
	// is this correct?
	float magnitudeSquared = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

	if (magnitudeSquared == 0.0) {
		fprintf(stderr, "quat mag is zero cannot invert.n");
		return (quat){{0, 0, 0, 0}};
	}

	quat conjugate = {{q.w, -q.x, -q.y, -q.z}};
	return (quat){conjugate.vec / magnitudeSquared};
}
MATH_INLINE vec4 Vec4MulMat4(mat4 m, vec4 v)
{
	vec4 out;
	out.x = m.c0.r0 * v.x + m.c1.r0 * v.y + m.c2.r0 * v.z + m.c3.r0 * v.w;
	out.y = m.c0.r1 * v.x + m.c1.r1 * v.y + m.c2.r1 * v.z + m.c3.r1 * v.w;
	out.z = m.c0.r2 * v.x + m.c1.r2 * v.y + m.c2.r2 * v.z + m.c3.r2 * v.w;
	out.w = m.c0.r3 * v.x + m.c1.r3 * v.y + m.c2.r3 * v.z + m.c3.r3 * v.w;
	return out;
}
MATH_INLINE vec3 Vec4WDivide(vec4 v)
{
	return (vec3){.vec = v.vec / v.vec[W]};
}
MATH_INLINE vec2 Vec2FromVec3(vec3 ndc)
{
	return (vec2){.x = ndc.x, .y = ndc.y};
}
MATH_INLINE vec2 UVFromNDC(vec3 ndc)
{
	vec2 out = Vec2FromVec3(ndc);
	out.vec = out.vec * 0.5f + 0.5f;
	return out;
}
MATH_INLINE ivec2 iVec2CeiDivide(ivec2 v, int d)
{
	vec2 fv = {.vec = {v.vec[X], v.vec[Y]}};
	fv.vec = fv.vec / (float)d;
	return (ivec2){{ceilf(fv.vec[X]), ceilf(fv.vec[Y])}};
}
MATH_INLINE ivec2 iVec2ShiftRightCeil(ivec2 v, int shift)
{
	if (shift <= 0) return v;
	return (ivec2){(v.vec >> shift) + (v.vec % 2)};
}
MATH_INLINE ivec2 iVec2Min(ivec2 v, u32 min)
{
	ivec2 out;
	for (int i = 0; i < 2; ++i) out.vec[i] = v.vec[i] < min ? min : v.vec[i];
	return out;
}
MATH_INLINE vec2 Vec2Clamp(vec2 v, float min, float max)
{
	float2_vec minVec = {min, min};
	float2_vec maxVec = {max, max};
	vec2       clamped = v;
	for (int i = 0; i < 2; ++i) {
		clamped.vec[i] = (v.vec[i] < minVec[i]) ? minVec[i] : v.vec[i];
		clamped.vec[i] = (clamped.vec[i] > maxVec[i]) ? maxVec[i] : clamped.vec[i];
	}
	return clamped;
}
MATH_INLINE vec2 Vec2Min(vec2 a, vec2 b)
{
	vec2 out;
	for (int i = 0; i < 2; ++i) out.vec[i] = a.vec[i] < b.vec[i] ? a.vec[i] : b.vec[i];
	return out;
}
MATH_INLINE vec2 Vec2Max(vec2 a, vec2 b)
{
	vec2 out;
	for (int i = 0; i < 2; ++i) out.vec[i] = a.vec[i] > b.vec[i] ? a.vec[i] : b.vec[i];
	return out;
}
MATH_INLINE float Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

MATH_INLINE void Mat4Print(mat4 m)
{
	printf("mat4:\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n% .4f % .4f % .4f % .4f\n",
		   m.c0.r0, m.c1.r0, m.c2.r0, m.c3.r0,
		   m.c0.r1, m.c1.r1, m.c2.r1, m.c3.r1,
		   m.c0.r2, m.c1.r2, m.c2.r2, m.c3.r2,
		   m.c0.r3, m.c1.r3, m.c2.r3, m.c3.r3);
}

#undef MATH_INLINE
#undef SHUFFLE