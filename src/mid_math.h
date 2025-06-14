////////////////////
//// Mid Math Header
////////////////////
#pragma once

#include "mid_common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define PI                 3.14159265358979323846f
#define RAD_FROM_DEG(_deg) (_deg * (PI / 180.0f))
#define DEG_FROM_RAD(_rad) (_rad * (180.0f / PI))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

// I think we want a basic VEC2 then a more advanced TO_VEC2 for conversions
#define VEC2(_x, _y) (vec2){{(_x), (_y)}}
#define TO_VEC2_1(_arg0) _Generic((_arg0),   \
	float: (vec2){{(_arg0), (_arg0)}},    \
	int: (vec2){{(_arg0), (_arg0)}},      \
	u32: (vec2){{(_arg0), (_arg0)}},      \
	vec2: (vec2){{(_arg0).x, (_arg0).y}})
#define TO_VEC2_2(_arg0, _1) _Generic((_arg0), \
	float: (vec2){{(_arg0), (_1)}},      \
	int: (vec2){{(_arg0), (_1)}},        \
	u32: (vec2){{(_arg0), (_1)}})
#define TO_VEC2_MACRO(_1, _2, NAME, ...) NAME
#define TO_VEC2(...) VEC2_MACRO(__VA_ARGS__, TO_VEC2_2, TO_VEC2_1)(__VA_ARGS__)

#define IVEC2(x, y) (ivec2) {{x, y}}

#define VEC3(x, y, z) (vec3) {{x, y, z}}
#define TO_VEC3_1(_arg0) ({                                   \
	auto _val0 = (_arg0);                                     \
	_Generic((_val0),                                         \
		vec2: VEC3(_val0.x, _val0.y, 0.0f),                   \
		vec3: VEC3(_val0.x, _val0.y, _val0.z),                \
		vec4: VEC3(_val0.x, _val0.y, _val0.z),                \
		default: assert(!"TO_VEC3 Unsupported Conversion!")); \
})
#define TO_VEC3_2(_arg0, _arg1) ({            \
	auto _val0 = (_arg0);                     \
	auto _val1 = (_arg1);                     \
	_Generic((_val0),                         \
		vec2: VEC3(_val0.x, _val0.y, _val1)); \
})
#define TO_VEC3_MACRO(_1, _2, NAME, ...) NAME
#define TO_VEC3(...) TO_VEC3_MACRO(__VA_ARGS__, TO_VEC3_2, TO_VEC3_1)(__VA_ARGS__)


#define IVEC3(x, y, z) (ivec3) {{x, y, z}}
#define IVEC4(x, y, z, w) (ivec4) {{x, y, z, w}}

#define VEC4_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define VEC4_1(_v) _Generic((_v),               \
	f32: (vec4){{(_v).x, 0.0f, 0.0f, 0.0f}},    \
	vec2: (vec4){{(_v).x, (_v).y, 0.0f, 0.0f}}, \
	vec3: (vec4){{(_v).x, (_v).y, (_v).z, 0.0f}})
#define VEC4_2(_v, _arg0) _Generic((_v),              \
	f32: (vec4){{(_v).x, (_arg0), (_arg0), (_arg0)}}, \
	vec2: (vec4){{(_v).x, (_v).y, (_arg0), (_arg0)}}, \
	vec3: (vec4){{(_v).x, (_v).y, (_v).z, (_arg0)}})
#define VEC4_3(_v, _arg0, _1) _Generic((_v), \
	f32: (vec4){{(_v).x, _arg0, _1, _1}},    \
	vec2: (vec4){{(_v).x, (_v).y, (_arg0), (_1)}})
#define VEC4_4(_arg0, _1, _2, _3) (vec4){{(_arg0), (_1), (_2), (_3)}}
#define VEC4(...) VEC4_MACRO(__VA_ARGS__, VEC4_4 ,VEC4_3, VEC4_2, VEC4_1)(__VA_ARGS__)

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

typedef struct ray {
	vec3 origin;
	vec3 dir;
} ray;

typedef struct pose {
	vec3 pos;
	vec3 euler;
	quat rot;
} pose;

typedef struct cam {
	f32 zNear;
	f32 zFar;
	f32 yFovRad;
} cam;

typedef struct vert {
	vec3 pos;
	vec3 norm;
	vec2 uv;
} vert;

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
constexpr vec3 VEC3_ZERO = {{0.0f, 0.0f, 0.0f}};
constexpr vec4 VEC4_ZERO = {{0.0f, 0.0f, 0.0f, 0.0f}};
constexpr vec4 VEC4_IDENT = {{0.0f, 0.0f, 0.0f, 1.0f}};
constexpr quat QUAT_IDENT = {{0.0f, 0.0f, 0.0f, 1.0f}};
constexpr mat4 MAT4_IDENT = {{
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

MATH_INLINE float float2Sum(float2_vec float2) {
	return float2[0] + float2[1];
}

MATH_INLINE float float3Sum(float3_vec float3)
{
	float3_vec shuf = SHUFFLE(float3, 2, 3, 0, 1);
	float3_vec sums = float3 + shuf;
	shuf = SHUFFLE(sums, 1, 0, 3, 2);
	sums = sums + shuf;
	return sums[0];
}

MATH_INLINE float float4Sum(float4_vec float4)
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

MATH_INLINE float vec2Dot(vec2 l, vec2 r) {
	return float2Sum(l.vec * r.vec);
}

MATH_INLINE float vec3Dot(vec3 l, vec3 r) {
	return float3Sum(l.vec * r.vec);
}

MATH_INLINE float vec4Dot(vec4 l, vec4 r) {
	return float4Sum(l.vec * r.vec);
}

MATH_INLINE float vec2Mag(vec2 v) {
	return sqrtf(vec2Dot(v, v));
}

MATH_INLINE float vec3Mag(vec3 v) {
	return sqrtf(vec3Dot(v, v));
}

MATH_INLINE float vec4Mag(vec4 v) {
	return sqrtf(vec4Dot(v, v));
}

MATH_INLINE vec2 vec2Sub(vec2 a, vec2 b) {
	float2_vec diff = a.vec - b.vec;
	return VEC2(diff[0], diff[1]);
}

MATH_INLINE vec3 vec3Sub(vec3 a, vec3 b) {
	float3_vec diff = a.vec - b.vec;
	return VEC3(diff[0], diff[1], diff[2]);
}

MATH_INLINE vec4 vec4Sub(vec4 a, vec4 b) {
	float4_vec diff = a.vec - b.vec;
	return VEC4(diff[0], diff[1], diff[2], diff[3]);
}

MATH_INLINE vec3 vec3Normalize(vec3 v)
{
	float mag = vec3Mag(v);
	return (vec3){{v.vec[0] / mag, v.vec[1] / mag, v.vec[2] / mag}};
}

MATH_INLINE mat4 Mat4Translation(vec3 v)
{
	mat4 out = MAT4_IDENT;
	for (int i = 0; i < 3; ++i) out.col[3].row += MAT4_IDENT.col[i].row * v.vec[i];
	return out;
}

MATH_INLINE vec3 vec3PosFromMat4(mat4 m)
{
	float w = m.c3.r3;
	return (vec3){{m.c3.r0 / w, m.c3.r1 / w, m.c3.r2 / w}};
}

MATH_INLINE mat4 mat4YInvert(mat4 m)
{
	m.c0.r1 = -m.c0.r1;
	m.c1.r1 = -m.c1.r1;
	m.c2.r1 = -m.c2.r1;
	m.c3.r1 = -m.c3.r1;
	return m;
}

MATH_INLINE mat4 mat4XInvert(mat4 m)
{
	m.c0.r0 = -m.c0.r0;
	m.c1.r0 = -m.c1.r0;
	m.c2.r0 = -m.c2.r0;
	m.c3.r0 = -m.c3.r0;
	return m;
}

MATH_INLINE quat quatFromMat4(mat4 m)
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

MATH_INLINE mat4 quatToMat4(quat q)
{
	float norm = vec4Mag(q);
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
MATH_INLINE mat4 mat4Mul(mat4 l, mat4 r)
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

MATH_INLINE mat4 mat4Inv(mat4 src)
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

MATH_INLINE mat4 mat4ZRot(mat4 m)
{
	mat4 zRot = {{
		-1, 0, 0, 0,
		0, -1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	}};
	return mat4Mul(m, zRot);
}

MATH_INLINE mat4 mat4FromPosRot(vec3 pos, quat rot)
{
	mat4 translationMat4 = Mat4Translation(pos);
	mat4 rotationMat4 = quatToMat4(rot);
	return mat4Mul(translationMat4, rotationMat4);
}

// should I do this?
typedef struct mat4_proj_packed {
	float c0r0;
	float c1r1;
	float c2r2;
	float c2r3;
	float c3r2;
} mat4_proj_packed;

//// Perspective matrix in Vulkan Reverse Z
MATH_INLINE mat4 Mat4PerspectiveVulkanReverseZ(float yFovRad, float aspect, float zNear, float zFar)
{
	const float tanHalfYFov = tan(yFovRad / 2.0f);
	return (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfYFov)},
				  .c1 = {.r1 = 1.0f / tanHalfYFov},
				  .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
				  .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}

MATH_INLINE vec3 vec3Cross(float3_vec l, float3_vec r)
{
	return (vec3){{l[Y] * r[Z] - r[Y] * l[Z],
				  l[Z] * r[X] - r[Z] * l[X],
				  l[X] * r[Y] - r[X] * l[Y]}};
}

MATH_INLINE vec3 vec3Rot(quat q, vec3 v)
{
	vec3 uv = vec3Cross(q.vec, v.vec);
	vec3 uuv = vec3Cross(q.vec, uv.vec);
	vec3 out;
	for (int i = 0; i < 3; ++i) out.vec[i] = v.vec[i] + ((uv.vec[i] * q.vec[W]) + uuv.vec[i]) * 2.0f;
	return out;
}

MATH_INLINE vec4 Vec4Rot(quat q, vec4 v)
{
	vec3 uv = vec3Cross(q.vec, v.vec);
	vec3 uuv = vec3Cross(q.vec, uv.vec);
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
		fprintf(stderr, "quatValue mag is zero cannot invert.n");
		return (quat){{0, 0, 0, 0}};
	}

	quat conjugate = {{q.w, -q.x, -q.y, -q.z}};
	return (quat){conjugate.vec / magnitudeSquared};
}

MATH_INLINE vec4 vec4MulMat4(mat4 m, vec4 v)
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

typedef struct Plane {
	vec3 normal;
	vec3 origin;
} Plane;

MATH_INLINE ray rayFromScreenUV(vec2 uv, mat4 invProj, mat4 invView, mat4 invViewProj)
{
	vec2 ndc = { uv.vec * 2.0f - 1.0f };
	vec4 clipRayPos = VEC4(ndc.x, ndc.y, 0.0f, 1.0f);
	vec4 clipWorldPos = vec4MulMat4(invViewProj, clipRayPos);

	vec4 clipRayDir = VEC4(ndc.x, ndc.y, 1.0f, 1.0f);
	vec4 viewSpace = vec4MulMat4(invProj, clipRayDir);
	vec4 viewDir = VEC4(viewSpace.x, viewSpace.y, 1, 0);
	vec4 worldRayDir = vec4MulMat4(invView, viewDir);

	return (ray) {.origin = TO_VEC3(clipWorldPos), .dir = TO_VEC3(worldRayDir)};
}

MATH_INLINE bool rayIntersetPlane(ray ray, Plane plane, vec3* outPoint)
{
	float facingRatio = vec3Dot(plane.normal, ray.dir);
	if (fabsf(facingRatio) < 0.0001f) return false;
	float t = vec3Dot(vec3Sub(plane.origin, ray.origin), plane.normal) / facingRatio;
	if (t < 0) return false;
	outPoint->vec = ray.origin.vec + ray.dir.vec * t;
	return true;
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



MATH_INLINE bool Vec2PointOnLineSegment(vec2 p, vec2 start, vec2 end, float tolerance)
{
	float d1 = vec2Mag(VEC2(p.x - start.x, p.y - start.y));
	float d2 = vec2Mag(VEC2(p.x - end.x, p.y - end.y));
	float lineLen = vec2Mag(VEC2(end.x - start.x, end.y - start.y));
	return d1 + d2 >= lineLen - tolerance && d1 + d2 <= lineLen + tolerance;
}

MATH_INLINE bool Vec2LineIntersect(vec2 a1, vec2 a2, vec2 b1, vec2 b2, vec2* intersection)
{
	float denominator = (b2.y - b1.y) * (a2.x - a1.x) - (b2.x - b1.x) * (a2.y - a1.y);
	if (denominator == 0) return false;

	float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / denominator;
	float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / denominator;

	if (ua >= 0 && ua <= 1 && ub >= 0 && ub <= 1) {
		if (intersection) {
			intersection->x = a1.x + ua * (a2.x - a1.x);
			intersection->y = a1.y + ua * (a2.y - a1.y);
		}
		return true;
	}

	return false;
}

MATH_INLINE float Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

MATH_INLINE float Clamp(float v, float min, float max)
{
	return fmax(fmin(v, min), max);
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