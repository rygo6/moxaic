#include "renderer.h"
#include "globals.h"
#include "stb_image.h"
#include "window.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

//----------------------------------------------------------------------------------
// Global Constants
//----------------------------------------------------------------------------------

#define VK_VERSION    VK_MAKE_API_VERSION(0, 1, 3, 2)
#define VK_ALLOC      NULL
#define VK_SWAP_COUNT 2

//----------------------------------------------------------------------------------
// Math Types
//----------------------------------------------------------------------------------

#define PI 3.14159265358979323846f
#define MATH_UNION(type, name, align, count, arrName, ...) \
  typedef union __attribute((aligned(align))) name {       \
    type arrName[count];                                   \
    struct {                                               \
      type __VA_ARGS__;                                    \
    };                                                     \
  } name;
MATH_UNION(float, vec2, 16, 4, vec, x, y);
MATH_UNION(uint32_t, ivec2, 16, 4, vec, x, y);
MATH_UNION(float, vec3, 16, 4, vec, x, y, z);
MATH_UNION(uint32_t, ivec3, 16, 4, vec, x, y, z);
MATH_UNION(float, vec4, 16, 4, vec, x, y, z, w);
MATH_UNION(uint32_t, ivec4, 16, 4, vec, x, y, z, w);
MATH_UNION(float, mat4_row, 16, 4, row, r0, r1, r2, r3);
MATH_UNION(mat4_row, mat4, 16, 4, col, c0, c1, c2, c3);
typedef vec4 quat;

#define MATH_INLINE static inline __attribute((always_inline))

const quat quatIdentity = {0.0f, 0.0f, 0.0f, 1.0f};
const mat4 mat4Identity = {
    .c0 = {1.0f, 0.0f, 0.0f, 0.0f},
    .c1 = {0.0f, 1.0f, 0.0f, 0.0f},
    .c2 = {0.0f, 0.0f, 1.0f, 0.0f},
    .c3 = {0.0f, 0.0f, 0.0f, 1.0f},
};

//----------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------

typedef struct Vertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
} Vertex;
enum VertexAttributes {
  VERTEX_ATTR_POSITION,
  VERTEX_ATTR_NORMAL,
  VERTEX_ATTR_UV,
  VERTEX_ATTR_COUNT,
};

typedef struct Transform {
  vec3 position;
  vec4 rotation;
} Transform;

typedef struct Camera {
  vec3 position;
  vec4 rotation;
  bool cameraLocked;
} Camera;

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

static struct Context {
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VkPhysicalDevice physicalDevice;

  VkDevice device;

  VkSampler nearestSampler;
  VkSampler linearSampler;
  VkSampler minSampler;
  VkSampler maxSampler;

  VkRenderPass renderPass;

  GlobalSetState        globalSetMapped;
  VkDeviceMemory        globalSetMemory;
  VkBuffer              globalSetBuffer;
  VkDescriptorSetLayout globalSetLayout;
  VkDescriptorSet       globalSet;

  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSetLayout objectSetLayout;

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkSwapchainKHR swapchain;
  VkImage        swapImages[VK_SWAP_COUNT];
  VkImageView    swapImageViews[VK_SWAP_COUNT];

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

} context;

//----------------------------------------------------------------------------------
// Math Operations
//----------------------------------------------------------------------------------

// MATH_INLINE void Vec4MulAdd(const vec4* restrict pSrc, const float s, vec4* restrict pDst) {
//   for (int i = 0; i < 4; ++i) pDst->arr[i] += pSrc->arr[i] * s;
// }
MATH_INLINE void Mat4Translation(const vec3* pTranslationVec3, mat4* pDstMat4) {
  // can this be one loop?
  for (int i = 0; i < 4; ++i) pDstMat4->col[3].row[i] += mat4Identity.col[0].row[i] * pTranslationVec3->vec[0];
  for (int i = 0; i < 4; ++i) pDstMat4->col[3].row[i] += mat4Identity.col[1].row[i] * pTranslationVec3->vec[1];
  for (int i = 0; i < 4; ++i) pDstMat4->col[3].row[i] += mat4Identity.col[2].row[i] * pTranslationVec3->vec[2];
  // Vec4MulAdd((vec4*)&mat4Identity.c0, pTranslationVec3->x, (vec4*)&pDstMat4->c3);
  // Vec4MulAdd((vec4*)&mat4Identity.c1, pTranslationVec3->y, (vec4*)&pDstMat4->c3);
  // Vec4MulAdd((vec4*)&mat4Identity.c2, pTranslationVec3->z, (vec4*)&pDstMat4->c3);
}
MATH_INLINE float Vec4Dot(const vec4* l, const vec4* r) {
  float sum = 0;
  for (int i = 0; i < 4; ++i) sum += l->vec[i] * r->vec[i];
  return sum;
}
MATH_INLINE float Vec4Mag(const vec4* v) {
  return sqrtf(Vec4Dot(v, v));
}
MATH_INLINE void Mat4FromQuat(const quat* q, mat4* dst) {
  // VECTORIZE
  float w, x, y, z,
      xx, yy, zz,
      xy, yz, xz,
      wx, wy, wz,
      norm, s;

  norm = Vec4Mag(q);
  s = norm > 0.0f ? 2.0f / norm : 0.0f;
  x = q->x;
  y = q->y;
  z = q->z;
  w = q->w;

  xx = s * x * x;
  xy = s * x * y;
  wx = s * w * x;

  yy = s * y * y;
  yz = s * y * z;
  wy = s * w * y;

  zz = s * z * z;
  xz = s * x * z;
  wz = s * w * z;

  dst->c0.r0 = 1.0f - yy - zz;
  dst->c1.r1 = 1.0f - xx - zz;
  dst->c2.r2 = 1.0f - xx - yy;

  dst->c0.r1 = xy + wz;
  dst->c1.r2 = yz + wx;
  dst->c2.r0 = xz + wy;

  dst->c1.r0 = xy - wz;
  dst->c2.r1 = yz - wx;
  dst->c0.r2 = xz - wy;

  dst->c0.r3 = 0.0f;
  dst->c1.r3 = 0.0f;
  dst->c2.r3 = 0.0f;
  dst->c3.r0 = 0.0f;
  dst->c3.r1 = 0.0f;
  dst->c3.r2 = 0.0f;
  dst->c3.r3 = 1.0f;
}
MATH_INLINE void Mat4MulRot(const mat4* src, const mat4* rot, mat4* dst) {
  // VECTORIZE
  float a00 = src->c0.r0, a01 = src->c0.r1, a02 = src->c0.r2, a03 = src->c0.r3,
        a10 = src->c1.r0, a11 = src->c1.r1, a12 = src->c1.r2, a13 = src->c1.r3,
        a20 = src->c2.r0, a21 = src->c2.r1, a22 = src->c2.r2, a23 = src->c2.r3,
        a30 = src->c3.r0, a31 = src->c3.r1, a32 = src->c3.r2, a33 = src->c3.r3,

        b00 = rot->c0.r0, b01 = rot->c0.r1, b02 = rot->c0.r2,
        b10 = rot->c1.r0, b11 = rot->c1.r1, b12 = rot->c1.r2,
        b20 = rot->c2.r0, b21 = rot->c2.r1, b22 = rot->c2.r2;

  dst->c0.r0 = a00 * b00 + a10 * b01 + a20 * b02;
  dst->c0.r1 = a01 * b00 + a11 * b01 + a21 * b02;
  dst->c0.r2 = a02 * b00 + a12 * b01 + a22 * b02;
  dst->c0.r3 = a03 * b00 + a13 * b01 + a23 * b02;

  dst->c1.r0 = a00 * b10 + a10 * b11 + a20 * b12;
  dst->c1.r1 = a01 * b10 + a11 * b11 + a21 * b12;
  dst->c1.r2 = a02 * b10 + a12 * b11 + a22 * b12;
  dst->c1.r3 = a03 * b10 + a13 * b11 + a23 * b12;

  dst->c2.r0 = a00 * b20 + a10 * b21 + a20 * b22;
  dst->c2.r1 = a01 * b20 + a11 * b21 + a21 * b22;
  dst->c2.r2 = a02 * b20 + a12 * b21 + a22 * b22;
  dst->c2.r3 = a03 * b20 + a13 * b21 + a23 * b22;

  dst->c3.r0 = a30;
  dst->c3.r1 = a31;
  dst->c3.r2 = a32;
  dst->c3.r3 = a33;
}
MATH_INLINE void Mat4Mul(mat4* pLeft, mat4* pRight, mat4* pDest) {
  float a00 = pLeft->col[0].row[0], a01 = pLeft->col[0].row[1], a02 = pLeft->col[0].row[2], a03 = pLeft->col[0].row[3],
        a10 = pLeft->col[1].row[0], a11 = pLeft->col[1].row[1], a12 = pLeft->col[1].row[2], a13 = pLeft->col[1].row[3],
        a20 = pLeft->col[2].row[0], a21 = pLeft->col[2].row[1], a22 = pLeft->col[2].row[2], a23 = pLeft->col[2].row[3],
        a30 = pLeft->col[3].row[0], a31 = pLeft->col[3].row[1], a32 = pLeft->col[3].row[2], a33 = pLeft->col[3].row[3],

        b00 = pRight->col[0].row[0], b01 = pRight->col[0].row[1], b02 = pRight->col[0].row[2], b03 = pRight->col[0].row[3],
        b10 = pRight->col[1].row[0], b11 = pRight->col[1].row[1], b12 = pRight->col[1].row[2], b13 = pRight->col[1].row[3],
        b20 = pRight->col[2].row[0], b21 = pRight->col[2].row[1], b22 = pRight->col[2].row[2], b23 = pRight->col[2].row[3],
        b30 = pRight->col[3].row[0], b31 = pRight->col[3].row[1], b32 = pRight->col[3].row[2], b33 = pRight->col[3].row[3];

  pDest->col[0].row[0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
  pDest->col[0].row[1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
  pDest->col[0].row[2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
  pDest->col[0].row[3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
  pDest->col[1].row[0] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
  pDest->col[1].row[1] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
  pDest->col[1].row[2] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
  pDest->col[1].row[3] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
  pDest->col[2].row[0] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
  pDest->col[2].row[1] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
  pDest->col[2].row[2] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
  pDest->col[2].row[3] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
  pDest->col[3].row[0] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
  pDest->col[3].row[1] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
  pDest->col[3].row[2] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
  pDest->col[3].row[3] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
}
MATH_INLINE void Mat4Scale(mat4* pMat4, float scale) {
  // VECTORIZE
  pMat4->c0.r0 *= scale;
  pMat4->c0.r1 *= scale;
  pMat4->c0.r2 *= scale;
  pMat4->c0.r3 *= scale;
  pMat4->c1.r0 *= scale;
  pMat4->c1.r1 *= scale;
  pMat4->c1.r2 *= scale;
  pMat4->c1.r3 *= scale;
  pMat4->c2.r0 *= scale;
  pMat4->c2.r1 *= scale;
  pMat4->c2.r2 *= scale;
  pMat4->c2.r3 *= scale;
  pMat4->c3.r0 *= scale;
  pMat4->c3.r1 *= scale;
  pMat4->c3.r2 *= scale;
  pMat4->c3.r3 *= scale;
}
MATH_INLINE void Mat4Inv(const mat4* pSrc, mat4* pDst) {
  // VECTORIZE
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

  Mat4Scale(pDst, det);
}

MATH_INLINE void TransformToModelMat4(Transform* pTransform, mat4* pDstModelMat4) {
  mat4 translationMat4;
  Mat4Translation(&pTransform->position, &translationMat4);
  mat4 rotationMat4;
  Mat4FromQuat(&pTransform->rotation, &rotationMat4);
  Mat4MulRot(&translationMat4, &rotationMat4, pDstModelMat4);
}

// Perspective matrix in Vulkan Reverse Z
void Mat4Perspective(const float fovy, const float aspect, const float zNear, const float zFar, mat4* pDstMat4) {
  const float tanHalfFovy = tan(fovy / 2.0f);
  *pDstMat4 = (mat4){
      .c0 = {.r0 = 1.0f / (aspect * tanHalfFovy)},
      .c1 = {.r1 = 1.0f / tanHalfFovy},
      .c2 = {.r2 = zNear / (zFar - zNear), .r3 = -1.0f},
      .c3 = {.r2 = -(zNear * zFar) / (zNear - zFar)},
  };
}

//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

#define VK_REQUIRE(command)                                 \
  {                                                         \
    VkResult result = command;                              \
    REQUIRE(result == VK_SUCCESS, string_VkResult(result)); \
  }
// #define CLEANUP_RESULT(command, cleanup_goto)      \
//   result = command;                                \
//   if (__builtin_expect(result != VK_SUCCESS, 0)) { \
//     LOG_ERROR(command, string_VkResult(result));   \
//     goto cleanup_goto;                             \
//   }

#define PFN_LOAD(vkFunction)                                                                            \
  PFN_##vkFunction vkFunction = (PFN_##vkFunction)vkGetInstanceProcAddr(context.instance, #vkFunction); \
  assert(vkFunction != NULL && "Couldn't load " #vkFunction)

#define COUNT(array) sizeof(array) / sizeof(array[0])

typedef enum Support {
  SUPPORT_OPTIONAL,
  SUPPORT_YES,
  SUPPORT_NO,
} Support;
const char* string_Support[] = {
    [SUPPORT_OPTIONAL] = "SUPPORT_OPTIONAL",
    [SUPPORT_YES] = "SUPPORT_YES",
    [SUPPORT_NO] = "SUPPORT_NO",
};

static void ReadFile(const char* pPath, size_t* pFileLength, char** ppFileContents) {
  FILE* file = fopen(pPath, "rb");
  REQUIRE(file != NULL, "File can't be opened!");
  fseek(file, 0, SEEK_END);
  *pFileLength = ftell(file);
  rewind(file);
  *ppFileContents = malloc(*pFileLength * sizeof(char));
  const size_t readCount = fread(*ppFileContents, *pFileLength, 1, file);
  REQUIRE(readCount > 0, "Failed to read file!");
  fclose(file);
}

static VkBool32 DebugUtilsCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  switch (messageSeverity) {
    default:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("%s\n", pCallbackData->pMessage); return VK_FALSE;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: PANIC(pCallbackData->pMessage); return VK_FALSE;
  }
}

static VkCommandBuffer BeginImmediateCommandBuffer(const VkCommandPool commandPool) {
  VkCommandBuffer                   commandBuffer;
  const VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_REQUIRE(vkAllocateCommandBuffers(context.device, &allocateInfo, &commandBuffer));
  const VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_REQUIRE(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  return commandBuffer;
}
static void EndImmediateCommandBuffer(const VkCommandPool commandPool, const VkQueue graphicsQueue, VkCommandBuffer commandBuffer) {
  VK_REQUIRE(vkEndCommandBuffer(commandBuffer));
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  VK_REQUIRE(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  VK_REQUIRE(vkQueueWaitIdle(graphicsQueue));
  vkFreeCommandBuffers(context.device, commandPool, 1, &commandBuffer);
}

//----------------------------------------------------------------------------------
// Descriptors
//----------------------------------------------------------------------------------

static void CreateGlobalSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT,
      },
  };
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.globalSetLayout));
}
#define SET_WRITE_GLOBAL                                                                                                                                                      \
  {                                                                                                                                                                           \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = context.globalSet, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                                                                                                                           \
        .buffer = context.globalSetBuffer,                                                                                                                                    \
        .range = sizeof(GlobalSetState),                                                                                                                                      \
    },                                                                                                                                                                        \
  }

static void CreateStandardMaterialSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.materialSetLayout));
}
#define SET_WRITE_STANDARD_MATERIAL(materialSet, materialImageView)                                                                                                             \
  {                                                                                                                                                                             \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = materialSet, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                                                                                                                               \
        .sampler = context.linearSampler,                                                                                                                                       \
        .imageView = materialImageView,                                                                                                                                         \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                                                                                                                \
    },                                                                                                                                                                          \
  }

static void CreateStandardObjectSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.objectSetLayout));
}
#define SET_WRITE_STANDARD_OBJECT(objectSet, objectSetBuffer)                                                                                                         \
  {                                                                                                                                                                   \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = objectSet, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                                                                                                                   \
        .buffer = objectSetBuffer,                                                                                                                                    \
        .range = sizeof(StandardObjectSetState),                                                                                                                      \
    },                                                                                                                                                                \
  }

static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout* pSetLayout) {
  const VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = context.descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = pSetLayout,
  };
  VkDescriptorSet set;
  VK_REQUIRE(vkAllocateDescriptorSets(context.device, &allocateInfo, &set));
  return set;
}

//----------------------------------------------------------------------------------
// Pipelines
//----------------------------------------------------------------------------------

#define COLOR_WRITE_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

VkShaderModule CreateShaderModule(const char* pShaderPath) {
  size_t codeSize;
  char*  pCode;
  ReadFile(pShaderPath, &codeSize, &pCode);
  const VkShaderModuleCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = codeSize,
      .pCode = (uint32_t*)pCode,
  };
  VkShaderModule shaderModule;
  VK_REQUIRE(vkCreateShaderModule(context.device, &createInfo, VK_ALLOC, &shaderModule));
  free(pCode);
  return shaderModule;
}

// Statndard Pipeline
#define STANDARD_VERTEX_BINDING 0
enum StandardSetBinding {
  STANDARD_SET_BINDING_GLOBAL,
  STANDARD_SET_BINDING_STANDARD_MATERIAL,
  STANDARD_SET_BINDING_STANDARD_OBJECT,
  STANDARD_SET_BINDING_STANDARD_COUNT,
};
void CreateStandardPipelineLayout() {
  VkDescriptorSetLayout pSetLayouts[STANDARD_SET_BINDING_STANDARD_COUNT];
  pSetLayouts[STANDARD_SET_BINDING_GLOBAL] = context.globalSetLayout;
  pSetLayouts[STANDARD_SET_BINDING_STANDARD_MATERIAL] = context.materialSetLayout;
  pSetLayouts[STANDARD_SET_BINDING_STANDARD_OBJECT] = context.objectSetLayout;
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = STANDARD_SET_BINDING_STANDARD_COUNT,
      .pSetLayouts = pSetLayouts,
  };
  VK_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VK_ALLOC, &context.standardPipelineLayout));
}

const VkFormat          GBufferFormat = VK_FORMAT_R32_SFLOAT;
const VkImageUsageFlags GBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

typedef enum RenderPassAttachment {
  RENDERPASS_COLOR_ATTACHMENT,
  RENDERPASS_NORMAL_ATTACHMENT,
  RENDERPASS_DEPTH_ATTACHMENT,
  RENDERPASS_ATTACHMENT_COUNT,
} RenderPassAttachment;
const VkFormat RenderPassFormats[] = {
    [RENDERPASS_COLOR_ATTACHMENT] = VK_FORMAT_R8G8B8A8_UNORM,
    [RENDERPASS_NORMAL_ATTACHMENT] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [RENDERPASS_DEPTH_ATTACHMENT] = VK_FORMAT_D32_SFLOAT,
};
const VkImageUsageFlags RenderPassUsages[] = {
    [RENDERPASS_COLOR_ATTACHMENT] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [RENDERPASS_NORMAL_ATTACHMENT] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [RENDERPASS_DEPTH_ATTACHMENT] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
};
void CreateStandardPipeline() {
  const VkShaderModule vertShader = CreateShaderModule("./shaders/basic_material.vert.spv");
  const VkShaderModule fragShader = CreateShaderModule("./shaders/basic_material.frag.spv");

  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &(VkPipelineRobustnessCreateInfoEXT){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
          .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT,
      },
      .stageCount = 2,
      .pStages = (VkPipelineShaderStageCreateInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_VERTEX_BIT,
              .module = vertShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
              .module = fragShader,
              .pName = "main",
          },
      },
      .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          .vertexBindingDescriptionCount = 1,
          .pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
              {
                  .binding = STANDARD_VERTEX_BINDING,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
          .vertexAttributeDescriptionCount = 3,
          .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
              {
                  .location = VERTEX_ATTR_POSITION,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, position),
              },
              {
                  .location = VERTEX_ATTR_NORMAL,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, normal),
              },
              {
                  .location = VERTEX_ATTR_UV,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(Vertex, uv),
              },
          },
      },
      .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE,
      },
      .pViewportState = &(VkPipelineViewportStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          .viewportCount = 1,
          .scissorCount = 1,
      },
      .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .frontFace = VK_FRONT_FACE_CLOCKWISE,
          .lineWidth = 1.0f,
      },
      .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
          .maxDepthBounds = 1.0f,
      },
      .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOp = VK_LOGIC_OP_COPY,
          .attachmentCount = 2,
          .pAttachments = (VkPipelineColorBlendAttachmentState[]){
              {/* Color */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
              {/* Normal */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
          },
          .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
      },
      .pDynamicState = &(VkPipelineDynamicStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = 2,
          .pDynamicStates = (VkDynamicState[]){
              VK_DYNAMIC_STATE_VIEWPORT,
              VK_DYNAMIC_STATE_SCISSOR,
          },
      },
      .layout = context.standardPipelineLayout,
      .renderPass = context.renderPass,
  };
  VK_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, &context.standardPipeline));
  vkDestroyShaderModule(context.device, fragShader, VK_ALLOC);
  vkDestroyShaderModule(context.device, vertShader, VK_ALLOC);
}

//----------------------------------------------------------------------------------
// Memory
//----------------------------------------------------------------------------------

#define MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

static uint32_t FindMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties* pPhysicalDeviceMemoryProperties, const VkMemoryRequirements* pMemoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags) {
  for (uint32_t i = 0; i < pPhysicalDeviceMemoryProperties->memoryTypeCount; i++) {
    bool hasTypeBits = pMemoryRequirements->memoryTypeBits & 1 << i;
    bool hasPropertyFlags = (pPhysicalDeviceMemoryProperties->memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;
    if (hasTypeBits && hasPropertyFlags) {
      return i;
    }
  }
  int index = 0;
  while (memoryPropertyFlags) {
    if (memoryPropertyFlags & 1) {
      printf(" %s ", string_VkMemoryPropertyFlagBits(1U << index));
    }
    ++index;
    memoryPropertyFlags >>= 1;
  }
  PANIC("Failed to find memory with properties!");
}
static void AllocateMemory(const VkMemoryRequirements* pMemoryRequirements, const VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceMemory* pDeviceMemory) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memoryProperties);
  const uint32_t             memoryTypeIndex = FindMemoryTypeIndex(&memoryProperties, pMemoryRequirements, memoryPropertyFlags);
  const VkMemoryAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = pMemoryRequirements->size,
      .memoryTypeIndex = memoryTypeIndex,
  };
  VK_REQUIRE(vkAllocateMemory(context.device, &allocateInfo, VK_ALLOC, pDeviceMemory));
}
static void CreateAllocateBindBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = usage,
  };
  VK_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, NULL, pBuffer));
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(context.device, *pBuffer, &memoryRequirements);
  AllocateMemory(&memoryRequirements, memoryPropertyFlags, pDeviceMemory);
  VK_REQUIRE(vkBindBufferMemory(context.device, *pBuffer, *pDeviceMemory, 0));
}
static void CreateStagingBuffer(const void* srcData, const VkDeviceSize bufferSize, VkDeviceMemory* pStagingBufferMemory, VkBuffer* pStagingBuffer) {
  void* dstData;
  CreateAllocateBindBuffer(MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, pStagingBufferMemory, pStagingBuffer);
  VK_REQUIRE(vkMapMemory(context.device, *pStagingBufferMemory, 0, bufferSize, 0, &dstData));
  memcpy(dstData, srcData, bufferSize);
  vkUnmapMemory(context.device, *pStagingBufferMemory);
}
static void PopulateBufferViaStaging(const void* srcData, const VkDeviceSize bufferSize, const VkBuffer buffer) {
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(srcData, bufferSize, &stagingBufferMemory, &stagingBuffer);
  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.size = bufferSize});
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VK_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VK_ALLOC);
}

//----------------------------------------------------------------------------------
// Image Barriers
//----------------------------------------------------------------------------------

typedef enum QueueBarrier {
  QUEUE_BARRIER_IGNORE,
  QUEUE_BARRIER_GRAPHICS,
  QUEUE_BARRIER_COMPUTE,
  QUEUE_BARRIER_TRANSITION,
  QUEUE_BARRIER_FAMILY_EXTERNAL,
} QueueBarrier;
static uint32_t GetQueueIndex(const QueueBarrier queueBarrier) {
  switch (queueBarrier) {
    default:
    case QUEUE_BARRIER_IGNORE: return VK_QUEUE_FAMILY_IGNORED;
    case QUEUE_BARRIER_GRAPHICS: return context.graphicsQueueFamilyIndex;
    case QUEUE_BARRIER_COMPUTE: return context.computeQueueFamilyIndex;
    case QUEUE_BARRIER_TRANSITION: return context.transferQueueFamilyIndex;
    case QUEUE_BARRIER_FAMILY_EXTERNAL: return VK_QUEUE_FAMILY_EXTERNAL;
  }
}
typedef struct ImageBarrier {
  VkPipelineStageFlagBits2 stageMask;
  VkAccessFlagBits2        accessMask;
  VkImageLayout            layout;
  // QueueBarrier             queueFamily;
} ImageBarrier;
const ImageBarrier UndefinedImageBarrier = {
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
};
const ImageBarrier PresentImageBarrier = {
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
const ImageBarrier TransferDstImageBarrier = {
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
};
const ImageBarrier ShaderReadImageBarrier = {
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static void EmplaceImageBarrier(const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image, VkImageMemoryBarrier2* pImageMemoryBarrier) {
  *pImageMemoryBarrier = (const VkImageMemoryBarrier2){
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
      .subresourceRange = (const VkImageSubresourceRange){
          .aspectMask = aspectMask,
          .levelCount = 1,
          .layerCount = 1,
      },
  };
}
static void CommandImageBarrier(const VkCommandBuffer commandBuffer, const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image) {
  VkImageMemoryBarrier2 transferImageMemoryBarrier;
  EmplaceImageBarrier(pSrc, pDst, aspectMask, image, &transferImageMemoryBarrier);
  vkCmdPipelineBarrier2(commandBuffer, &(VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &transferImageMemoryBarrier});
}
static void TransitionImageLayoutImmediate(const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image) {
  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  CommandImageBarrier(commandBuffer, pSrc, pDst, aspectMask, image);
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
}

#define DEFAULT_IMAGE_CREATE_INFO                 \
  (VkImageCreateInfo) {                           \
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, \
    .imageType = VK_IMAGE_TYPE_2D,                \
    .format = VK_FORMAT_B8G8R8A8_SRGB,            \
    .mipLevels = 1,                               \
    .arrayLayers = 1,                             \
    .samples = VK_SAMPLE_COUNT_1_BIT,             \
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT           \
  }
static void CreateImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImage image, const VkImageAspectFlags aspectMask, VkImageView* pImageView) {
  const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = pImageCreateInfo->imageType,
      .format = pImageCreateInfo->format,
      .subresourceRange = {
          .aspectMask = aspectMask,
          .levelCount = pImageCreateInfo->mipLevels,
          .layerCount = pImageCreateInfo->arrayLayers,
      },
  };
  VK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VK_ALLOC, pImageView));
}
// static void CreateImageView(const VkImage image, VkImageView* pImageView) {
//   const VkImageViewCreateInfo imageViewCreateInfo = {
//       .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
//       .image = image,
//       .viewType = VK_IMAGE_VIEW_TYPE_2D,
//       .format = VK_FORMAT_B8G8R8A8_SRGB,
//       .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
//   };
//   VK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VK_ALLOC, pImageView));
// }
static void CreateAllocateBindImage(const VkImageCreateInfo* pImageCreateInfo, VkDeviceMemory* pMemory, VkImage* pImage) {
  // const VkImageCreateInfo imageCreateInfo = {
  //     .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
  //     .imageType = VK_IMAGE_TYPE_2D,
  //     .format = VK_FORMAT_B8G8R8A8_SRGB,
  //     .extent = extent,
  //     .mipLevels = 1,
  //     .arrayLayers = 1,
  //     .samples = VK_SAMPLE_COUNT_1_BIT,
  //     .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  // };
  VK_REQUIRE(vkCreateImage(context.device, pImageCreateInfo, VK_ALLOC, pImage));
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, *pImage, &memRequirements);
  AllocateMemory(&memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pMemory);
  VK_REQUIRE(vkBindImageMemory(context.device, *pImage, *pMemory, 0));
}
static void CreateAllocateBindImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImageAspectFlags aspectMask, VkDeviceMemory* pMemory, VkImage* pImage, VkImageView* pImageView) {
  CreateAllocateBindImage(pImageCreateInfo, pMemory, pImage);
  CreateImageView(pImageCreateInfo, *pImage, aspectMask, pImageView);
}

static void CreateImageFromFile(const char* pPath, VkDeviceMemory* pImageMemory, VkImage* pImage, VkImageView* pImageView) {
  int      texChannels, width, height;
  stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
  REQUIRE(width > 0 && height > 0, "Image height or width is equal to zero.")
  const VkDeviceSize imageBufferSize = width * height * 4;
  VkBuffer           stagingBuffer;
  VkDeviceMemory     stagingBufferMemory;
  CreateStagingBuffer(pImagePixels, imageBufferSize, &stagingBufferMemory, &stagingBuffer);
  stbi_image_free(pImagePixels);

  VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
  imageCreateInfo.extent = (VkExtent3D){width, height, 1.0f};
  imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, pImageMemory, pImage, pImageView);

  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  CommandImageBarrier(commandBuffer, &UndefinedImageBarrier, &TransferDstImageBarrier, VK_IMAGE_ASPECT_COLOR_BIT, *pImage);
  const VkBufferImageCopy region = {
      .imageSubresource =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .layerCount = 1,
          },
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, *pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  CommandImageBarrier(commandBuffer, &TransferDstImageBarrier, &ShaderReadImageBarrier, VK_IMAGE_ASPECT_COLOR_BIT, *pImage);
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------

typedef struct QueueDesc {
  Support      graphics;
  Support      compute;
  Support      transfer;
  Support      globalPriority;
  VkSurfaceKHR presentSurface;
} QueueDesc;
static uint32_t FindQueueIndex(const VkPhysicalDevice physicalDevice, const QueueDesc* pQueueDesc) {
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties2                 queueFamilyProperties[queueFamilyCount] = {};
  VkQueueFamilyGlobalPriorityPropertiesEXT queueFamilyGlobalPriorityProperties[queueFamilyCount] = {};
  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    queueFamilyGlobalPriorityProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT;
    queueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    queueFamilyProperties[i].pNext = &queueFamilyGlobalPriorityProperties[i];
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties);

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    bool graphicsSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    bool computeSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    bool transferSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
    bool globalPrioritySupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;

    VkBool32 presentSupport = VK_FALSE;
    if (pQueueDesc->presentSurface != NULL) vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, pQueueDesc->presentSurface, &presentSupport);

    if (pQueueDesc->graphics == SUPPORT_YES && !graphicsSupport) continue;
    if (pQueueDesc->graphics == SUPPORT_NO && graphicsSupport) continue;
    if (pQueueDesc->compute == SUPPORT_YES && !computeSupport) continue;
    if (pQueueDesc->compute == SUPPORT_NO && computeSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_YES && !transferSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_NO && transferSupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_YES && !globalPrioritySupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_NO && globalPrioritySupport) continue;
    if (pQueueDesc->presentSurface != NULL && !presentSupport) continue;

    return i;
  }

  fprintf(stderr, "graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
          string_Support[pQueueDesc->graphics],
          string_Support[pQueueDesc->compute],
          string_Support[pQueueDesc->transfer],
          string_Support[pQueueDesc->globalPriority],
          pQueueDesc->presentSurface == NULL ? "No" : "Yes");
  PANIC("Can't find queue family");
  return -1;
}

void mxcInitContext() {
  {  // Instance
    const char* ppEnabledLayerNames[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const char* ppEnabledInstanceExtensionNames[] = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        WindowExtensionName,
    };
    const VkInstanceCreateInfo instanceCreationInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &(const VkApplicationInfo){
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Moxaic",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Vulkan",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_VERSION,
        },
        .enabledLayerCount = COUNT(ppEnabledLayerNames),
        .ppEnabledLayerNames = ppEnabledLayerNames,
        .enabledExtensionCount = COUNT(ppEnabledInstanceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
    };
    VK_REQUIRE(vkCreateInstance(&instanceCreationInfo, VK_ALLOC, &context.instance));
    printf("Instance Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }

  {
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = 0;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT messageType = 0;
    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = DebugUtilsCallback,
    };
    PFN_LOAD(vkCreateDebugUtilsMessengerEXT);
    VK_REQUIRE(vkCreateDebugUtilsMessengerEXT(context.instance, &debugUtilsMessengerCreateInfo, VK_ALLOC, &context.debugUtilsMessenger));
  }

  {  // PhysicalDevice
    uint32_t deviceCount = 0;
    VK_REQUIRE(vkEnumeratePhysicalDevices(context.instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    VK_REQUIRE(vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices));
    context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    REQUIRE(physicalDeviceProperties.properties.apiVersion >= VK_VERSION, "Insufficinet Vulkan API Version");
  }

  {  // Surface
    VK_REQUIRE(mxcCreateSurface(context.instance, VK_ALLOC, &context.surface));
  }

  {  // QueueIndices
    const QueueDesc graphicsQueueDesc = {.graphics = SUPPORT_YES, .compute = SUPPORT_YES, .transfer = SUPPORT_YES, .globalPriority = SUPPORT_YES, .presentSurface = context.surface};
    context.graphicsQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &graphicsQueueDesc);
    const QueueDesc computeQueueDesc = {.graphics = SUPPORT_NO, .compute = SUPPORT_YES, .transfer = SUPPORT_YES, .globalPriority = SUPPORT_YES, .presentSurface = context.surface};
    context.computeQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &computeQueueDesc);
    const QueueDesc transferQueueDesc = {.graphics = SUPPORT_NO, .compute = SUPPORT_NO, .transfer = SUPPORT_YES};
    context.transferQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &transferQueueDesc);
  }

  {  // Device
    const VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
        .pNext = NULL,
        .globalPriorityQuery = VK_TRUE,
    };
    const VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = (void*)&physicalDeviceGlobalPriorityQueryFeatures,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
        .nullDescriptor = VK_TRUE,
    };
    const VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = (void*)&physicalDeviceRobustness2Features,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = (void*)&physicalDeviceMeshShaderFeatures,
        .synchronization2 = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = (void*)&physicalDeviceVulkan13Features,
        .samplerFilterMinmax = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = (void*)&physicalDeviceVulkan12Features,
    };
    const VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = (void*)&physicalDeviceVulkan11Features,
        .features = {
            .robustBufferAccess = VK_TRUE,
        },
    };
    const char* ppEnabledDeviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME,
        VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        ExternalMemoryExntensionName,
        ExternalSemaphoreExntensionName,
        ExternalFenceExntensionName,
    };
    const VkDeviceQueueGlobalPriorityCreateInfoEXT deviceQueueGlobalPriorityCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
        .globalPriority = isCompositor ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT,
    };
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = 3,
        .pQueueCreateInfos = (const VkDeviceQueueCreateInfo[]){
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.graphicsQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){isCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.computeQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){isCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = context.transferQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){0},
            },
        },
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    VK_REQUIRE(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VK_ALLOC, &context.device));
  }

  {  // Queues
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    REQUIRE((context.graphicsQueue != NULL), "graphicsQueue not found!");
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    REQUIRE((context.computeQueue != NULL), "computeQueue not found!");
    vkGetDeviceQueue(context.device, context.transferQueueFamilyIndex, 0, &context.transferQueue);
    REQUIRE((context.transferQueue != NULL), "transferQueue not found!");
  }

  // PFN_LOAD(vkSetDebugUtilsObjectNameEXT);

  {  // RenderPass
#define DEFAULT_ATTACHMENT_DESCRIPTION                \
  .samples = VK_SAMPLE_COUNT_1_BIT,                   \
  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              \
  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,            \
  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   \
  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, \
  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         \
  .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    const VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments = (const VkAttachmentDescription[]){
            {.format = RenderPassFormats[RENDERPASS_COLOR_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = RenderPassFormats[RENDERPASS_NORMAL_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = RenderPassFormats[RENDERPASS_DEPTH_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
        },
        .subpassCount = 1,
        .pSubpasses = &(const VkSubpassDescription){
            .colorAttachmentCount = 2,
            .pColorAttachments = (const VkAttachmentReference[]){
                {
                    .attachment = RENDERPASS_COLOR_ATTACHMENT,
                    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                },
                {
                    .attachment = RENDERPASS_NORMAL_ATTACHMENT,
                    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                },
            },
            .pDepthStencilAttachment = &(const VkAttachmentReference){
                .attachment = RENDERPASS_DEPTH_ATTACHMENT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            },
        },
    };
    VK_REQUIRE(vkCreateRenderPass(context.device, &renderPassCreateInfo, VK_ALLOC, &context.renderPass));
#undef DEFAULT_ATTACHMENT_DESCRIPTION
  }

  {  // Pools
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex,
    };
    VK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &context.graphicsCommandPool));
    const VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.computeQueueFamilyIndex,
    };
    VK_REQUIRE(vkCreateCommandPool(context.device, &computeCommandPoolCreateInfo, VK_ALLOC, &context.computeCommandPool));
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 30,
        .poolSizeCount = 3,
        .pPoolSizes = (const VkDescriptorPoolSize[]){
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
        },
    };
    VK_REQUIRE(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, VK_ALLOC, &context.descriptorPool));
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    VK_REQUIRE(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VK_ALLOC, &context.timeQueryPool));
  }

  {  // CommandBuffers
    const VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.graphicsCommandPool,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(context.device, &graphicsCommandBufferAllocateInfo, &context.graphicsCommandBuffer));
    const VkCommandBufferAllocateInfo computeCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.computeCommandPool,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(context.device, &computeCommandBufferAllocateInfo, &context.computeCommandBuffer));
  }

  {
#define DEFAULT_LINEAR_SAMPLER .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR, .maxLod = 16.0, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK

    const VkSamplerCreateInfo nearestSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .maxLod = 16.0,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &nearestSampleCreateInfo, VK_ALLOC, &context.nearestSampler));
    const VkSamplerCreateInfo linearSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &linearSampleCreateInfo, VK_ALLOC, &context.linearSampler));
    const VkSamplerCreateInfo minSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(const VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &minSamplerCreateInfo, VK_ALLOC, &context.minSampler));
    const VkSamplerCreateInfo maxSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(const VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &maxSamplerCreateInfo, VK_ALLOC, &context.minSampler));

#undef DEFAULT_LINEAR_SAMPLER
  }

  {  // Swapchain
    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.surface,
        .minImageCount = VK_SWAP_COUNT,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageExtent = {DEFAULT_WIDTH, DEFAULT_HEIGHT},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VK_REQUIRE(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, VK_ALLOC, &context.swapchain));
    uint32_t swapCount;
    VK_REQUIRE(vkGetSwapchainImagesKHR(context.device, context.swapchain, &swapCount, NULL));
    REQUIRE(swapCount == VK_SWAP_COUNT, "Resulting swap image count does not match requested swap count!");
    VK_REQUIRE(vkGetSwapchainImagesKHR(context.device, context.swapchain, &swapCount, context.swapImages));

    for (int i = 0; i < VK_SWAP_COUNT; ++i) {
      const VkImageViewCreateInfo imageViewCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = context.swapImages[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_B8G8R8A8_SRGB,
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
      };
      VK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VK_ALLOC, &context.swapImageViews[i]));
      TransitionImageLayoutImmediate(&UndefinedImageBarrier, &PresentImageBarrier, VK_IMAGE_ASPECT_COLOR_BIT, context.swapImages[i]);
    }
  }

  {  // Global Descriptor
    CreateGlobalSetLayout();
    context.globalSet = AllocateDescriptorSet(&context.globalSetLayout);
    CreateAllocateBindBuffer(MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(GlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &context.globalSetMemory, &context.globalSetBuffer);
    VK_REQUIRE(vkMapMemory(context.device, context.globalSetMemory, 0, sizeof(GlobalSetState), 0, (void**)&context.globalSetMapped));
  }

  {  // Standard Pipeline
    CreateStandardMaterialSetLayout();
    CreateStandardObjectSetLayout();
    CreateStandardPipelineLayout();
    CreateStandardPipeline();
  }
}

// should I do these?
typedef struct Texture {
  VkImage        image;
  VkImageView    imageView;
  VkDeviceMemory imageMemory;
  VkFormat       format;
  VkExtent3D     extent;
} Texture;
typedef struct Mesh {
  VkDeviceMemory indexMemory;
  VkBuffer       indexBuffer;
  VkDeviceMemory vertexMemory;
  VkBuffer       vertexBuffer;
} Mesh;

static int  GenerateSphereVertexCount(const int slicesCount, const int stacksCount) { return (slicesCount + 1) * (stacksCount + 1); }
static int  GenerateSphereIndexCount(const int slicesCount, const int stacksCount) { return slicesCount * stacksCount * 2 * 3; }
static void GenerateSphere(const int slicesCount, const int stacksCount, const float radius, Vertex* pVertex) {
  const float slices = (float)slicesCount;
  const float stacks = (float)stacksCount;
  const float dtheta = 2.0f * PI / slices;
  const float dphi = PI / stacks;
  int         idx = 0;
  for (int i = 0; +i <= stacksCount; i++) {
    const float fi = (float)i;
    const float phi = fi * dphi;
    for (int j = 0; j <= slicesCount; j++) {
      const float ji = (float)j;
      const float theta = ji * dtheta;
      const float x = radius * sinf(phi) * cosf(theta);
      const float y = radius * sinf(phi) * sinf(theta);
      const float z = radius * cosf(phi);
      pVertex[idx++] = (Vertex){
          .position = {x, y, z},
          .normal = {x, y, z},
          .uv = {ji / slices, fi / stacks},
      };
    }
  }
}
static void GenerateSphereIndices(const int nslices, const int nstacks, uint16_t* pIndices) {
  int idx = 0;
  for (int i = 0; i < nstacks; i++) {
    for (int j = 0; j < nslices; j++) {
      const uint16_t v1 = i * (nslices + 1) + j;
      const uint16_t v2 = i * (nslices + 1) + j + 1;
      const uint16_t v3 = (i + 1) * (nslices + 1) + j;
      const uint16_t v4 = (i + 1) * (nslices + 1) + j + 1;
      pIndices[idx++] = v1;
      pIndices[idx++] = v2;
      pIndices[idx++] = v3;
      pIndices[idx++] = v2;
      pIndices[idx++] = v4;
      pIndices[idx++] = v3;
    }
  }
}
static void CreateSphereMeshBuffers(const float radius, const int slicesCount, const int stackCount, VkDeviceMemory* pIndexMemory, VkBuffer* pIndexBuffer, VkDeviceMemory* pVertexMemory, VkBuffer* pVertexBuffer) {
  {
    const int indexCount = GenerateSphereIndexCount(slicesCount, stackCount);
    uint16_t  pIndices[indexCount];
    GenerateSphereIndices(slicesCount, stackCount, pIndices);
    uint32_t indexBufferSize = sizeof(uint16_t) * indexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, pIndexMemory, pIndexBuffer);
    PopulateBufferViaStaging(pIndices, indexBufferSize, *pIndexBuffer);
  }
  {
    const int vertexCount = GenerateSphereVertexCount(slicesCount, stackCount);
    Vertex    pVertices[vertexCount];
    GenerateSphere(slicesCount, stackCount, radius, pVertices);
    uint32_t vertexBufferSize = sizeof(Vertex) * vertexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, pVertexMemory, pVertexBuffer);
    PopulateBufferViaStaging(pVertices, vertexBufferSize, *pVertexBuffer);
  }
}

int mxcRenderNode() {

  Framebuffer framebuffers[VK_SWAP_COUNT];
  for (int i = 0; i < VK_SWAP_COUNT; ++i) {
    VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
    imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    imageCreateInfo.format = RenderPassFormats[RENDERPASS_COLOR_ATTACHMENT];
    imageCreateInfo.usage = RenderPassUsages[RENDERPASS_COLOR_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &framebuffers[i].colorImageMemory, &framebuffers[i].colorImage, &framebuffers[i].colorImageView);
    imageCreateInfo.format = RenderPassFormats[RENDERPASS_NORMAL_ATTACHMENT];
    imageCreateInfo.usage = RenderPassUsages[RENDERPASS_NORMAL_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &framebuffers[i].normalImageMemory, &framebuffers[i].normalImage, &framebuffers[i].normalImageView);
    imageCreateInfo.format = RenderPassFormats[RENDERPASS_DEPTH_ATTACHMENT];
    imageCreateInfo.usage = RenderPassUsages[RENDERPASS_DEPTH_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_DEPTH_BIT, &framebuffers[i].depthImageMemory, &framebuffers[i].depthImage, &framebuffers[i].depthImageView);

    imageCreateInfo.format = GBufferFormat;
    imageCreateInfo.usage = GBufferUsage;
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &framebuffers[i].gBufferImageMemory, &framebuffers[i].gBufferImage, &framebuffers[i].gBufferImageView);

    VkImageView attachments[RENDERPASS_ATTACHMENT_COUNT];
    attachments[RENDERPASS_COLOR_ATTACHMENT] = framebuffers[i].colorImageView;
    attachments[RENDERPASS_NORMAL_ATTACHMENT] = framebuffers[i].normalImageView;
    attachments[RENDERPASS_DEPTH_ATTACHMENT] = framebuffers[i].depthImageView;
    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = context.renderPass,
        .attachmentCount = RENDERPASS_ATTACHMENT_COUNT,
        .pAttachments = attachments,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .layers = 1,
    };
    VK_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VK_ALLOC, &framebuffers[i].framebuffer));

    const VkSemaphoreCreateInfo renderCompleteCreateInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_REQUIRE(vkCreateSemaphore(context.device, &renderCompleteCreateInfo, VK_ALLOC, &framebuffers[i].renderCompleteSemaphore));
  }

  VkDeviceMemory checkerImageMemory;
  VkImage        checkerImage;
  VkImageView    checkerImageView;
  CreateImageFromFile("textures/uvgrid.jpg", &checkerImageMemory, &checkerImage, &checkerImageView);

  VkDeviceMemory sphereIndexMemory, sphereVertexBufferMemory;
  VkBuffer       sphereIndexBuffer, sphereVertexBuffer;
  CreateSphereMeshBuffers(0.5f, 32, 32, &sphereIndexMemory, &sphereIndexBuffer, &sphereVertexBufferMemory, &sphereVertexBuffer);

  StandardObjectSetState sphereObjectSetMapped;
  VkDeviceMemory         sphereObjectSetMemory;
  VkBuffer               sphereObjectSetBuffer;
  CreateAllocateBindBuffer(MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(StandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &sphereObjectSetMemory, &sphereObjectSetBuffer);
  VK_REQUIRE(vkMapMemory(context.device, sphereObjectSetMemory, 0, sizeof(StandardObjectSetState), 0, (void**)&sphereObjectSetMapped));

  const VkDescriptorSet      checkerMaterialSet = AllocateDescriptorSet(&context.materialSetLayout);
  const VkDescriptorSet      sphereObjectSet = AllocateDescriptorSet(&context.objectSetLayout);
  const VkWriteDescriptorSet writeSets[] = {
      SET_WRITE_GLOBAL,
      SET_WRITE_STANDARD_MATERIAL(checkerMaterialSet, checkerImageView),
      SET_WRITE_STANDARD_OBJECT(sphereObjectSet, sphereObjectSetBuffer),
  };
  vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);

  Camera camera = {
      .position = {0.0f, 0.0f, -1.0f},
      .rotation = quatIdentity,
  };
  GlobalSetState globalSetState = {.screenSize = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT}};
  Mat4Perspective(45.0f, DEFAULT_WIDTH / DEFAULT_HEIGHT, 0.1f, 100.0f, &globalSetState.projection);
  Mat4Inv(&globalSetState.projection, &globalSetState.inverseProjection);
  TransformToModelMat4((Transform*)&camera.position, &globalSetState.inverseView);
  Mat4Inv(&globalSetState.inverseView, &globalSetState.view);
  Mat4Mul(&globalSetState.projection, &globalSetState.view, &globalSetState.viewProjection);
  Mat4Mul(&globalSetState.inverseView, &globalSetState.inverseViewProjection, &globalSetState.inverseViewProjection);
  memcpy(&context.globalSetMapped, &globalSetState, sizeof(GlobalSetState));

  Transform sphereTransform = {
      .position = {0.0f, 0.0f, 0.0f},
      .rotation = quatIdentity,
  };
  StandardObjectSetState sphereObjectState = {};
  TransformToModelMat4(&sphereTransform, &sphereObjectState.model);
  memcpy(&sphereObjectSetMapped, &sphereObjectState, sizeof(StandardObjectSetState));

  int framebufferIndex = 0;

  while (isRunning) {
    mxUpdateWindow();

    vkResetCommandBuffer(context.graphicsCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    const VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(context.graphicsCommandBuffer, &beginInfo);
    const VkViewport viewport = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f};
    vkCmdSetViewport(context.graphicsCommandBuffer, 0, 1, &viewport);
    const VkRect2D scissor = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}};
    vkCmdSetScissor(context.graphicsCommandBuffer, 0, 1, &scissor);

    VkClearValue clearValues[RENDERPASS_ATTACHMENT_COUNT];
    clearValues[RENDERPASS_COLOR_ATTACHMENT].color = (VkClearColorValue){{0.1f, 0.2f, 0.3f, 0.0f}};
    clearValues[RENDERPASS_NORMAL_ATTACHMENT].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}},
    clearValues[RENDERPASS_DEPTH_ATTACHMENT].depthStencil = (VkClearDepthStencilValue){0.0f, 0};
    const VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context.renderPass,
        .framebuffer = framebuffers[framebufferIndex].framebuffer,
        .renderArea = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
        .clearValueCount = COUNT(clearValues),
        .pClearValues = clearValues,
    };
    vkCmdBeginRenderPass(context.graphicsCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }
}