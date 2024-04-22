#pragma once

#include "globals.h"
#include "window.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define VK_ALLOC      NULL
#define VK_VERSION    VK_MAKE_API_VERSION(0, 1, 3, 2)
#define VK_SWAP_COUNT 2
#define VK_REQUIRE(command)                                 \
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
#define VK_PFN_LOAD(vkFunction)                                                                         \
  PFN_##vkFunction vkFunction = (PFN_##vkFunction)vkGetInstanceProcAddr(context.instance, #vkFunction); \
  REQUIRE(vkFunction != NULL, "Couldn't load " #vkFunction)

#define SIMD_TYPE(type, name, count) typedef type name##_simd __attribute__((vector_size(sizeof(type) * count)))
SIMD_TYPE(float, float2, 2);
SIMD_TYPE(uint32_t, int2, 2);
SIMD_TYPE(float, float3, 4);
SIMD_TYPE(uint32_t, int3, 4);
SIMD_TYPE(float, float4, 4);
SIMD_TYPE(uint32_t, int4, 4);
SIMD_TYPE(float, mat4, 16);

#define PI 3.14159265358979323846f
#define MATH_UNION(type, simd_type, name, align, count, vec_name, ...) \
  typedef union __attribute((aligned(align))) name {                   \
    type vec_name[count];                                              \
    struct {                                                           \
      type __VA_ARGS__;                                                \
    };                                                                 \
    simd_type simd;                                                    \
  } name;
MATH_UNION(float, float2_simd, vec2, 8, 2, vec, x, y);
MATH_UNION(uint32_t, int2_simd, ivec2, 16, 2, vec, x, y);
MATH_UNION(float, float3_simd, vec3, 16, 3, vec, x, y, z);
MATH_UNION(uint32_t, int3_simd, ivec3, 16, 3, vec, x, y, z);
MATH_UNION(float, float4_simd, vec4, 16, 4, vec, x, y, z, w);
MATH_UNION(uint32_t, int4_simd, ivec4, 16, 4, vec, x, y, z, w);
MATH_UNION(float, float4_simd, mat4_row, 16, 4, row, r0, r1, r2, r3);
MATH_UNION(mat4_row, mat4_simd, mat4, 16, 4, col, c0, c1, c2, c3);
typedef vec4 quat;

typedef struct Timeline {
  VkSemaphore semaphore;
  uint64_t    waitValue;
} Timeline;

typedef struct Swap {
  VkSwapchainKHR chain;
  VkSemaphore    acquireSemaphore;
  VkSemaphore    renderCompleteSemaphore;
  uint32_t       index;
} Swap;

typedef struct Transform {
  vec3 position;
  vec3 euler;
  vec4 rotation;
} Transform;

typedef struct Texture {
  VkImage        image;
  VkImageView    imageView;
  VkDeviceMemory imageMemory;
  VkFormat       format;
  VkExtent3D     extent;
} Texture;

typedef struct Mesh {
  uint32_t       indexCount;
  VkDeviceMemory indexMemory;
  VkBuffer       indexBuffer;
  uint32_t       vertexCount;
  VkDeviceMemory vertexMemory;
  VkBuffer       vertexBuffer;
} Mesh;

typedef struct Framebuffer {
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

} Framebuffer;

typedef struct GlobalSetState {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;

  mat4 inverseView;
  mat4 inverseProjection;
  mat4 inverseViewProjection;

  ivec2 screenSize;
} GlobalSetState;

typedef struct StandardObjectSetState {
  mat4 model;
} StandardObjectSetState;

typedef struct MvkContext {
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VkPhysicalDevice physicalDevice;

  VkDevice device;

  VkRenderPass renderPass;

  Timeline timeline;

  Swap        swap;
  VkImage     swapImages[VK_SWAP_COUNT];
  VkImageView swapImageViews[VK_SWAP_COUNT];

  VkDescriptorSetLayout globalSetLayout;
  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSetLayout objectSetLayout;

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkSampler nearestSampler;
  VkSampler linearSampler;
  VkSampler minSampler;
  VkSampler maxSampler;

  VkCommandPool   graphicsCommandPool;
  VkCommandBuffer graphicsCommandBuffer;
  uint32_t        graphicsQueueFamilyIndex;
  VkQueue         graphicsQueue;

  VkCommandPool   computeCommandPool;
  VkCommandBuffer computeCommandBuffer;
  uint32_t        computeQueueFamilyIndex;
  VkQueue         computeQueue;

  uint32_t transferQueueFamilyIndex;
  VkQueue  transferQueue;

  VkDescriptorPool descriptorPool;

  VkQueryPool timeQueryPool;

  VkDebugUtilsMessengerEXT debugUtilsMessenger;

} MvkContext;

//----------------------------------------------------------------------------------
// Render
//----------------------------------------------------------------------------------

enum PassAttachment {
  PASS_COLOR_ATTACHMENT,
  PASS_NORMAL_ATTACHMENT,
  PASS_DEPTH_ATTACHMENT,
  PASS_ATTACHMENT_COUNT,
};
#define PASS_CLEAR_COLOR \
  { 0.1f, 0.2f, 0.3f, 0.0f }

#define MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

// pipe should probably go in a diff file
#define PIPE_VERTEX_BINDING_STANDARD 0
enum StandardPipeSetBinding {
  PIPE_SET_BINDING_STANDARD_GLOBAL,
  PIPE_SET_BINDING_STANDARD_MATERIAL,
  PIPE_SET_BINDING_STANDARD_OBJECT,
  PIPE_SET_BINDING_STANDARD_COUNT,
};

#define SET_BINDING_GLOBAL_BUFFER             0
#define SET_BINDING_STANDARD_MATERIAL_TEXTURE 0
#define SET_BINDING_STANDARD_OBJECT_BUFFER    0
#define SET_WRITE_GLOBAL(globalSet, globalSetBuffer)     \
  {                                                      \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
    .dstSet = globalSet,                                 \
    .dstBinding = SET_BINDING_GLOBAL_BUFFER,             \
    .descriptorCount = 1,                                \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(const VkDescriptorBufferInfo){      \
        .buffer = globalSetBuffer,                       \
        .range = sizeof(GlobalSetState),                 \
    },                                                   \
  }
#define SET_WRITE_STANDARD_MATERIAL(materialSet, material_image_view, material_image_sample) \
  {                                                                                          \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                                         \
    .dstSet = materialSet,                                                                   \
    .dstBinding = SET_BINDING_STANDARD_MATERIAL_TEXTURE,                                     \
    .descriptorCount = 1,                                                                    \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                             \
    .pImageInfo = &(const VkDescriptorImageInfo){                                            \
        .sampler = material_image_sample,                                                    \
        .imageView = material_image_view,                                                    \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                             \
    },                                                                                       \
  }
#define SET_WRITE_STANDARD_OBJECT(objectSet, objectSetBuffer) \
  {                                                           \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,          \
    .dstSet = objectSet,                                      \
    .dstBinding = SET_BINDING_STANDARD_OBJECT_BUFFER,         \
    .descriptorCount = 1,                                     \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      \
    .pBufferInfo = &(const VkDescriptorBufferInfo){           \
        .buffer = objectSetBuffer,                            \
        .range = sizeof(StandardObjectSetState),              \
    },                                                        \
  }

//----------------------------------------------------------------------------------
// Math
//----------------------------------------------------------------------------------

enum VecElement {
  VEC_X, VEC_Y, VEC_Z, VEC_W,
  VEC_COUNT,
};
enum MatElement {
  MAT_C0_R0, MAT_C0_R1, MAT_C0_R2, MAT_C0_R3,
  MAT_C1_R0, MAT_C1_R1, MAT_C1_R2, MAT_C1_R3,
  MAT_C2_R0, MAT_C2_R1, MAT_C2_R2, MAT_C2_R3,
  MAT_C3_R0, MAT_C3_R1, MAT_C3_R2, MAT_C3_R3,
  MAT_COUNT,
};
#define VEC3_ITERATE for (int i = 0; i < 3; ++i)
#define VEC4_ITERATE for (int i = 0; i < 4; ++i)

static const quat QUAT_IDENT = {0.0f, 0.0f, 0.0f, 1.0f};
static const mat4 MAT4_IDENT = {
    .c0 = {1.0f, 0.0f, 0.0f, 0.0f},
    .c1 = {0.0f, 1.0f, 0.0f, 0.0f},
    .c2 = {0.0f, 0.0f, 1.0f, 0.0f},
    .c3 = {0.0f, 0.0f, 0.0f, 1.0f},
};

#define SIMD_NIL               -1
#define SIMD_SHUFFLE(vec, ...) __builtin_shufflevector(vec, vec, __VA_ARGS__)

INLINE float mvkFloat4Sum(float4_simd* pFloat4) {
  // appears to make better SIMD assembly than a loop:
  // https://godbolt.org/z/6jWe4Pj5b
  // https://godbolt.org/z/M5Goq57vj
  float4_simd shuf = SIMD_SHUFFLE(*pFloat4, 2, 3, 0, 1);
  float4_simd sums = *pFloat4 + shuf;
  shuf = SIMD_SHUFFLE(sums, 1, 0, 3, 2);
  sums = sums + shuf;
  return sums[0];
}

INLINE void mvkMat4Translation(const vec3* pTranslationVec3, mat4* pDstMat4) {
  VEC3_ITERATE pDstMat4->col[3].simd += MAT4_IDENT.col[i].simd * pTranslationVec3->vec[i];
}
INLINE float mvkVec4Dot(const vec4* l, const vec4* r) {
  float4_simd product = l->simd * r->simd;
  return mvkFloat4Sum(&product);
}
INLINE float mvkVec4Mag(const vec4* v) {
  return sqrtf(mvkVec4Dot(v, v));
}
INLINE void mvkQuatToMat4(const quat* pQuat, mat4* pDst) {
  const float norm = mvkVec4Mag(pQuat);
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
INLINE void mvkMat4MulRot(const mat4* pSrc, const mat4* pRot, mat4* pDst) {
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
INLINE void mvkMat4Mul(const mat4* pLeft, const mat4* pRight, mat4* pDst) {
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
INLINE void mvkMat4Scale(const float scale, mat4* pDst) {
  pDst->simd *= scale;
}
INLINE void mvkMat4Inv(const mat4* pSrc, mat4* pDst) {
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

  mvkMat4Scale(det, pDst);
}
INLINE void mvkMat4FromTransform(const vec3* pPos, const quat* pRot, mat4* pDst) {
  mat4 translationMat4 = MAT4_IDENT;
  mvkMat4Translation(pPos, &translationMat4);
  mat4 rotationMat4 = MAT4_IDENT;
  mvkQuatToMat4(pRot, &rotationMat4);
  mvkMat4MulRot(&translationMat4, &rotationMat4, pDst);
}
// Perspective matrix in Vulkan Reverse Z
INLINE void mvkMat4Perspective(const float fov, const float aspect, const float zNear, const float zFar, mat4* pDstMat4) {
  const float tanHalfFovy = tan(fov / 2.0f);
  *pDstMat4 = (mat4){.c0 = {.r0 = 1.0f / (aspect * tanHalfFovy)},
                     .c1 = {.r1 = 1.0f / tanHalfFovy},
                     .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
                     .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)}};
}
INLINE void mvkVec3Cross(const vec3* pLeft, const vec3* pRight, vec3* pDst) {
  *pDst = (vec3){pLeft->y * pRight->z - pRight->y * pLeft->z,
                 pLeft->z * pRight->x - pRight->z * pLeft->x,
                 pLeft->x * pRight->y - pRight->x * pLeft->y};
}
INLINE void mvkVec3Rot(const vec3* pSrc, const quat* pRot, vec3* pDst) {
  vec3 uv, uuv;
  mvkVec3Cross((vec3*)pRot, pSrc, &uv);
  mvkVec3Cross((vec3*)pRot, &uv, &uuv);
  for (int i = 0; i < 3; ++i) pDst->vec[i] = pSrc->vec[i] + ((uv.vec[i] * pRot->w) + uuv.vec[i]) * 2.0f;
}
INLINE void mvkVec3EulerToQuat(const vec3* pEuler, quat* pDst) {
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
INLINE void mvkQuatMul(const quat* pSrc, const quat* pMul, quat* pDst) {
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
typedef struct ImageBarrier {
  VkPipelineStageFlagBits2 stageMask;
  VkAccessFlagBits2        accessMask;
  VkImageLayout            layout;
  // QueueBarrier             queueFamily;
} ImageBarrier;
static const ImageBarrier UNDEFINED_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
};
static const ImageBarrier PRESENT_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
static const ImageBarrier TRANSFER_SRC_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
};
static const ImageBarrier TRANSFER_DST_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
};
static const ImageBarrier SHADER_READ_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const ImageBarrier COLOR_ATTACHMENT_IMAGE_BARRIER = {
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
};
INLINE void EmplaceImageBarrier(const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image, VkImageMemoryBarrier2* pImageMemoryBarrier) {
  *pImageMemoryBarrier = (VkImageMemoryBarrier2){
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = pSrc->stageMask,
      .srcAccessMask = pSrc->accessMask,
      .dstStageMask = pDst->stageMask,
      .dstAccessMask = pDst->accessMask,
      .oldLayout = pSrc->layout,
      .newLayout = pDst->layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = (VkImageSubresourceRange){
          .aspectMask = aspectMask,
          .levelCount = 1,
          .layerCount = 1,
      },
  };
}
INLINE void CommandPipelineBarrier(const VkCommandBuffer commandBuffer, const uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers) {
  vkCmdPipelineBarrier2(commandBuffer, &(VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}
INLINE void CommandImageBarrier(const VkCommandBuffer commandBuffer, const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image) {
  VkImageMemoryBarrier2 transferImageMemoryBarrier;
  EmplaceImageBarrier(pSrc, pDst, aspectMask, image, &transferImageMemoryBarrier);
  CommandPipelineBarrier(commandBuffer, 1, &transferImageMemoryBarrier);
}

INLINE void mxcBlit(const VkCommandBuffer cmd, const VkImage srcImage, const VkImage dstImage) {
  VkImageMemoryBarrier2 toBlitBarrier[2];
  EmplaceImageBarrier(&COLOR_ATTACHMENT_IMAGE_BARRIER, &TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, srcImage, &toBlitBarrier[0]);
  EmplaceImageBarrier(&PRESENT_IMAGE_BARRIER, &TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, dstImage, &toBlitBarrier[1]);
  CommandPipelineBarrier(cmd, 2, toBlitBarrier);

  const VkImageSubresourceLayers imageSubresourceLayers = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .layerCount = 1,
  };
  const VkOffset3D offsets[2] = {
      {.x = 0, .y = 0, .z = 0},
      {.x = DEFAULT_WIDTH, .y = DEFAULT_WIDTH, .z = 1},
  };
  const VkImageBlit imageBlit = {
      .srcSubresource = imageSubresourceLayers,
      .srcOffsets = {offsets[0], offsets[1]},
      .dstSubresource = imageSubresourceLayers,
      .dstOffsets = {offsets[0], offsets[1]},
  };
  vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);

  VkImageMemoryBarrier2 toPresentBarrier[2];
  EmplaceImageBarrier(&TRANSFER_SRC_IMAGE_BARRIER, &COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, srcImage, &toPresentBarrier[0]);
  EmplaceImageBarrier(&TRANSFER_DST_IMAGE_BARRIER, &PRESENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, dstImage, &toPresentBarrier[1]);
  CommandPipelineBarrier(cmd, 2, toPresentBarrier);
}

INLINE void mxcBeginPass(const VkCommandBuffer cmd, const VkRenderPass renderPass, const VkFramebuffer framebuffer) {
  const VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass,
      .framebuffer = framebuffer,
      .renderArea = (const VkRect2D){.extent = (const VkExtent2D){.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
      .clearValueCount = PASS_ATTACHMENT_COUNT,
      .pClearValues = (const VkClearValue[]){
          {.color = (const VkClearColorValue){PASS_CLEAR_COLOR}},
          {.color = (const VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}}},
          {.depthStencil = (const VkClearDepthStencilValue){0.0f, 0}},
      },
  };
  vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}
INLINE void mxcResetBeginCommandBuffer(const VkCommandBuffer commandBuffer) {
  vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO});
  vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport){.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f});
  vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D){.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}});
}
INLINE void mxcSubmitPresentCommandBuffer(const VkCommandBuffer cmd, const VkQueue queue, const Swap* pSwap, Timeline* pTimeline) {
  const uint64_t     waitValue = pTimeline->waitValue++;
  const uint64_t     signalValue = pTimeline->waitValue;
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &(const VkTimelineSemaphoreSubmitInfo){
          .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
          .waitSemaphoreValueCount = 2,
          .pWaitSemaphoreValues = (const uint64_t[]){waitValue, 0},
          .signalSemaphoreValueCount = 2,
          .pSignalSemaphoreValues = (const uint64_t[]){signalValue, 0},
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
  VK_REQUIRE(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  const VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &pSwap->renderCompleteSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &pSwap->chain,
      .pImageIndices = &pSwap->index,
  };
  VK_REQUIRE(vkQueuePresentKHR(queue, &presentInfo));
}
INLINE void TimelineWait(const VkDevice device, const Timeline timeline) {
  const VkSemaphoreWaitInfo semaphoreWaitInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores = &timeline.semaphore,
      .pValues = &timeline.waitValue,
  };
  VK_REQUIRE(vkWaitSemaphores(device, &semaphoreWaitInfo, UINT64_MAX));
}

INLINE void UpdateGlobalSet(Transform* pCameraTransform, GlobalSetState* pState, GlobalSetState* pMapped) {
  pCameraTransform->position = (vec3){0.0f, 0.0f, 2.0f};
  pCameraTransform->euler = (vec3){0.0f, 0.0f, 0.0f};
  pState->screenSize = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT};
  mvkMat4Perspective(45.0f, DEFAULT_WIDTH / DEFAULT_HEIGHT, 0.1f, 100.0f, &pState->projection);
  mvkVec3EulerToQuat(&pCameraTransform->euler, &pCameraTransform->rotation);
  mvkMat4Inv(&pState->projection, &pState->inverseProjection);
  mvkMat4FromTransform(&pCameraTransform->position, &pCameraTransform->rotation, &pState->inverseView);
  mvkMat4Inv(&pState->inverseView, &pState->view);
  mvkMat4Mul(&pState->projection, &pState->view, &pState->viewProjection);
  mvkMat4Mul(&pState->inverseView, &pState->inverseViewProjection, &pState->inverseViewProjection);
  memcpy(pMapped, pState, sizeof(GlobalSetState));
}
INLINE void mxcUpdateGlobalSetView(Transform* pCameraTransform, GlobalSetState* pState, GlobalSetState* pMapped) {
  mvkMat4FromTransform(&pCameraTransform->position, &pCameraTransform->rotation, &pState->inverseView);
  mvkMat4Inv(&pState->inverseView, &pState->view);
  mvkMat4Mul(&pState->projection, &pState->view, &pState->viewProjection);
  mvkMat4Mul(&pState->inverseView, &pState->inverseViewProjection, &pState->inverseViewProjection);
  memcpy(pMapped, pState, sizeof(GlobalSetState));
}
INLINE void UpdateObjectSet(Transform* pTransform, StandardObjectSetState* pState, StandardObjectSetState* pSphereObjectSetMapped) {
  mvkVec3EulerToQuat(&pTransform->euler, &pTransform->rotation);
  mvkMat4FromTransform(&pTransform->position, &pTransform->rotation, &pState->model);
  memcpy(pSphereObjectSetMapped, pState, sizeof(StandardObjectSetState));
}

INLINE bool mxcProcessInput(Transform* pCameraTransform) {
  bool inputDirty = false;
  if (input.mouseLocked) {
    pCameraTransform->euler.y -= input.mouseDeltaX * input.mouseLocked;
    mvkVec3EulerToQuat(&pCameraTransform->euler, &pCameraTransform->rotation);
    inputDirty = true;
  }
  if (input.moveForward || input.moveBack || input.moveLeft || input.moveRight) {
    vec3 localTranslate = {.x = input.moveRight - input.moveLeft, .z = input.moveBack - input.moveForward};
    mvkVec3Rot(&localTranslate, &pCameraTransform->rotation, &localTranslate);
    const float moveSensitivity = .0002f;
    for (int i = 0; i < 3; ++i) pCameraTransform->position.vec[i] += localTranslate.vec[i] * moveSensitivity;
    inputDirty = true;
  }
  return inputDirty;
}

void            CreateFramebuffers(const uint32_t framebufferCount, Framebuffer* pFrameBuffers);
void            CreateSphereMeshBuffers(const float radius, const int slicesCount, const int stackCount, Mesh* pMesh);
VkDescriptorSet AllocateDescriptorSet(const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet);
void            CreateAllocateBindMapBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer, void** ppMapped);
void            CreateTextureFromFile(const char* pPath, Texture* pTexture);

const MvkContext* mxcInitRendererContext();