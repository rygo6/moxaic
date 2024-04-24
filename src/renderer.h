#pragma once

#include "globals.h"
#include "window.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

//----------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------

#define VKM_ALLOC      NULL
#define VKM_VERSION    VK_MAKE_API_VERSION(0, 1, 3, 2)
#define VKM_SWAP_COUNT 2
#define VKM_REQUIRE(command)                                \
  {                                                         \
    VkResult result = command;                              \
    REQUIRE(result == VK_SUCCESS, string_VkResult(result)); \
  }
#define CLEANUP_RESULT(command, cleanup_goto)      \
  result = command;                                \
  if (__builtin_expect(result != VK_SUCCESS, 0)) { \
    LOG_ERROR(command, string_VkResult(result));   \
    goto cleanup_goto;                             \
  }
#define VKM_PFN_LOAD(instance, vkFunction)                                                      \
  PFN_##vkFunction vkFunction = (PFN_##vkFunction)vkGetInstanceProcAddr(instance, #vkFunction); \
  REQUIRE(vkFunction != NULL, "Couldn't load " #vkFunction)

#define VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define VKM_MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

//----------------------------------------------------------------------------------
// Math Types
//----------------------------------------------------------------------------------

#define VKM_SIMD_TYPE(type, name, count) typedef type name##_simd __attribute__((vector_size(sizeof(type) * count)))
#define VKM_MATH_UNION(type, simd_type, name, align, count, vec_name, ...) \
  typedef union __attribute((aligned(align))) name {                       \
    type vec_name[count];                                                  \
    struct {                                                               \
      type __VA_ARGS__;                                                    \
    };                                                                     \
    simd_type simd;                                                        \
  } name;
VKM_SIMD_TYPE(float, float2, 2);
VKM_SIMD_TYPE(uint32_t, int2, 2);
VKM_SIMD_TYPE(float, float3, 4);
VKM_SIMD_TYPE(uint32_t, int3, 4);
VKM_SIMD_TYPE(float, float4, 4);
VKM_SIMD_TYPE(uint32_t, int4, 4);
VKM_SIMD_TYPE(float, mat4, 16);
VKM_MATH_UNION(float, float2_simd, vec2, 8, 2, vec, x, y);
VKM_MATH_UNION(uint32_t, int2_simd, ivec2, 16, 2, vec, x, y);
VKM_MATH_UNION(float, float3_simd, vec3, 16, 3, vec, x, y, z);
VKM_MATH_UNION(uint32_t, int3_simd, ivec3, 16, 3, vec, x, y, z);
VKM_MATH_UNION(float, float4_simd, vec4, 16, 4, vec, x, y, z, w);
VKM_MATH_UNION(uint32_t, int4_simd, ivec4, 16, 4, vec, x, y, z, w);
VKM_MATH_UNION(float, float4_simd, mat4_row, 16, 4, row, r0, r1, r2, r3);
VKM_MATH_UNION(mat4_row, mat4_simd, mat4, 16, 4, col, c0, c1, c2, c3);
typedef vec4 quat;
#define VKM_PI 3.14159265358979323846f

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------

typedef struct VkmTimeline {
  VkSemaphore semaphore;
  uint64_t    waitValue;
} VkmTimeline;

typedef struct VkmSwap {
  VkSwapchainKHR chain;
  VkSemaphore    acquireSemaphore;
  VkSemaphore    renderCompleteSemaphore;
  uint32_t       swapIndex;
  VkImage        images[VKM_SWAP_COUNT];
} VkmSwap;

typedef struct VkmTransform {
  vec3 position;
  vec3 euler;
  vec4 rotation;
} VkmTransform;

typedef struct VkmTexture {
  VkImage        image;
  VkImageView    imageView;
  VkDeviceMemory imageMemory;
  VkFormat       format;
  VkExtent3D     extent;
} VkmTexture;

typedef struct VkmVertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
} VkmVertex;

typedef struct VkmMesh {
  uint32_t       indexCount;
  VkDeviceMemory indexMemory;
  VkBuffer       indexBuffer;
  uint32_t       vertexCount;
  VkDeviceMemory vertexMemory;
  VkBuffer       vertexBuffer;
} VkmMesh;

typedef struct VkmFramebuffer {
  VkImage        colorImage;
  VkDeviceMemory colorImageMemory;
  VkImageView    colorImageView;

  VkImage        normalImage;
  VkDeviceMemory normalImageMemory;
  VkImageView    normalImageView;

  VkImage        depthImage;
  VkDeviceMemory depthImageMemory;
  VkImageView    depthImageView;

  VkImage        gBufferImage;
  VkDeviceMemory gBufferImageMemory;
  VkImageView    gBufferImageView;

  VkFramebuffer framebuffer;
  VkSemaphore   renderCompleteSemaphore;

} VkmFramebuffer;

typedef struct VkmGlobalSetState {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;

  mat4 inverseView;
  mat4 inverseProjection;
  mat4 inverseViewProjection;

  ivec2 screenSize;
} VkmGlobalSetState;

typedef struct VkmStandardObjectSetState {
  mat4 model;
} VkmStandardObjectSetState;

typedef struct VkmCommandContext {
  VkCommandPool   pool;
  VkCommandBuffer buffer;
  uint32_t        queueFamilyIndex;
  VkQueue         queue;
} VkmCommandContext;

typedef struct VkmContext {
  VkPhysicalDevice physicalDevice;
  VkDevice         device;

  VkmTimeline      timeline;

  VkDescriptorPool descriptorPool;
  VkQueryPool      timeQueryPool;

  VkmCommandContext graphicsCommand;
  VkmCommandContext computeCommand;
  VkmCommandContext transferCommand;

} VkmContext;

typedef struct VkmStandardPipeline {
  VkPipelineLayout pipelineLayout;
  VkPipeline       pipeline;

  VkDescriptorSetLayout globalSetLayout;
  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSetLayout objectSetLayout;

} VkmStandardPipe;

//----------------------------------------------------------------------------------
// Render
//----------------------------------------------------------------------------------

enum VkmPassAttachmentStandardIndices {
  VKM_PASS_ATTACHMENT_STD_COLOR_INDEX,
  VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX,
  VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX,
  VKM_PASS_ATTACHMENT_STD_COUNT,
};
enum VkmPipeSetStandardIndices {
  VKM_PIPE_SET_STD_GLOBAL_INDEX,
  VKM_PIPE_SET_STD_MATERIAL_INDEX,
  VKM_PIPE_SET_STD_OBJECT_INDEX,
  VKM_PIPE_SET_STD_INDEX_COUNT,
};
#define VKM_STD_G_BUFFER_FORMAT VK_FORMAT_R32_SFLOAT;
#define VKM_STD_G_BUFFER_USAGE  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
#define VKM_PASS_CLEAR_COLOR \
  { 0.1f, 0.2f, 0.3f, 0.0f }

#define VKM_SET_BINDING_STD_GLOBAL_BUFFER 0
#define VKM_SET_WRITE_STD_GLOBAL_BUFFER(globalSet, globalSetBuffer) \
  {                                                                 \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                \
    .dstSet = globalSet,                                            \
    .dstBinding = VKM_SET_BINDING_STD_GLOBAL_BUFFER,                \
    .descriptorCount = 1,                                           \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                 \
        .buffer = globalSetBuffer,                                  \
        .range = sizeof(VkmGlobalSetState),                         \
    },                                                              \
  }
#define VKM_SET_BINDING_STD_MATERIAL_TEXTURE 0
#define VKM_SET_WRITE_STD_MATERIAL_IMAGE(materialSet, material_image_view, material_image_sample) \
  {                                                                                               \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                                              \
    .dstSet = materialSet,                                                                        \
    .dstBinding = VKM_SET_BINDING_STD_MATERIAL_TEXTURE,                                           \
    .descriptorCount = 1,                                                                         \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                                  \
    .pImageInfo = &(const VkDescriptorImageInfo){                                                 \
        .sampler = material_image_sample,                                                         \
        .imageView = material_image_view,                                                         \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                                  \
    },                                                                                            \
  }
#define VKM_SET_BINDING_STD_OBJECT_BUFFER 0
#define VKM_SET_WRITE_STD_OBJECT_BUFFER(objectSet, objectSetBuffer) \
  {                                                                 \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                \
    .dstSet = objectSet,                                            \
    .dstBinding = VKM_SET_BINDING_STD_OBJECT_BUFFER,                \
    .descriptorCount = 1,                                           \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                 \
        .buffer = objectSetBuffer,                                  \
        .range = sizeof(VkmStandardObjectSetState),                 \
    },                                                              \
  }

//----------------------------------------------------------------------------------
// Math
//----------------------------------------------------------------------------------

#define VEC3_ITERATE           for (int i = 0; i < 3; ++i)
#define VEC4_ITERATE           for (int i = 0; i < 4; ++i)
#define SIMD_NIL               -1
#define SIMD_SHUFFLE(vec, ...) __builtin_shufflevector(vec, vec, __VA_ARGS__)
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
static const quat QUAT_IDENT = {0.0f, 0.0f, 0.0f, 1.0f};
static const mat4 MAT4_IDENT = {
    .c0 = {1.0f, 0.0f, 0.0f, 0.0f},
    .c1 = {0.0f, 1.0f, 0.0f, 0.0f},
    .c2 = {0.0f, 0.0f, 1.0f, 0.0f},
    .c3 = {0.0f, 0.0f, 0.0f, 1.0f},
};

static inline float vkmFloat4Sum(float4_simd* pFloat4) {
  // appears to make better SIMD assembly than a loop:
  // https://godbolt.org/z/6jWe4Pj5b
  // https://godbolt.org/z/M5Goq57vj
  float4_simd shuf = SIMD_SHUFFLE(*pFloat4, 2, 3, 0, 1);
  float4_simd sums = *pFloat4 + shuf;
  shuf = SIMD_SHUFFLE(sums, 1, 0, 3, 2);
  sums = sums + shuf;
  return sums[0];
}
static inline void vkmMat4Translation(const vec3* pTranslationVec3, mat4* pDstMat4) {
  VEC3_ITERATE pDstMat4->col[3].simd += MAT4_IDENT.col[i].simd * pTranslationVec3->vec[i];
}
static inline float vkmVec4Dot(const vec4* l, const vec4* r) {
  float4_simd product = l->simd * r->simd;
  return vkmFloat4Sum(&product);
}
static inline float vkmVec4Mag(const vec4* v) {
  return sqrtf(vkmVec4Dot(v, v));
}
static inline void vkmQuatToMat4(const quat* pQuat, mat4* pDst) {
  const float norm = vkmVec4Mag(pQuat);
  const float s = norm > 0.0f ? 2.0f / norm : 0.0f;

  const float3_simd xxw = SIMD_SHUFFLE(pQuat->simd, VEC_X, VEC_X, VEC_W, SIMD_NIL);
  const float3_simd xyx = SIMD_SHUFFLE(pQuat->simd, VEC_X, VEC_Y, VEC_X, SIMD_NIL);
  const float3_simd xx_xy_wx = s * xxw * xyx;

  const float3_simd yyw = SIMD_SHUFFLE(pQuat->simd, VEC_Y, VEC_Y, VEC_W, SIMD_NIL);
  const float3_simd yzy = SIMD_SHUFFLE(pQuat->simd, VEC_Y, VEC_Z, VEC_Y, SIMD_NIL);
  const float3_simd yy_yz_wy = s * yyw * yzy;

  const float3_simd zxw = SIMD_SHUFFLE(pQuat->simd, VEC_Z, VEC_X, VEC_W, SIMD_NIL);
  const float3_simd zzz = SIMD_SHUFFLE(pQuat->simd, VEC_Z, VEC_Z, VEC_Z, SIMD_NIL);
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
static inline void vkmMat4MulRot(const mat4* pSrc, const mat4* pRot, mat4* pDst) {
  const mat4_simd src0 = SIMD_SHUFFLE(pSrc->simd,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  const mat4_simd rot0 = SIMD_SHUFFLE(pRot->simd,
                                      MAT_C0_R0, MAT_C0_R0, MAT_C0_R0, MAT_C0_R0,
                                      MAT_C1_R0, MAT_C1_R0, MAT_C1_R0, MAT_C1_R0,
                                      MAT_C2_R0, MAT_C2_R0, MAT_C2_R0, MAT_C2_R0,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  const mat4_simd src1 = SIMD_SHUFFLE(pSrc->simd,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  const mat4_simd rot1 = SIMD_SHUFFLE(pRot->simd,
                                      MAT_C0_R1, MAT_C0_R1, MAT_C0_R1, MAT_C0_R1,
                                      MAT_C1_R1, MAT_C1_R1, MAT_C1_R1, MAT_C1_R1,
                                      MAT_C2_R1, MAT_C2_R1, MAT_C2_R1, MAT_C2_R1,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  const mat4_simd src2 = SIMD_SHUFFLE(pSrc->simd,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  const mat4_simd rot2 = SIMD_SHUFFLE(pRot->simd,
                                      MAT_C0_R2, MAT_C0_R2, MAT_C0_R2, MAT_C0_R2,
                                      MAT_C1_R2, MAT_C1_R2, MAT_C1_R2, MAT_C1_R2,
                                      MAT_C2_R2, MAT_C2_R2, MAT_C2_R2, MAT_C2_R2,
                                      SIMD_NIL, SIMD_NIL, SIMD_NIL, SIMD_NIL);
  pDst->simd = src0 * rot0 + src1 * rot1 + src2 * rot2;
  // I don't actually know if doing this simd_nil is faster and this whole rot only mat multiply is pointless
  pDst->c3.simd = pSrc->c3.simd;
}
static inline void vkmMat4Mul(const mat4* pLeft, const mat4* pRight, mat4* pDst) {
  const mat4_simd src0 = SIMD_SHUFFLE(pLeft->simd,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
                                      MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3);
  const mat4_simd rot0 = SIMD_SHUFFLE(pRight->simd,
                                      MAT_C0_R0, MAT_C0_R0, MAT_C0_R0, MAT_C0_R0,
                                      MAT_C1_R0, MAT_C1_R0, MAT_C1_R0, MAT_C1_R0,
                                      MAT_C2_R0, MAT_C2_R0, MAT_C2_R0, MAT_C2_R0,
                                      MAT_C3_R0, MAT_C3_R0, MAT_C3_R0, MAT_C3_R0);
  const mat4_simd src1 = SIMD_SHUFFLE(pLeft->simd,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
                                      MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3);
  const mat4_simd rot1 = SIMD_SHUFFLE(pRight->simd,
                                      MAT_C0_R1, MAT_C0_R1, MAT_C0_R1, MAT_C0_R1,
                                      MAT_C1_R1, MAT_C1_R1, MAT_C1_R1, MAT_C1_R1,
                                      MAT_C2_R1, MAT_C2_R1, MAT_C2_R1, MAT_C2_R1,
                                      MAT_C3_R1, MAT_C3_R1, MAT_C3_R1, MAT_C3_R1);
  const mat4_simd src2 = SIMD_SHUFFLE(pLeft->simd,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
                                      MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3);
  const mat4_simd rot2 = SIMD_SHUFFLE(pRight->simd,
                                      MAT_C0_R2, MAT_C0_R2, MAT_C0_R2, MAT_C0_R2,
                                      MAT_C1_R2, MAT_C1_R2, MAT_C1_R2, MAT_C1_R2,
                                      MAT_C2_R2, MAT_C2_R2, MAT_C2_R2, MAT_C2_R2,
                                      MAT_C3_R2, MAT_C3_R2, MAT_C3_R2, MAT_C3_R2);
  const mat4_simd src3 = SIMD_SHUFFLE(pLeft->simd,
                                      MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                      MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                      MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
                                      MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3);
  const mat4_simd rot3 = SIMD_SHUFFLE(pRight->simd,
                                      MAT_C0_R3, MAT_C0_R3, MAT_C0_R3, MAT_C0_R3,
                                      MAT_C1_R3, MAT_C1_R3, MAT_C1_R3, MAT_C1_R3,
                                      MAT_C2_R3, MAT_C2_R3, MAT_C2_R3, MAT_C2_R3,
                                      MAT_C3_R3, MAT_C3_R3, MAT_C3_R3, MAT_C3_R3);
  pDst->simd = src0 * rot0 + src1 * rot1 + src2 * rot2 + src3 * rot3;
}
static inline void vkmMat4Scale(const float scale, mat4* pDst) {
  pDst->simd *= scale;
}
static inline void vkmMat4Inv(const mat4* pSrc, mat4* pDst) {
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

  pDst->c0.r0 = f * t[0] - g * t[1] + h * t[2];
  pDst->c1.r0 = -(e * t[0] - g * t[3] + h * t[4]);
  pDst->c2.r0 = e * t[1] - f * t[3] + h * t[5];
  pDst->c3.r0 = -(e * t[2] - f * t[4] + g * t[5]);

  pDst->c0.r1 = -(b * t[0] - c * t[1] + d * t[2]);
  pDst->c1.r1 = a * t[0] - c * t[3] + d * t[4];
  pDst->c2.r1 = -(a * t[1] - b * t[3] + d * t[5]);
  pDst->c3.r1 = a * t[2] - b * t[4] + c * t[5];

  t[0] = g * p - o * h;
  t[1] = f * p - n * h;
  t[2] = f * o - n * g;
  t[3] = e * p - m * h;
  t[4] = e * o - m * g;
  t[5] = e * n - m * f;

  pDst->c0.r2 = b * t[0] - c * t[1] + d * t[2];
  pDst->c1.r2 = -(a * t[0] - c * t[3] + d * t[4]);
  pDst->c2.r2 = a * t[1] - b * t[3] + d * t[5];
  pDst->c3.r2 = -(a * t[2] - b * t[4] + c * t[5]);

  t[0] = g * l - k * h;
  t[1] = f * l - j * h;
  t[2] = f * k - j * g;
  t[3] = e * l - i * h;
  t[4] = e * k - i * g;
  t[5] = e * j - i * f;

  pDst->c0.r3 = -(b * t[0] - c * t[1] + d * t[2]);
  pDst->c1.r3 = a * t[0] - c * t[3] + d * t[4];
  pDst->c2.r3 = -(a * t[1] - b * t[3] + d * t[5]);
  pDst->c3.r3 = a * t[2] - b * t[4] + c * t[5];

  det = 1.0f / (a * pDst->c0.r0 + b * pDst->c1.r0 + c * pDst->c2.r0 + d * pDst->c3.r0);

  vkmMat4Scale(det, pDst);
}
static inline void vkmMat4FromTransform(const vec3* pPos, const quat* pRot, mat4* pDst) {
  mat4 translationMat4 = MAT4_IDENT;
  vkmMat4Translation(pPos, &translationMat4);
  mat4 rotationMat4 = MAT4_IDENT;
  vkmQuatToMat4(pRot, &rotationMat4);
  vkmMat4MulRot(&translationMat4, &rotationMat4, pDst);
}
// Perspective matrix in Vulkan Reverse Z
static inline void vkmMat4Perspective(const float fov, const float aspect, const float zNear, const float zFar, mat4* pDstMat4) {
  const float tanHalfFovy = tan(fov / 2.0f);
  *pDstMat4 = (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfFovy)},
                     .c1 = {.r1 = 1.0f / tanHalfFovy},
                     .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
                     .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}
static inline void vkmVec3Cross(const vec3* pLeft, const vec3* pRight, vec3* pDst) {
  *pDst = (vec3){pLeft->y * pRight->z - pRight->y * pLeft->z,
                 pLeft->z * pRight->x - pRight->z * pLeft->x,
                 pLeft->x * pRight->y - pRight->x * pLeft->y};
}
static inline void vkmVec3Rot(const vec3* pSrc, const quat* pRot, vec3* pDst) {
  vec3 uv, uuv;
  vkmVec3Cross((vec3*)pRot, pSrc, &uv);
  vkmVec3Cross((vec3*)pRot, &uv, &uuv);
  for (int i = 0; i < 3; ++i) pDst->vec[i] = pSrc->vec[i] + ((uv.vec[i] * pRot->w) + uuv.vec[i]) * 2.0f;
}
static inline void vkmVec3EulerToQuat(const vec3* pEuler, quat* pDst) {
  vec3 c, s;
  VEC3_ITERATE {
    c.vec[i] = cos(pEuler->vec[i] * 0.5f);
    s.vec[i] = sin(pEuler->vec[i] * 0.5f);
  }
  pDst->w = c.x * c.y * c.z + s.x * s.y * s.z;
  pDst->x = s.x * c.y * c.z - c.x * s.y * s.z;
  pDst->y = c.x * s.y * c.z + s.x * c.y * s.z;
  pDst->z = c.x * c.y * s.z - s.x * s.y * c.z;
}
static inline void vkmQuatMul(const quat* pSrc, const quat* pMul, quat* pDst) {
  pDst->w = pSrc->w * pMul->w - pSrc->x * pMul->x - pSrc->y * pMul->y - pSrc->z * pMul->z;
  pDst->x = pSrc->w * pMul->x + pSrc->x * pMul->w + pSrc->y * pMul->z - pSrc->z * pMul->y;
  pDst->y = pSrc->w * pMul->y + pSrc->y * pMul->w + pSrc->z * pMul->x - pSrc->x * pMul->z;
  pDst->z = pSrc->w * pMul->z + pSrc->z * pMul->w + pSrc->x * pMul->y - pSrc->y * pMul->x;
}

//----------------------------------------------------------------------------------
// Image Barriers
//----------------------------------------------------------------------------------

//typedef enum QueueBarrier {
//  QUEUE_BARRIER_IGNORE,
//  QUEUE_BARRIER_GRAPHICS,
//  QUEUE_BARRIER_COMPUTE,
//  QUEUE_BARRIER_TRANSITION,
//  QUEUE_BARRIER_FAMILY_EXTERNAL,
//} QueueBarrier;
//static uint32_t GetQueueIndex(const QueueBarrier queueBarrier) {
//  switch (queueBarrier) {
//    default:
//    case QUEUE_BARRIER_IGNORE:          return VK_QUEUE_FAMILY_IGNORED;
//    case QUEUE_BARRIER_GRAPHICS:        return context.graphicsQueueFamilyIndex;
//    case QUEUE_BARRIER_COMPUTE:         return context.computeQueueFamilyIndex;
//    case QUEUE_BARRIER_TRANSITION:      return context.transferQueueFamilyIndex;
//    case QUEUE_BARRIER_FAMILY_EXTERNAL: return VK_QUEUE_FAMILY_EXTERNAL;
//  }
//}
typedef struct VkmImageBarrier {
  VkPipelineStageFlagBits2 stageMask;
  VkAccessFlagBits2        accessMask;
  VkImageLayout            layout;
  // QueueBarrier             queueFamily;
} VkmImageBarrier;
static const VkmImageBarrier* VKM_IMAGE_BARRIER_UNDEFINED = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
};
static const VkmImageBarrier* VKM_IMAGE_BARRIER_PRESENT = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
static const VkmImageBarrier* VKM_TRANSFER_SRC_IMAGE_BARRIER = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
};
static const VkmImageBarrier* VKM_TRANSFER_DST_IMAGE_BARRIER = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
};
static const VkmImageBarrier* VKM_SHADER_READ_IMAGE_BARRIER = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_COLOR_ATTACHMENT_IMAGE_BARRIER = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
};
#define VKM_IMAGE_BARRIER(src, dst, aspect_mask, barrier_image) \
  (const VkImageMemoryBarrier2) {                               \
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,          \
    .srcStageMask = src->stageMask,                             \
    .srcAccessMask = src->accessMask,                           \
    .dstStageMask = dst->stageMask,                             \
    .dstAccessMask = dst->accessMask,                           \
    .oldLayout = src->layout,                                   \
    .newLayout = dst->layout,                                   \
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,             \
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,             \
    .image = barrier_image,                                     \
    .subresourceRange = (VkImageSubresourceRange){              \
        .aspectMask = aspect_mask,                              \
        .levelCount = 1,                                        \
        .layerCount = 1,                                        \
    },                                                          \
  }
static inline void vkmCommandPipelineImageBarrier(const VkCommandBuffer commandBuffer, const uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers) {
  vkCmdPipelineBarrier2(commandBuffer, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}
static inline void vkmBlit(const VkCommandBuffer cmd, const VkImage srcImage, const VkImage dstImage) {
  const VkImageBlit imageBlit = {
      .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .srcOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_WIDTH, .z = 1}},
      .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .dstOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_WIDTH, .z = 1}},
  };
  vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
}
static inline void vkmCmdBeginPass(const VkCommandBuffer cmd, const VkRenderPass renderPass, const VkFramebuffer framebuffer) {
  const VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass,
      .framebuffer = framebuffer,
      .renderArea = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
      .clearValueCount = VKM_PASS_ATTACHMENT_STD_COUNT,
      .pClearValues = (const VkClearValue[]){
          [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = {.color = {VKM_PASS_CLEAR_COLOR}},
          [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
          [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = {.depthStencil = {0.0f}},
      },
  };
  vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}
static inline void vkmCmdResetBegin(const VkCommandBuffer commandBuffer) {
  vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkBeginCommandBuffer(commandBuffer, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO});
  vkCmdSetViewport(commandBuffer, 0, 1, &(const VkViewport){.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f});
  vkCmdSetScissor(commandBuffer, 0, 1, &(const VkRect2D){.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}});
}
static inline void vkmSubmitPresentCommandBuffer(const VkCommandBuffer cmd, const VkQueue queue, const VkmSwap* pSwap, VkmTimeline* pTimeline) {
  const uint64_t     waitValue = pTimeline->waitValue++;
  const uint64_t     signalValue = pTimeline->waitValue;
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &(const VkTimelineSemaphoreSubmitInfo){
          .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
          .waitSemaphoreValueCount = 2,
          .pWaitSemaphoreValues = (uint64_t[]){waitValue, 0},
          .signalSemaphoreValueCount = 2,
          .pSignalSemaphoreValues = (uint64_t[]){signalValue, 0},
      },
      .waitSemaphoreCount = 2,
      .pWaitSemaphores = (const VkSemaphore[]){
          pTimeline->semaphore,
          pSwap->acquireSemaphore,
      },
      .pWaitDstStageMask = (const VkPipelineStageFlags[]){
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      },
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 2,
      .pSignalSemaphores = (const VkSemaphore[]){
          pTimeline->semaphore,
          pSwap->renderCompleteSemaphore,
      },
  };
  VKM_REQUIRE(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  const VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &pSwap->renderCompleteSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &pSwap->chain,
      .pImageIndices = &pSwap->swapIndex,
  };
  VKM_REQUIRE(vkQueuePresentKHR(queue, &presentInfo));
}
static inline void vkmTimelineWait(const VkDevice device, const VkmTimeline* pTimeline) {
  const VkSemaphoreWaitInfo semaphoreWaitInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores = &pTimeline->semaphore,
      .pValues = &pTimeline->waitValue,
  };
  VKM_REQUIRE(vkWaitSemaphores(device, &semaphoreWaitInfo, UINT64_MAX));
}

static inline void vkmUpdateGlobalSet(VkmTransform* pCameraTransform, VkmGlobalSetState* pState, VkmGlobalSetState* pMapped) {
  pCameraTransform->position = (vec3){0.0f, 0.0f, 2.0f};
  pCameraTransform->euler = (vec3){0.0f, 0.0f, 0.0f};
  pState->screenSize = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT};
  vkmMat4Perspective(45.0f, DEFAULT_WIDTH / DEFAULT_HEIGHT, 0.1f, 100.0f, &pState->projection);
  vkmVec3EulerToQuat(&pCameraTransform->euler, &pCameraTransform->rotation);
  vkmMat4Inv(&pState->projection, &pState->inverseProjection);
  vkmMat4FromTransform(&pCameraTransform->position, &pCameraTransform->rotation, &pState->inverseView);
  vkmMat4Inv(&pState->inverseView, &pState->view);
  vkmMat4Mul(&pState->projection, &pState->view, &pState->viewProjection);
  vkmMat4Mul(&pState->inverseView, &pState->inverseViewProjection, &pState->inverseViewProjection);
  memcpy(pMapped, pState, sizeof(VkmGlobalSetState));
}
static inline void vkmUpdateGlobalSetView(VkmTransform* pCameraTransform, VkmGlobalSetState* pState, VkmGlobalSetState* pMapped) {
  vkmMat4FromTransform(&pCameraTransform->position, &pCameraTransform->rotation, &pState->inverseView);
  vkmMat4Inv(&pState->inverseView, &pState->view);
  vkmMat4Mul(&pState->projection, &pState->view, &pState->viewProjection);
  vkmMat4Mul(&pState->inverseView, &pState->inverseViewProjection, &pState->inverseViewProjection);
  memcpy(pMapped, pState, sizeof(VkmGlobalSetState));
}
static inline void vkmUpdateObjectSet(VkmTransform* pTransform, VkmStandardObjectSetState* pState, VkmStandardObjectSetState* pSphereObjectSetMapped) {
  vkmVec3EulerToQuat(&pTransform->euler, &pTransform->rotation);
  vkmMat4FromTransform(&pTransform->position, &pTransform->rotation, &pState->model);
  memcpy(pSphereObjectSetMapped, pState, sizeof(VkmStandardObjectSetState));
}

static inline bool vkmProcessInput(VkmTransform* pCameraTransform) {
  bool inputDirty = false;
  if (input.mouseLocked) {
    pCameraTransform->euler.y -= input.mouseDeltaX * input.mouseLocked * input.deltaTime * 4.0f;
    vkmVec3EulerToQuat(&pCameraTransform->euler, &pCameraTransform->rotation);
    inputDirty = true;
  }
  if (input.moveForward || input.moveBack || input.moveLeft || input.moveRight) {
    vec3 localTranslate = {.x = input.moveRight - input.moveLeft, .z = input.moveBack - input.moveForward};
    vkmVec3Rot(&localTranslate, &pCameraTransform->rotation, &localTranslate);
    const float moveSensitivity = input.deltaTime * 4.0f;
    for (int i = 0; i < 3; ++i) pCameraTransform->position.vec[i] += localTranslate.vec[i] * moveSensitivity;
    inputDirty = true;
  }
  return inputDirty;
}

void VkmCreateFramebuffers(const VkRenderPass renderPass, const uint32_t framebufferCount, VkmFramebuffer* pFrameBuffers);
void VkmCreateSphereMesh(const VkmCommandContext* pCommand, const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh);
void VkmAllocateDescriptorSet(const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet);
void VkmCreateAllocateBindMapBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer, void** ppMapped);
void VkmCreateTextureFromFile(const VkmCommandContext* pCommand, const char* pPath, VkmTexture* pTexture);
void VkmCreateStandardPipeline(const VkRenderPass renderPass, VkmStandardPipe* pStandardPipeline);

typedef struct VkmInstance {
  VkInstance               instance;
  VkDebugUtilsMessengerEXT debugUtilsMessenger;
  VkSurfaceKHR             surface;
} VkmInstance;
void vkmCreateInstance(const bool createSurface);

typedef struct VkmContextCreateInfo {
  bool highPriority;
  bool createGraphicsCommand;
  bool createComputeCommand;
  bool createTransferCommand;
} VkmContextCreateInfo;
typedef struct VkmSamplerDesc {
  VkFilter               filter;
  VkSamplerAddressMode   addressMode;
  VkSamplerReductionMode reductionMode;
} VkmSamplerDesc;
#define VKM_SAMPLER_LINEAR_CLAMP_DESC (VkmSamplerDesc) { .filter = VK_FILTER_LINEAR, .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE }

extern _Thread_local VkmContext context;

void vkmCreateContext(const VkmContextCreateInfo* pCreateInfo);
void VkmContextCreateSampler(const VkmSamplerDesc* pSamplerDesc, VkSampler* pSampler);
void VkmCreateStandardRenderPass(VkRenderPass* pRenderPass);
void VkmContextCreateSwap(VkmSwap* pSwap);