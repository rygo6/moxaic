#pragma once

#include <stdbool.h>
#include <vulkan/vulkan.h>

#define VK_ALLOC      NULL
#define VK_SWAP_COUNT 2

#define VK_VERSION VK_MAKE_API_VERSION(0, 1, 3, 2)
#define VK_REQUIRE(command)                                 \
  {                                                         \
    VkResult result = command;                              \
    REQUIRE(result == VK_SUCCESS, string_VkResult(result)); \
  }

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
  VkSemaphore sempahore;
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

enum RenderPassAttachment {
  RENDERPASS_COLOR_ATTACHMENT,
  RENDERPASS_NORMAL_ATTACHMENT,
  RENDERPASS_DEPTH_ATTACHMENT,
  RENDERPASS_ATTACHMENT_COUNT,
};
static const VkClearValue RenderPassClearValues[] = {
    [RENDERPASS_COLOR_ATTACHMENT] = {.color = {{0.1f, 0.2f, 0.3f, 0.0f}}},
    [RENDERPASS_NORMAL_ATTACHMENT] = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
    [RENDERPASS_DEPTH_ATTACHMENT] = {.depthStencil = {0.0f, 0}},
};

#define MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

#define PIPE_VERTEX_BINDING_STANDARD 0
enum StandardPipeSetBinding {
  PIPE_SET_BINDING_STANDARD_GLOBAL,
  PIPE_SET_BINDING_STANDARD_MATERIAL,
  PIPE_SET_BINDING_STANDARD_OBJECT,
  PIPE_SET_BINDING_STANDARD_COUNT,
};

bool ProcessInput(Transform* pCameraTransform);
void UpdateGlobalSetView(Transform* pCameraTransform, GlobalSetState* pState, GlobalSetState* pMapped);
void ResetBeginCommandBuffer(const VkCommandBuffer commandBuffer);
void BeginRenderPass(const VkCommandBuffer cmd, const VkRenderPass, const VkFramebuffer framebuffer, const VkClearValue* pClearValues);
void Blit(const VkCommandBuffer cmd, const VkImage srcImage, const VkImage dstImage);
void SubmitPresentCommandBuffer(const VkCommandBuffer cmd, const VkQueue queue, const Swap* pSwap, Timeline* pTimeline);

void mxcInitContext();
void mxcRenderNodeInit();
void mxcRenderNodeUpdate();
