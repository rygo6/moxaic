#pragma once

#include "mid_math.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#define MIDVK_PLATFORM_SURFACE_EXTENSION_NAME   VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define MIDVK_EXTERNAL_MEMORY_EXTENSION_NAME    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
#define MIDVK_EXTERNAL_SEMAPHORE_EXTENSION_NAME VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#define MIDVK_EXTERNAL_FENCE_EXTENSION_NAME     VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME
#define MIDVK_EXTERNAL_MEMORY_HANDLE_TYPE       VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
#define MIDVK_EXTERNAL_SEMAPHORE_HANDLE_TYPE    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

//
/// Debug
#ifndef MID_DEBUG
#define MID_DEBUG
extern void Panic(const char* file, int line, const char* message);
#define PANIC(message) Panic(__FILE__, __LINE__, message)
#define REQUIRE(condition, message)        \
  if (__builtin_expect(!(condition), 0)) { \
    PANIC(message);                        \
  }
#ifdef MID_VULKAN_IMPLEMENTATION
[[noreturn]] void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  __builtin_trap();
}
#endif
#endif

//
/// Globals
//#define VKM_DEBUG_MEMORY_ALLOC

#ifndef COUNT
#define COUNT(_array) (sizeof(_array) / sizeof(_array[0]))
#endif

// these values shouldnt be macros
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH  1024
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 1024
#endif

#define MIDVK_ALLOC      NULL
#define MIDVK_VERSION    VK_MAKE_API_VERSION(0, 1, 3, 2)
#define MIDVK_SWAP_COUNT 2
#define MIDVK_REQUIRE(command)                              \
  {                                                         \
    VkResult result = command;                              \
    REQUIRE(result == VK_SUCCESS, string_VkResult(result)); \
  }

#define MIDVK_INSTANCE_FUNC(_func)                                                             \
  const PFN_##vk##_func _func = (PFN_##vk##_func)vkGetInstanceProcAddr(instance, "vk" #_func); \
  REQUIRE(_func != NULL, "Couldn't load " #_func)
#define MIDVK_INSTANCE_STATIC_FUNC(_func)                                  \
  static PFN_##vk##_func _func = NULL;                                     \
  if (_func == NULL) {                                                     \
    _func = (PFN_##vk##_func)vkGetInstanceProcAddr(instance, "vk" #_func); \
    REQUIRE(_func != NULL, "Couldn't load " #_func)                        \
  }
#define MIDVK_DEVICE_FUNC(_func)                                                           \
  const PFN_##vk##_func _func = (PFN_##vk##_func)vkGetDeviceProcAddr(device, "vk" #_func); \
  REQUIRE(_func != NULL, "Couldn't load " #_func)

#define VKM_BUFFER_USAGE_MESH                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
#define VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define VKM_MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

#define MIDVK_EXTERNAL_IMAGE_CREATE_INFO                          \
  &(const VkExternalMemoryImageCreateInfo) {                      \
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, \
    .handleTypes = MIDVK_EXTERNAL_MEMORY_HANDLE_TYPE,             \
  }

//----------------------------------------------------------------------------------
// Mid Types
//----------------------------------------------------------------------------------

typedef enum MidLocality {
  // Used within the context it was made
  MID_LOCALITY_CONTEXT,
  // Used by multiple contexts, but in the same process
  MID_LOCALITY_PROCESS,
  // Used by nodes external to this context, device and process.
  MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
  MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
  // Used by nodes external to this context, device and process.
  MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
  MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
  MID_LOCALITY_COUNT,
} MidLocality;
#define MID_LOCALITY_INTERPROCESS(_locality) (_locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || \
                                              _locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY ||  \
                                              _locality == MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE || \
                                              _locality == MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY)
#define MID_LOCALITY_INTERPROCESS_EXPORTED(_locality) (_locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || \
                                                       _locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY)
#define MID_LOCALITY_INTERPROCESS_IMPORTED(_locality) (_locality == MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE || \
                                                       _locality == MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY)

// these should go in math. Can I make MidVk not depend on specific math lib?
typedef struct MidTransform {
  vec3 position;
  vec3 euler;
  vec4 rotation;
} MidTransform;
typedef struct MidVertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
} MidVertex;

//----------------------------------------------------------------------------------
// Vulkan Types
//----------------------------------------------------------------------------------

typedef struct MidVkSwap {
  VkSwapchainKHR chain;
  VkSemaphore    acquireSemaphore;
  VkSemaphore    renderCompleteSemaphore;
  VkImage        images[MIDVK_SWAP_COUNT];
} MidVkSwap;
typedef uint8_t MidVkMemoryType;
typedef struct MidVkSharedMemory {
  VkDeviceSize  offset;
  VkDeviceSize  size;
  MidVkMemoryType type;
} MidVkSharedMemory;
typedef struct MidVkTexture {
  VkImage         image;
  VkImageView     view;

  // store more info? probably...

  // need to implement texture shared memory
  // should there be different structs for external, shared, or dedicate?
  // this is the 'cold' storage so maybe it doesn't matter
//  MidVkSharedMemory sharedMemory;
  VkDeviceMemory  memory;
} MidVkTexture;
typedef struct VkmMeshCreateInfo {
  uint32_t         indexCount;
  uint32_t         vertexCount;
  const uint16_t*  pIndices;
  const MidVertex* pVertices;
} VkmMeshCreateInfo;
typedef struct VkmMesh {
  // get rid of this? I don't think I have a use for non-shared memory meshes
  // maybe in an IPC shared mesh? But I cant think of a use for that
  VkDeviceMemory  memory;
  VkBuffer        buffer;

  MidVkSharedMemory sharedMemory;

  uint32_t        indexCount;
  uint32_t        vertexCount;
  VkDeviceSize    indexOffset;
  VkDeviceSize    vertexOffset;
} VkmMesh;

typedef struct MidVkFramebufferTexture {
  MidVkTexture    color;
  MidVkTexture    normal;
  MidVkTexture    depth;
} MidVkFramebufferTexture;

typedef struct VkmGlobalSetState {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 invView;
  mat4 invProj;
  mat4 invViewProj;
  ivec2 framebufferSize;
} VkmGlobalSetState;

typedef struct VkmStandardObjectSetState {
  mat4 model;
} VkmStdObjectSetState;

typedef enum VkmQueueFamilyType {
  VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS,
  VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE,
  VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER,
  VKM_QUEUE_FAMILY_TYPE_COUNT
} VkmQueueFamilyType;
static const char* string_QueueFamilyType[] = {
    [VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS] = "VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS",
    [VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = "VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE",
    [VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER] = "VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER",
};
typedef struct VkmQueueFamily {
  VkQueue       queue;
  VkCommandPool pool;
  uint32_t      index;
} VkmQueueFamily;
typedef struct VkmStdPipeLayout {
  VkPipelineLayout      pipeLayout;
  VkDescriptorSetLayout globalSetLayout;
  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSetLayout objectSetLayout;
} VkmStdPipeLayout;

typedef struct VkmGlobalSet {
  VkmGlobalSetState* pMapped;
  VkDeviceMemory     memory;
  MidVkSharedMemory  sharedMemory;
  VkBuffer           buffer;
  VkDescriptorSet    set;
} VkmGlobalSet;

typedef struct __attribute((aligned(64))) MidVkContext {
  VkPhysicalDevice physicalDevice;
  VkDevice         device;
  VkmQueueFamily   queueFamilies[VKM_QUEUE_FAMILY_TYPE_COUNT];
//  VkDescriptorPool descriptorPool;
  VkmStdPipeLayout stdPipeLayout;

  VkSampler    linearSampler;
  VkRenderPass renderPass;

  // this should go elsewhere
  VkRenderPass nodeRenderPass;
  // basic pipe could stay here
  VkPipeline basicPipe;

} MidVkContext;

typedef struct __attribute((aligned(64))) MidVkThreadContext {
    VkDescriptorPool descriptorPool;
} MidVkThreadContext;

extern VkInstance instance;
extern MidVkContext context;
extern __thread MidVkThreadContext threadContext;

extern __thread VkDeviceMemory deviceMemory[VK_MAX_MEMORY_TYPES];
extern __thread void*          pMappedMemory[VK_MAX_MEMORY_TYPES];

//----------------------------------------------------------------------------------
// Render
//----------------------------------------------------------------------------------

typedef enum MidVkPassAttachmentStdIndices {
  MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX,
  MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX,
  MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX,
  MIDVK_PASS_ATTACHMENT_STD_COUNT,
} MidVkPassAttachmentStdIndices;
typedef enum MidVkPipeSetStdIndices {
  MIDVK_PIPE_SET_STD_GLOBAL_INDEX,
  MIDVK_PIPE_SET_STD_MATERIAL_INDEX,
  MIDVK_PIPE_SET_STD_OBJECT_INDEX,
  MIDVK_PIPE_SET_STD_INDEX_COUNT,
} MidVkPipeSetStdIndices;
static const VkFormat MIDVK_PASS_FORMATS[] = {
    [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_FORMAT_R8G8B8A8_UNORM,
    [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_FORMAT_D32_SFLOAT,
};
static const VkImageUsageFlags MIDVK_PASS_USAGES[] = {
    [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
};
#define MIDVK_PASS_CLEAR_COLOR (VkClearColorValue) { 0.1f, 0.2f, 0.3f, 0.0f }

#define VKM_SET_BIND_STD_GLOBAL_BUFFER 0
#define VKM_SET_WRITE_STD_GLOBAL_BUFFER(global_set, global_set_buffer) \
  (VkWriteDescriptorSet) {                                             \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                   \
    .dstSet = global_set,                                              \
    .dstBinding = VKM_SET_BIND_STD_GLOBAL_BUFFER,                      \
    .descriptorCount = 1,                                              \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,               \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                    \
        .buffer = global_set_buffer,                                   \
        .range = sizeof(VkmGlobalSetState),                            \
    },                                                                 \
  }
#define VKM_SET_BIND_STD_MATERIAL_TEXTURE 0
#define VKM_SET_WRITE_STD_MATERIAL_IMAGE(materialSet, material_image_view) \
  (VkWriteDescriptorSet) {                                                 \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                       \
    .dstSet = materialSet,                                                 \
    .dstBinding = VKM_SET_BIND_STD_MATERIAL_TEXTURE,                       \
    .descriptorCount = 1,                                                  \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,           \
    .pImageInfo = &(const VkDescriptorImageInfo){                          \
        .imageView = material_image_view,                                  \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,           \
    },                                                                     \
  }
#define VKM_SET_BIND_STD_OBJECT_BUFFER 0
#define VKM_SET_WRITE_STD_OBJECT_BUFFER(objectSet, objectSetBuffer) \
  (VkWriteDescriptorSet) {                                          \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                \
    .dstSet = objectSet,                                            \
    .dstBinding = VKM_SET_BIND_STD_OBJECT_BUFFER,                   \
    .descriptorCount = 1,                                           \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            \
    .pBufferInfo = &(const VkDescriptorBufferInfo){                 \
        .buffer = objectSetBuffer,                                  \
        .range = sizeof(VkmStdObjectSetState),                      \
    },                                                              \
  }

//----------------------------------------------------------------------------------
// Image Barriers
//----------------------------------------------------------------------------------

typedef enum VkmQueueBarrier {
  QUEUE_BARRIER_MAIN_GRAPHICS,
  QUEUE_BARRIER_DEDICATED_COMPUTE,
  QUEUE_BARRIER_DEDICATED_TRANSITION,
  QUEUE_BARRIER_IGNORE,
  QUEUE_BARRIER_FAMILY_EXTERNAL,
} VkmQueueBarrier;
//static uint32_t GetQueueIndex(const QueueBarrier queueBarrier) {
//  switch (queueBarrier) {
//    default:
//    case QUEUE_BARRIER_IGNORE:          return VK_QUEUE_FAMILY_IGNORED;
//    case QUEUE_BARRIER_MAIN_GRAPHICS:        return context.graphicsQueueFamilyIndex;
//    case QUEUE_BARRIER_DEDICATED_COMPUTE:         return context.computeQueueFamilyIndex;
//    case QUEUE_BARRIER_DEDICATED_TRANSITION:      return context.transferQueueFamilyIndex;
//    case QUEUE_BARRIER_FAMILY_EXTERNAL: return VK_QUEUE_FAMILY_EXTERNAL;
//  }
//}
typedef struct VkmImageBarrier {
  VkPipelineStageFlagBits2 stageMask;
  VkAccessFlagBits2        accessMask;
  VkImageLayout            layout;
  // QueueBarrier             queueFamily;
} VkmImageBarrier;
// I wonder if its bad to have these stored static? stack wont let them go
static const VkmImageBarrier* VKM_IMAGE_BARRIER_UNDEFINED = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
};
static const VkmImageBarrier* VKM_IMAGE_BARRIER_COLOR_ATTACHMENT_UNDEFINED = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_EXTERNAL_RELEASE_SHADER_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_PRESENT_ACQUIRE = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_PRESENT_BLIT_RELEASE = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_PRESENT = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_COMPUTE_SHADER_READ_ONLY = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMAGE_BARRIER_COMPUTE_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_GENERAL,
};
static const VkmImageBarrier* VKM_IMAGE_BARRIER_COMPUTE_WRITE = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_GENERAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_COMPUTE_READ_WRITE = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_GENERAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_TRANSFER_SRC = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_TRANSFER_DST = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_BLIT_DST = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
    .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMAGE_BARRIER_TRANSFER_DST_GENERAL = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
    .layout = VK_IMAGE_LAYOUT_GENERAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_TRANSFER_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .accessMask = VK_ACCESS_2_MEMORY_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_SHADER_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_COLOR_ATTACHMENT_WRITE = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_COLOR_ATTACHMENT_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
};
static const VkmImageBarrier* VKM_IMG_BARRIER_DEPTH_ATTACHMENT = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
};
#define VKM_COLOR_IMAGE_BARRIER(src, dst, barrier_image)   \
  (const VkImageMemoryBarrier2) {                        \
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,   \
    .srcStageMask = src->stageMask,                      \
    .srcAccessMask = src->accessMask,                    \
    .dstStageMask = dst->stageMask,                      \
    .dstAccessMask = dst->accessMask,                    \
    .oldLayout = src->layout,                            \
    .newLayout = dst->layout,                            \
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,      \
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,      \
    .image = barrier_image,                              \
    .subresourceRange = (const VkImageSubresourceRange){ \
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,         \
        .levelCount = 1,                                 \
        .layerCount = 1,                                 \
    },                                                   \
  }
#define VKM_COLOR_IMAGE_BARRIER_MIPS(src, dst, barrier_image, base_mip_level, level_count) \
  (const VkImageMemoryBarrier2) {                                                          \
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,                                     \
    .srcStageMask = src->stageMask,                                                        \
    .srcAccessMask = src->accessMask,                                                      \
    .dstStageMask = dst->stageMask,                                                        \
    .dstAccessMask = dst->accessMask,                                                      \
    .oldLayout = src->layout,                                                              \
    .newLayout = dst->layout,                                                              \
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,                                        \
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,                                        \
    .image = barrier_image,                                                                \
    .subresourceRange = (const VkImageSubresourceRange){                                   \
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,                                           \
        .baseMipLevel = base_mip_level,                                                    \
        .levelCount = level_count,                                                         \
        .layerCount = 1,                                                                   \
    },                                                                                     \
  }
#define VKM_IMG_BARRIER(src, dst, aspect_mask, barrier_image) \
  (const VkImageMemoryBarrier2) {                             \
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,        \
    .srcStageMask = src->stageMask,                           \
    .srcAccessMask = src->accessMask,                         \
    .dstStageMask = dst->stageMask,                           \
    .dstAccessMask = dst->accessMask,                         \
    .oldLayout = src->layout,                                 \
    .newLayout = dst->layout,                                 \
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           \
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,           \
    .image = barrier_image,                                   \
    .subresourceRange = (const VkImageSubresourceRange){      \
        .aspectMask = aspect_mask,                            \
        .levelCount = 1,                                      \
        .layerCount = 1,                                      \
    },                                                        \
  }
#define VKM_IMG_BARRIER_TRANSFER(src, dst, aspect_mask, barrier_image, src_queue, dst_queue) \
  (const VkImageMemoryBarrier2) {                                                            \
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,                                       \
    .srcStageMask = src->stageMask,                                                          \
    .srcAccessMask = src->accessMask,                                                        \
    .dstStageMask = dst->stageMask,                                                          \
    .dstAccessMask = dst->accessMask,                                                        \
    .oldLayout = src->layout,                                                                \
    .newLayout = dst->layout,                                                                \
    .srcQueueFamilyIndex = src_queue,                                                        \
    .dstQueueFamilyIndex = dst_queue,                                                        \
    .image = barrier_image,                                                                  \
    .subresourceRange = (VkImageSubresourceRange){                                           \
        .aspectMask = aspect_mask,                                                           \
        .levelCount = 1,                                                                     \
        .layerCount = 1,                                                                     \
    },                                                                                       \
  }

//----------------------------------------------------------------------------------
// Inline Methods
//----------------------------------------------------------------------------------
#define VKM_INLINE __attribute__((always_inline)) static inline

#define CmdPipelineImageBarriers2(cmd, imageMemoryBarrierCount, pImageMemoryBarriers) PFN_CmdPipelineImageBarriers(CmdPipelineBarrier2, cmd, imageMemoryBarrierCount, pImageMemoryBarriers)
VKM_INLINE void PFN_CmdPipelineImageBarriers(const PFN_vkCmdPipelineBarrier2 func, const VkCommandBuffer cmd, const uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers) {
  func(cmd, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}
#define CmdPipelineImageBarrier2(cmd, pImageMemoryBarrier) PFN_CmdPipelineImageBarrierFunc(CmdPipelineBarrier2, cmd, pImageMemoryBarrier)
VKM_INLINE void PFN_CmdPipelineImageBarrierFunc(const PFN_vkCmdPipelineBarrier2 func, const VkCommandBuffer cmd, const VkImageMemoryBarrier2* pImageMemoryBarrier) {
  func(cmd, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = pImageMemoryBarrier});
}
#define CmdBlitImageFullScreen(cmd, srcImage, dstImage) PFN_CmdBlitImageFullScreen(CmdBlitImage, cmd, srcImage, dstImage)
VKM_INLINE void PFN_CmdBlitImageFullScreen(const PFN_vkCmdBlitImage func, const VkCommandBuffer cmd, const VkImage srcImage, const VkImage dstImage) {
  const VkImageBlit imageBlit = {
      .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1},
      .srcOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_WIDTH, .z = 1}},
      .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1},
      .dstOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_WIDTH, .z = 1}},
  };
  func(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
}
#define CmdBeginRenderPass(_cmd, _render_pass, _framebuffer, _clear_color, _color_view, _normal_view, _depth_view) \
  PFN_CmdBeginRenderPass(CmdBeginRenderPass, _cmd, _render_pass, _framebuffer, _clear_color, _color_view, _normal_view, _depth_view)
VKM_INLINE void PFN_CmdBeginRenderPass(
    const PFN_vkCmdBeginRenderPass func,
    const VkCommandBuffer          cmd,
    const VkRenderPass             renderPass,
    const VkFramebuffer            framebuffer,
    const VkClearColorValue        clearColor,
    const VkImageView              colorView,
    const VkImageView              normalView,
    const VkImageView              depthView) {
  const VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = &(VkRenderPassAttachmentBeginInfo){
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
          .attachmentCount = MIDVK_PASS_ATTACHMENT_STD_COUNT,
          .pAttachments = (const VkImageView[]){
              [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = colorView,
              [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = normalView,
              [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = depthView,
          },
      },
      .renderPass = renderPass,
      .framebuffer = framebuffer,
      .renderArea = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
      .clearValueCount = MIDVK_PASS_ATTACHMENT_STD_COUNT,
      .pClearValues = (const VkClearValue[]){
          [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = {.color = clearColor},
          [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
          [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = {.depthStencil = {0.0f}},
      },
  };
  func(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}
VKM_INLINE void vkmCmdResetBegin(const VkCommandBuffer commandBuffer) {
  vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkBeginCommandBuffer(commandBuffer, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});
  vkCmdSetViewport(commandBuffer, 0, 1, &(const VkViewport){.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f});
  vkCmdSetScissor(commandBuffer, 0, 1, &(const VkRect2D){.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}});
}
VKM_INLINE void vkmSubmitPresentCommandBuffer(
    const VkCommandBuffer cmd,
    const VkSwapchainKHR  chain,
    const VkSemaphore     acquireSemaphore,
    const VkSemaphore     renderCompleteSemaphore,
    const uint32_t        swapIndex,
    const VkSemaphore     timeline,
    const uint64_t        timelineSignalValue) {
  const VkSubmitInfo2 submitInfo2 = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount = 1,
      .pWaitSemaphoreInfos = (VkSemaphoreSubmitInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
              .semaphore = acquireSemaphore,
              .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          },
      },
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = (VkCommandBufferSubmitInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
              .commandBuffer = cmd,
          },
      },
      .signalSemaphoreInfoCount = 2,
      .pSignalSemaphoreInfos = (VkSemaphoreSubmitInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
              .value = timelineSignalValue,
              .semaphore = timeline,
              .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          },
          {
              .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
              .semaphore = renderCompleteSemaphore,
              .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
          },
      },
  };
  MIDVK_REQUIRE(vkQueueSubmit2(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue, 1, &submitInfo2, VK_NULL_HANDLE));
  const VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &renderCompleteSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &chain,
      .pImageIndices = &swapIndex,
  };
  MIDVK_REQUIRE(vkQueuePresentKHR(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue, &presentInfo));
  //  vkQueueWaitIdle(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue);
}
VKM_INLINE void vkmSubmitCommandBuffer(const VkCommandBuffer cmd, const VkQueue queue, const VkSemaphore timeline, const uint64_t signal) {
  const VkSubmitInfo2 submitInfo2 = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = (VkCommandBufferSubmitInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
              .commandBuffer = cmd,
          },
      },
      .signalSemaphoreInfoCount = 1,
      .pSignalSemaphoreInfos = (VkSemaphoreSubmitInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
              .value = signal,
              .semaphore = timeline,
              .stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          },
      },
  };
  MIDVK_REQUIRE(vkQueueSubmit2(queue, 1, &submitInfo2, VK_NULL_HANDLE));
}
VKM_INLINE void vkmTimelineWait(const VkDevice device, const uint64_t waitValue, const VkSemaphore timeline) {
  const VkSemaphoreWaitInfo semaphoreWaitInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .semaphoreCount = 1,
      .pSemaphores = &timeline,
      .pValues = &waitValue,
  };
  MIDVK_REQUIRE(vkWaitSemaphores(device, &semaphoreWaitInfo, UINT64_MAX));
}
VKM_INLINE void vkmTimelineSignal(const VkDevice device, const uint64_t signalValue, const VkSemaphore timeline) {
  const VkSemaphoreSignalInfo semaphoreSignalInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
      .semaphore = timeline,
      .value = signalValue,
  };
  MIDVK_REQUIRE(vkSignalSemaphore(device, &semaphoreSignalInfo));
}
VKM_INLINE void vkmUpdateDescriptorSet(const VkDevice device, const VkWriteDescriptorSet* pWriteSet) {
  vkUpdateDescriptorSets(device, 1, pWriteSet, 0, NULL);
}

VKM_INLINE void vkmUpdateGlobalSetViewProj(MidTransform* pCameraTransform, VkmGlobalSetState* pState, VkmGlobalSetState* pMapped) {
  pCameraTransform->position = (vec3){0.0f, 0.0f, 2.0f};
  pCameraTransform->euler = (vec3){0.0f, 0.0f, 0.0f};
  pState->framebufferSize = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT};
  vkmMat4Perspective(45.0f, DEFAULT_WIDTH / DEFAULT_HEIGHT, 0.1f, 100.0f, &pState->proj);
  pCameraTransform->rotation = QuatFromEuler(pCameraTransform->euler);
  pState->invProj = Mat4Inv(pState->proj);
  pState->invView = Mat4FromPosRot(pCameraTransform->position, pCameraTransform->rotation);
  pState->view = Mat4Inv(pState->invView);
  pState->viewProj = Mat4Mul(pState->proj, pState->view);
  pState->invViewProj = Mat4Inv(pState->viewProj);
  memcpy(pMapped, pState, sizeof(VkmGlobalSetState));
}
VKM_INLINE void vkmUpdateGlobalSetView(MidTransform* pCameraTransform, VkmGlobalSetState* pState, VkmGlobalSetState* pMapped) {
  pState->invView = Mat4FromPosRot(pCameraTransform->position, pCameraTransform->rotation);
  pState->view = Mat4Inv(pState->invView);
  pState->viewProj = Mat4Mul(pState->proj, pState->view);
  pState->invViewProj = Mat4Inv(pState->viewProj);
  memcpy(pMapped, pState, sizeof(VkmGlobalSetState));
}
VKM_INLINE void vkmUpdateObjectSet(MidTransform* pTransform, VkmStdObjectSetState* pState, VkmStdObjectSetState* pSphereObjectSetMapped) {
  pTransform->rotation = QuatFromEuler(pTransform->euler);
  pState->model = Mat4FromPosRot(pTransform->position, pTransform->rotation);
  memcpy(pSphereObjectSetMapped, pState, sizeof(VkmStdObjectSetState));
}
VKM_INLINE void vkmProcessCameraMouseInput(double deltaTime, vec2 mouseDelta, MidTransform* pCameraTransform) {
  pCameraTransform->euler.y -= mouseDelta.x * deltaTime * 0.4f;
  pCameraTransform->euler.x += mouseDelta.y * deltaTime * 0.4f;
  pCameraTransform->rotation = QuatFromEuler(pCameraTransform->euler);
}
// move[] = Forward, Back, Left, Right
VKM_INLINE void vkmProcessCameraKeyInput(double deltaTime, bool move[4], MidTransform* pCameraTransform) {
  const vec3  localTranslate = Vec3Rot(pCameraTransform->rotation, (vec3){.x = move[3] - move[2], .z = move[1] - move[0]});
  const float moveSensitivity = deltaTime * 0.8f;
  for (int i = 0; i < 3; ++i) pCameraTransform->position.vec[i] += localTranslate.vec[i] * moveSensitivity;
}

//----------------------------------------------------------------------------------
// Methods
//----------------------------------------------------------------------------------

typedef enum VkmDedicatedMemory {
  VKM_DEDICATED_MEMORY_FALSE,
  VKM_DEDICATED_MEMORY_IF_PREFERRED,
  VKM_DEDICATED_MEMORY_FORCE_TRUE,
  VKM_DEDICATED_MEMORY_COUNT,
} VkmDedicatedMemory;
typedef struct VkmRequestAllocationInfo {
  VkMemoryPropertyFlags memoryPropertyFlags;
  VkDeviceSize          size;
  VkBufferUsageFlags    usage;
  MidLocality           locality;
  VkmDedicatedMemory    dedicated;
} VkmRequestAllocationInfo;
void vkmAllocateDescriptorSet(const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet);
//void vkmAllocMemory(const VkMemoryRequirements* pMemReqs, const VkMemoryPropertyFlags propFlags, const MidLocality locality, const VkMemoryDedicatedAllocateInfoKHR* pDedicatedAllocInfo, VkDeviceMemory* pDeviceMemory);
//void vkmCreateAllocBindBuffer(const VkMemoryPropertyFlags memPropFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const MidLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer);
void vkmCreateAllocBindMapBuffer(const VkMemoryPropertyFlags memPropFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const MidLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer, void** ppMapped);
void vkmUpdateBufferViaStaging(const void* srcData, const VkDeviceSize dstOffset, const VkDeviceSize bufferSize, const VkBuffer buffer);
void vkmCreateBufferSharedMemory(const VkmRequestAllocationInfo* pRequest, VkBuffer* pBuffer, MidVkSharedMemory* pMemory);
void vkmCreateMeshSharedMemory(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh);
void vkmBindUpdateMeshSharedMemory(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh);

void vkmBeginAllocationRequests();
void vkmEndAllocationRequests();

typedef struct VkmInitializeDesc {
  // should use this... ? but need to decide on this vs vulkan configurator
  const char* applicationName;
  uint32_t    applicationVersion;

  bool enableMeshTaskShader;
  bool enableBufferRobustness;
  bool enableGlobalPriority;
  bool enableExternalCapabilities;

  bool enableMessageSeverityVerbose;
  bool enableMessageSeverityInfo;
  bool enableMessageSeverityWarnings;
  bool enableMessageSeverityErrors;

  bool enableGeneralMessages;
  bool enableValidationMessages;
  bool enablePerformanceMessages;
} VkmInitializeDesc;
void vkmInitialize();

typedef enum VkmSupport {
  VKM_SUPPORT_OPTIONAL,
  VKM_SUPPORT_YES,
  VKM_SUPPORT_NO,
  VKM_SUPPORT_COUNT,
} VkmSupport;
static const char* string_Support[] = {
    [VKM_SUPPORT_OPTIONAL] = "SUPPORT_OPTIONAL",
    [VKM_SUPPORT_YES] = "SUPPORT_YES",
    [VKM_SUPPORT_NO] = "SUPPORT_NO",
};
typedef struct VkmQueueFamilyCreateInfo {
  VkmSupport               supportsGraphics;
  VkmSupport               supportsCompute;
  VkmSupport               supportsTransfer;
  VkQueueGlobalPriorityKHR globalPriority;
  uint32_t                 queueCount;  // probably get rid of this, we will multiplex queues automatically and presume only 1
  const float*             pQueuePriorities;
} VkmQueueFamilyCreateInfo;
typedef struct VkmContextCreateInfo {
  uint32_t                 maxDescriptorCount;
  uint32_t                 uniformDescriptorCount;
  uint32_t                 combinedImageSamplerDescriptorCount;
  uint32_t                 storageImageDescriptorCount;
  VkSurfaceKHR             presentSurface;
  VkmQueueFamilyCreateInfo queueFamilyCreateInfos[VKM_QUEUE_FAMILY_TYPE_COUNT];
} VkmContextCreateInfo;
void vkmCreateContext(const VkmContextCreateInfo* pContextCreateInfo);

void vkmCreateGlobalSet(VkmGlobalSet* pSet);

void vkmCreateStdRenderPass();
void vkmCreateStdPipeLayout();
void midvkCreateFramebufferTexture(const uint32_t framebufferCount, const MidLocality locality, MidVkFramebufferTexture* pFrameBuffers);
void vkmCreateFramebuffer(const VkRenderPass renderPass, VkFramebuffer* pFramebuffer);

void vkmCreateShaderModule(const char* pShaderPath, VkShaderModule* pShaderModule);

void vkmCreateBasicPipe(const char* vertShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe);
void vkmCreateTessPipe(const char* vertShaderPath, const char* tescShaderPath, const char* teseShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe);
void vkmCreateTaskMeshPipe(const char* taskShaderPath, const char* meshShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe);

typedef struct VkmSamplerCreateInfo {
  VkFilter               filter;
  VkSamplerAddressMode   addressMode;
  VkSamplerReductionMode reductionMode;
} VkmSamplerCreateInfo;
#define VKM_SAMPLER_LINEAR_CLAMP_DESC \
  (const VkmSamplerCreateInfo) { .filter = VK_FILTER_LINEAR, .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE }
void VkmCreateSampler(const VkmSamplerCreateInfo* pDesc, VkSampler* pSampler);

void vkmCreateSwap(const VkSurfaceKHR surface, const VkmQueueFamilyType presentQueueFamily, MidVkSwap* pSwap);

typedef struct VkmTextureCreateInfo {
  const char*        debugName;
  VkImageCreateInfo  imageCreateInfo;
  VkImageAspectFlags aspectMask;
  MidLocality        locality;
  HANDLE             externalHandle;
} VkmTextureCreateInfo;
void midvkCreateTexture(const VkmTextureCreateInfo* pCreateInfo, MidVkTexture* pTexture);
void vkmCreateTextureFromFile(const char* pPath, MidVkTexture* pTexture);


typedef struct MidVkSemaphoreCreateInfo {
  const char*     debugName;
  VkSemaphoreType semaphoreType;
  MidLocality     locality;
  HANDLE          externalHandle;
} MidVkSemaphoreCreateInfo;
void midvkCreateSemaphore(const MidVkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore);

void vkmCreateMesh(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh);

HANDLE GetMemoryExternalHandle(const VkDeviceMemory memory);
HANDLE GetSemaphoreExternalHandle(const VkSemaphore semaphore);

void vkmSetDebugName(VkObjectType objectType, uint64_t objectHandle, const char* pDebugName);

#ifdef WIN32
void midVkCreateVulkanSurface(HINSTANCE hInstance, HWND hWnd, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

//
//// MidVulkan Implementation
#ifdef MID_VULKAN_IMPLEMENTATION

#ifdef WIN32
void midVkCreateVulkanSurface(HINSTANCE hInstance, HWND hWnd, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
  const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = hInstance,
      .hwnd = hWnd,
  };
  VKM_INSTANCE_FUNC(vkCreateWin32SurfaceKHR);
  VKM_REQUIRE(vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pSurface));
}
#endif

#endif