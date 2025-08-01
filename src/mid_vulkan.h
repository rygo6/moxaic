//////////////////////
//// Mid Vulkan Header
//////////////////////
#pragma once

#include "mid_common.h"
#include "mid_math.h"

#include <stdatomic.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <initguid.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include <vulkan/vulkan_win32.h>

#endif

///////////////////
//// Vulkan Globals
////
// these values shouldn't be macros
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 1024
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 1024
#endif

// move everything to this
#ifndef PFN_FUNCS

#define PFN_FUNCS                      \
	PFN_FUNC(WaitSemaphores)           \
	PFN_FUNC(ResetCommandBuffer)       \
	PFN_FUNC(BeginCommandBuffer)       \
	PFN_FUNC(CmdSetViewport)           \
	PFN_FUNC(CmdBeginRenderPass)       \
	PFN_FUNC(CmdSetScissor)            \
	PFN_FUNC(CmdBindPipeline)          \
	PFN_FUNC(CmdDispatch)              \
	PFN_FUNC(CmdBindDescriptorSets)    \
	PFN_FUNC(CmdBindVertexBuffers)     \
	PFN_FUNC(CmdBindIndexBuffer)       \
	PFN_FUNC(CmdDrawIndexed)           \
	PFN_FUNC(CmdEndRenderPass)         \
	PFN_FUNC(EndCommandBuffer)         \
	PFN_FUNC(CmdPipelineBarrier2)      \
	PFN_FUNC(CmdPushDescriptorSetKHR)  \
	PFN_FUNC(CmdClearColorImage)       \
	PFN_FUNC(ResetQueryPool)           \
	PFN_FUNC(GetQueryPoolResults)      \
	PFN_FUNC(CmdWriteTimestamp2)       \
	PFN_FUNC(CmdBlitImage)             \
	PFN_FUNC(AcquireNextImageKHR)      \
	PFN_FUNC(CmdDrawMeshTasksEXT)      \
	PFN_FUNC(SignalSemaphore)          \
	PFN_FUNC(QueueSubmit2)             \
	PFN_FUNC(QueuePresentKHR)          \
	PFN_FUNC(UpdateDescriptorSets)     \
	PFN_FUNC(GetSemaphoreCounterValue)

#endif

#ifdef _WIN32
#define VK_PLATFORM_SURFACE_EXTENSION_NAME         VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_EXTERNAL_MEMORY_EXTENSION_NAME          VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
#define VK_EXTERNAL_SEMAPHORE_EXTENSION_NAME       VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#define VK_EXTERNAL_FENCE_EXTENSION_NAME           VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
#define VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_PLATFORM VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#define VK_EXTERNAL_FENCE_HANDLE_TYPE_PLATFORM     VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#define VK_EXTERNAL_HANDLE_PLATFORM                HANDLE
#else
#define VK_PLATFORM_SURFACE_EXTENSION_NAME         0
#define VK_EXTERNAL_MEMORY_EXTENSION_NAME          0
#define VK_EXTERNAL_SEMAPHORE_EXTENSION_NAME       0
#define VK_EXTERNAL_FENCE_EXTENSION_NAME           0
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM    0
#define VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_PLATFORM 0
#define VK_EXTERNAL_FENCE_HANDLE_TYPE_PLATFORM     0
#define VK_EXTERNAL_HANDLE_PLATFORM                0
#endif

#define VK_ALLOC   NULL
#define VK_VERSION VK_MAKE_API_VERSION(0, 1, 3, 2)  // this prolly redundant?

#define VK_SWAP_COUNT  2
#define VK_SWAP_FORMAT VK_FORMAT_B8G8R8A8_UNORM

#define VK_CHECK(command)                       \
	({                                          \
		VkResult result = command;              \
		CHECK(result, string_VkResult(result)); \
	})

#define VK_ASSERT(_command)                         \
	({                                              \
		VkResult _result = _command;                \
		assert(_result == VK_SUCCESS && #_command); \
	})

#define VK_INSTANCE_FUNC(_func)                                                             \
	PFN_vk##_func _func = (PFN_##vk##_func)vkGetInstanceProcAddr(vk.instance, "vk" #_func); \
	CHECK(_func == NULL, "Couldn't load " #_func)
#define VK_DEVICE_FUNC(_func)                                                                   \
	PFN_vk##_func _func = (PFN_##vk##_func)vkGetDeviceProcAddr(vk.context.device, "vk" #_func); \
	CHECK(_func == NULL, "Couldn't load " #_func)

#define VK_BUFFER_USAGE_MESH                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
#define VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
#define VK_MEMORY_HOST_VISIBLE_COHERENT       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

// I don't think I want this? don't make wrappers to simple things?
#define VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM                        \
	&(VkExternalMemoryImageCreateInfo)                                \
	{                                                                 \
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, \
		.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM,       \
	}

#define VK_COLOR_SUBRESOURCE_RANGE                                                                                    \
	(VkImageSubresourceRange) { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
#define VK_DEPTH_SUBRESOURCE_RANGE                                                                                    \
	(VkImageSubresourceRange) { VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }

/////////////////
//// Vulkan Types
////
typedef enum VkLocality : u8 {
	// Used within the context it was made
	VK_LOCALITY_CONTEXT,
	// Used by multiple contexts, but in the same process
	VK_LOCALITY_PROCESS,
	// Used by nodes external to this context, device and process.
	VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
	VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
	// Used by nodes external to this context, device and process.
	VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
	VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
	VK_LOCALITY_COUNT,
} VkLocality;
#define VK_LOCALITY_INTERPROCESS(_)                         \
	(_ == VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE ||    \
		_ == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY ||  \
		_ == VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE || \
		_ == VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY)
#define VK_LOCALITY_INTERPROCESS_EXPORTED(_)             \
	(_ == VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || \
		_ == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY)
#define VK_LOCALITY_INTERPROCESS_IMPORTED(_)             \
	(_ == VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE || \
		_ == VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY)

typedef struct VkSwapFrame {
	VkSemaphore completeSemaphore;
	VkImage     image;
	VkImageView view;
} VkSwapFrame;

typedef struct VkSwapContext {
	VkSwapchainKHR chain;
	u32            acquireIdx;
	VkSemaphore    acquireSemaphores[VK_SWAP_COUNT];
	u32            frameIdx;
	VkSwapFrame    frames[VK_SWAP_COUNT];
} VkSwapContext;

typedef enum VkSharedMemoryState {
	VK_SHARED_MEMORY_STATE_UNITIALIZED,
	VK_SHARED_MEMORY_STATE_REQUESTED,
	VK_SHARED_MEMORY_STATE_BOUND,
	VK_SHARED_MEMORY_STATE_FREED,
	VK_SHARED_MEMORY_STATE_COUNT,
} VkSharedMemoryState;
typedef u8 VkSharedMemoryType;
typedef struct VkSharedMemory {
	VkDeviceSize        offset;
	VkDeviceSize        size;
	VkSharedMemoryType  type;
	VkSharedMemoryState state;
} VkSharedMemory;

typedef struct VkSharedBuffer {
	VkBuffer       buffer;
	VkSharedMemory memory;
} VkSharedBuffer;

//typedef struct VkTexture {
//	VkImage        image;
//	VkImageView    view;
//	VkSharedMemory sharedMemory;
//} VkTexture;

typedef struct VkDedicatedTexture {
	VkImage        image;
	VkImageView    view;
	VkDeviceMemory memory;
} VkDedicatedTexture;

typedef struct VkMeshOffsets {
	u32          indexCount;
	u32          vertexCount;
	VkDeviceSize indexOffset;
	VkDeviceSize vertexOffset;
} VkMeshOffsets;

typedef struct VkMesh {
	VkDeviceMemory mem;
	VkBuffer       buf;
	VkMeshOffsets  offsets;
} VkMesh;

typedef struct VkSharedMesh {
	VkSharedBuffer sharedBuffer;
	VkMeshOffsets  offsets;
} VkSharedMesh;

typedef struct VkDepthFramebufferTexture {
	VkDedicatedTexture color;
	VkDedicatedTexture depth;
} VkDepthFramebufferTexture;

//typedef struct VkDepthNormalFramebufferTexture {
//	VkDedicatedTexture color;
//	// do I need normal?
//	VkDedicatedTexture normal;
//	VkDedicatedTexture depth;
//} VkDepthNormalFramebufferTexture;

typedef struct VkGlobalSetState {
	mat4  view;
	mat4  proj;
	mat4  viewProj;
	mat4  invView;
	mat4  invProj;
	mat4  invViewProj;
	ivec2 framebufferSize;
} VkGlobalSetState;

typedef struct VkMaterialSetState {
	vec4 color;
} VkMaterialSetState;

typedef struct VkObjectSetState {
	mat4 model;
} VkObjectSetState;

typedef enum VkQueueFamilyType : u8 {
	VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS,
	VK_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE,
	VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER,
	VK_QUEUE_FAMILY_TYPE_COUNT
} VkQueueFamilyType;

static const char* string_QueueFamilyType[] = {
	[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS] = "VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS",
	[VK_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = "VK_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE",
	[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER] = "VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER",
};

typedef struct VkmQueueFamily {
	VkQueue       queue;
	VkCommandPool pool;
	uint32_t      index;
} VkmQueueFamily;

typedef struct VkSharedDescriptor {
	VkDescriptorSet set;
	void*           pMapped;
	VkSharedBuffer  buffer;
} VkSharedDescriptor;

//////////////////////////
//// Vulkan Global Context
////
typedef struct VkContext {
	VkPhysicalDevice physicalDevice;
	VkDevice         device;
	VkmQueueFamily   queueFamilies[VK_QUEUE_FAMILY_TYPE_COUNT];

	VkDescriptorSetLayout globalSetLayout;
	VkDescriptorSetLayout materialSetLayout;
	VkDescriptorSetLayout objectSetLayout;

	VkDescriptorSetLayout materialPushSetLayout;

	VkRenderPass  depthRenderPass;
	VkFramebuffer depthFramebuffer;

	VkPipelineLayout trianglePipeLayout;
	VkPipelineLayout linePipeLayout;

	VkPipeline trianglePipe;
	VkPipeline linePipe;

	VkSampler nearestSampler;
	VkSampler linearSampler;

} VkContext;

//#define VK_CONTEXT_CAPACITY    2 // should be 1 always?
#define VK_SURFACE_CAPACITY 2

typedef struct CACHE_ALIGN Vk {
	VkInstance   instance;
	VkContext    context;
	VkSurfaceKHR surfaces[VK_SURFACE_CAPACITY];

#define PFN_FUNC(_func) PFN_vk##_func _func;
	PFN_FUNCS
#undef PFN_FUNC

} Vk;
extern Vk vk;

// do this ?? I think so yes... enforce thread on vulkan mechanics which need same thread
// should be context and shared context? maybe
typedef struct MidVkThreadContext {
	VkDescriptorPool descriptorPool;
} MidVkThreadContext;
extern __thread MidVkThreadContext threadContext;

// move these into thread context
extern __thread VkDeviceMemory deviceMemory[VK_MAX_MEMORY_TYPES];
extern __thread void*          pMappedMemory[VK_MAX_MEMORY_TYPES];


//////////////////////////
//// Basic Render Pipeline
////

enum {
	VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR,
	VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH,
	VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
};
constexpr VkFormat VK_RENDER_PASS_FORMATS[] = {
	[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR]  = VK_FORMAT_R8G8B8A8_UNORM,
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH]  = VK_FORMAT_D32_SFLOAT,
	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH]  = VK_FORMAT_D16_UNORM,
};
constexpr VkImageUsageFlags VK_RENDER_PASS_USAGES[] = {
	[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR]  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
											   VK_IMAGE_USAGE_STORAGE_BIT          |
											   VK_IMAGE_USAGE_SAMPLED_BIT,
	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH]  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                               VK_IMAGE_USAGE_STORAGE_BIT                  |
                                               VK_IMAGE_USAGE_SAMPLED_BIT,
};

////// DepthNormal Render Pass
//enum {
//	VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR,
//	VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL,
//	VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH,
//	VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COUNT,
//};
//constexpr VkFormat VK_RENDER_PASS_DEPTHNORMAL_FORMATS[] = {
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR]  = VK_FORMAT_R8G8B8A8_UNORM,
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL] = VK_FORMAT_R16G16B16A16_SFLOAT,
////	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH]        = VK_FORMAT_D32_SFLOAT,
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH]  = VK_FORMAT_D16_UNORM,
//};
//constexpr VkImageUsageFlags VK_RENDER_PASS_DEPTHNORMAL_USAGES[] = {
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR]  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
//                                                    VK_IMAGE_USAGE_STORAGE_BIT          |
//                                                    VK_IMAGE_USAGE_SAMPLED_BIT          |
//                                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
//                                                    VK_IMAGE_USAGE_SAMPLED_BIT,
//	[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH]  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
//                                                    VK_IMAGE_USAGE_SAMPLED_BIT,
//};

#define VK_RENDER_PASS_CLEAR_COLOR (VkClearColorValue) {{0.1f, 0.2f, 0.3f, 0.0f}}

//// Pipeline Layouts
// PIPE_SET_INDEX is index of a descriptor set in pipe layout

// Basic
enum {
	VK_PIPE_SET_INDEX_GLOBAL,
	VK_PIPE_SET_INDEX_MATERIAL,
	VK_PIPE_SET_INDEX_OBJECT,
	VK_PIPE_SET_INDEX_COUNT,
};

// Line
#define VK_SHADER_STAGE_LINE VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
enum {
	VK_PIPE_SET_INDEX_LINE_GLOBAL,
	VK_PIPE_SET_INDEX_LINE_COUNT,
};
typedef struct VkLineMaterialState {
	vec4 primaryColor;
	vec4 secondaryColor;
} VkLineMaterialState;
INLINE void vkCmdPushLineMaterial(VkCommandBuffer cmd, VkLineMaterialState state)
{
	vkCmdPushConstants(cmd, vk.context.linePipeLayout, VK_SHADER_STAGE_LINE, 0, sizeof(VkLineMaterialState), &state);
}

//// Descriptor Set Bindings
// SET_BIND_INDEX is index of descriptor binding in a descriptor set
enum {
	VK_SET_BIND_INDEX_GLOBAL_BUFFER,
	VK_SET_BIND_INDEX_COUNT,
};
#define VK_BIND_WRITE_GLOBAL_BUFFER(_set, _buf) (VkWriteDescriptorSet) { \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                          \
			.dstSet = _set,                                              \
			.dstBinding = VK_SET_BIND_INDEX_GLOBAL_BUFFER,               \
			.descriptorCount = 1,                                        \
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         \
			.pBufferInfo = &(VkDescriptorBufferInfo){                    \
				.buffer = _buf,                                          \
				.range = sizeof(VkGlobalSetState),                       \
			},                                                           \
	}

enum {
	VK_SET_BIND_INDEX_MATERIAL_COLOR,
	VK_SET_BIND_INDEX_MATERIAL_IMAGE,
	VK_SET_BIND_INDEX_MATERIAL_COUNT,
};
#define VK_BIND_WRITE_MATERIAL_COLOR(_set, _buf)                 \
	(VkWriteDescriptorSet) {                                     \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
			.dstSet = _set,                                      \
			.dstBinding = VK_SET_BIND_INDEX_MATERIAL_COLOR,      \
			.descriptorCount = 1,                                \
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
			.pBufferInfo = &(VkDescriptorBufferInfo){            \
				.buffer = _buf,                                  \
				.range = sizeof(VkMaterialSetState),             \
			},                                                   \
	}
#define VK_BIND_WRITE_MATERIAL_IMAGE(_set, _view)                    \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = _set,                                              \
		.dstBinding = VK_SET_BIND_INDEX_MATERIAL_IMAGE,              \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = _view,                                      \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}

enum {
	VK_SET_BIND_INDEX_OBJECT_BUFFER,
	VK_SET_BIND_INDEX_OBJECT_COUNT,
};
#define VK_BIND_WRITE_OBJECT_BUFFER(_set, _buf)              \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstSet = _set,                                      \
		.dstBinding = VK_SET_BIND_INDEX_OBJECT_BUFFER,       \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = _buf,                                  \
			.range = sizeof(VkObjectSetState),               \
		},                                                   \
	}


//////////////////////////////
//// Mid Vulkan Image Barriers
////
#define VK_IMAGE_BARRIER_SRC_UNDEFINED        \
	.srcStageMask = VK_PIPELINE_STAGE_2_NONE, \
	.srcAccessMask = VK_ACCESS_2_NONE,        \
	.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED

#define VK_IMAGE_BARRIER_SRC_TRANSFER_WRITE              \
	.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,    \
	.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, \
	.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
#define VK_IMAGE_BARRIER_DST_TRANSFER_WRITE              \
	.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,    \
	.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, \
	.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

#define VK_IMAGE_BARRIER_SRC_COMPUTE_READ                   \
	.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, \
	.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,           \
	.oldLayout = VK_IMAGE_LAYOUT_GENERAL
#define VK_IMAGE_BARRIER_DST_COMPUTE_READ                   \
	.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,           \
	.newLayout = VK_IMAGE_LAYOUT_GENERAL
#define VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE                  \
	.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, \
	.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,          \
	.oldLayout = VK_IMAGE_LAYOUT_GENERAL
#define VK_IMAGE_BARRIER_DST_COMPUTE_WRITE                  \
	.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, \
	.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,          \
	.newLayout = VK_IMAGE_LAYOUT_GENERAL

#define VK_IMAGE_BARRIER_DST_COMPUTE_NONE     \
	.dstStageMask = VK_PIPELINE_STAGE_2_NONE, \
	.dstAccessMask = VK_ACCESS_2_NONE,        \
	.newLayout = VK_IMAGE_LAYOUT_GENERAL

#define VK_IMAGE_BARRIER_SRC_COMPUTE_READ_WRITE                                  \
	.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,                      \
	.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT, \
	.oldLayout = VK_IMAGE_LAYOUT_GENERAL
#define VK_IMAGE_BARRIER_DST_COMPUTE_READ_WRITE                                  \
	.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,                      \
	.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT, \
	.newLayout = VK_IMAGE_LAYOUT_GENERAL

#define VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_WRITE                  \
	.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, \
	.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,         \
	.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL

#define VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_UNDEFINED              \
	.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, \
	.srcAccessMask = VK_ACCESS_2_NONE,                               \
	.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED

#define VK_IMAGE_BARRIER_DST_PRESENT             \
	.dstStageMask = VK_PIPELINE_STAGE_2_NONE,    \
	.dstAccessMask = VK_ACCESS_2_NONE,           \
	.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

#define VK_IMAGE_BARRIER_SRC_BLIT_WRITE                  \
	.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,        \
	.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, \
	.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
#define VK_IMAGE_BARRIER_DST_BLIT_WRITE                  \
	.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,        \
	.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, \
	.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

#define VK_IMAGE_BARRIER_SRC_BLIT_READ                  \
	.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,       \
	.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR, \
	.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
#define VK_IMAGE_BARRIER_DST_BLIT_READ                  \
	.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,       \
	.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR, \
	.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL

#define VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED       \
	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, \
	.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED

#define VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE .subresourceRange = VK_COLOR_SUBRESOURCE_RANGE

//////////////////////////////////
//// Mid Vulkan Inline Cmd Methods
////
#define CMD_PUSH_SETS(_commandBuffer, _pipelineBindPoint, _layout, _set, ...) ({                                              \
	VkWriteDescriptorSet descriptorWrites[] = {__VA_ARGS__};                                                                  \
	vk.CmdPushDescriptorSetKHR(_commandBuffer, _pipelineBindPoint, _layout, _set, COUNT(descriptorWrites), descriptorWrites); \
})

#define CMD_WRITE_SINGLE_SETS(_device, ...) ({                                           \
	VkWriteDescriptorSet descriptorWrites[] = {__VA_ARGS__};                             \
	vkUpdateDescriptorSets(_device, COUNT(descriptorWrites), descriptorWrites, 0, NULL); \
})

#define CMD_IMAGE_BARRIERS(_cmd, ...) ({                          \
	VkImageMemoryBarrier2 barriers[] = {__VA_ARGS__};             \
	CmdPipelineImageBarriers2((_cmd), COUNT(barriers), barriers); \
})

#define CMD_BIND_DESCRIPTOR_SETS(_commandBuffer, _pipelineBindPoint, _layout, _set, ...) ({                          \
	VkDescriptorSet bindSets[] = {__VA_ARGS__};                                                                      \
	vk.CmdBindDescriptorSets(_commandBuffer, _pipelineBindPoint, _layout, _set, COUNT(bindSets), bindSets, 0, NULL); \
})

#define CMD_PUSH_DESCRIPTOR_SETS(_commandBuffer, _pipelineBindPoint, _layout, _set, ...) ({                   \
	VkWriteDescriptorSet pushSets[] = {__VA_ARGS__};                                                          \
	vk.CmdPushDescriptorSetKHR(_commandBuffer, _pipelineBindPoint, _layout, _set, COUNT(pushSets), pushSets); \
})

#define VK_UPDATE_DESCRIPTOR_SETS(...) ({                                             \
	VkWriteDescriptorSet writeSets[] = {__VA_ARGS__};                                 \
	vk.UpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL); \
})

#define VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(familyType)                     \
	for (VkCommandBuffer cmd = vkBeginImmediateCommandBuffer(familyType);   \
		cmd != VK_NULL_HANDLE;                                              \
		vkEndImmediateCommandBuffer(familyType, cmd), cmd = VK_NULL_HANDLE)

INLINE void CmdPipelineImageBarriers2(VkCommandBuffer cmd, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers)
{
	vk.CmdPipelineBarrier2(cmd, &(VkDependencyInfo){VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}

INLINE void CmdBlitImageFullScreen(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage)
{
	VkImageBlit imageBlit = {
		.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1},
		.srcOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_HEIGHT, .z = 1}},
		.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1},
		.dstOffsets = {{.x = 0, .y = 0, .z = 0}, {.x = DEFAULT_WIDTH, .y = DEFAULT_HEIGHT, .z = 1}},
	};
	vk.CmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
}

INLINE void CmdBeginDepthRenderPass(
	VkCommandBuffer   cmd,
	VkRenderPass      renderPass,
	VkFramebuffer     framebuffer,
	VkClearColorValue clearColor,
	VkImageView       colorView,
	VkImageView       depthView)
{
	VkRenderPassBeginInfo renderPassBeginInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		&(VkRenderPassAttachmentBeginInfo){
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
			.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
			.pAttachments = (VkImageView[]){
				[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = colorView,
				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = depthView,
			},
		},
		.renderPass = renderPass,
		.framebuffer = framebuffer,
		.renderArea = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
		.clearValueCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
		.pClearValues = (VkClearValue[]){
			[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = {.color        = clearColor},
			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = {.depthStencil = {0.0f, 0}},
		},
	};
	vk.CmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

//INLINE void CmdBeginDepthNormalRenderPass(
//	VkCommandBuffer   cmd,
//	VkRenderPass      renderPass,
//	VkFramebuffer     framebuffer,
//	VkClearColorValue clearColor,
//	VkImageView       colorView,
//	VkImageView       normalView,
//	VkImageView       depthView)
//{
//	VkRenderPassBeginInfo renderPassBeginInfo = {
//		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
//		&(VkRenderPassAttachmentBeginInfo){
//			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
//			.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
//			.pAttachments    = (VkImageView[]){
//				[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = colorView,
//				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = depthView,
//			},
//		},
//		.renderPass = renderPass,
//		.framebuffer = framebuffer,
//		.renderArea = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}},
//		.clearValueCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
//		.pClearValues = (VkClearValue[]){
//			[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = {.color = clearColor},
//			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = {.depthStencil = {0.0f, 0}},
//		},
//	};
//	vk.CmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
//}

INLINE void CmdResetBegin(VkCommandBuffer cmd)
{
	VK_ASSERT(vk.ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
	VK_ASSERT(vk.BeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
}

INLINE void CmdSwapAcquire(VkDevice device, VkSwapContext* pSwapCtx, u32* pFrameIdx)
{
	pSwapCtx->acquireIdx = (pSwapCtx->acquireIdx + 1) % VK_SWAP_COUNT;
	VK_ASSERT(vk.AcquireNextImageKHR(device, pSwapCtx->chain, UINT64_MAX, pSwapCtx->acquireSemaphores[pSwapCtx->acquireIdx], VK_NULL_HANDLE, &pSwapCtx->frameIdx));
	atomic_thread_fence(memory_order_release);  // We are going to move the submit queue logic into here so we will need some memory barriers
	*pFrameIdx = pSwapCtx->frameIdx;
}

INLINE void CmdSubmitPresent(
	VkCommandBuffer   cmd,
	VkQueueFamilyType queueFamilyType,
	VkSwapContext     swapCtx,
	VkSemaphore       timeline,
	uint64_t          timelineSignalValue)
{
	VkSubmitInfo2 submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = (VkSemaphoreSubmitInfo[]){
			{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = swapCtx.acquireSemaphores[swapCtx.acquireIdx],
				.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
		},
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = (VkCommandBufferSubmitInfo[]){
			{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = cmd,
			},
		},
		.signalSemaphoreInfoCount = 2,
		.pSignalSemaphoreInfos = (VkSemaphoreSubmitInfo[]){
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = swapCtx.frames[swapCtx.frameIdx].completeSemaphore,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			},
			{
				VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.value = timelineSignalValue,
				.semaphore = timeline,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			},
		},
	};
	VK_ASSERT(vk.QueueSubmit2(vk.context.queueFamilies[queueFamilyType].queue, 1, &submitInfo, VK_NULL_HANDLE));
	VkPresentInfoKHR presentInfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &swapCtx.frames[swapCtx.frameIdx].completeSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &swapCtx.chain,
		.pImageIndices = &swapCtx.frameIdx,
	};
	VK_ASSERT(vk.QueuePresentKHR(vk.context.queueFamilies[queueFamilyType].queue, &presentInfo));
}

INLINE void CmdSubmit(VkCommandBuffer cmd, VkQueue queue, VkSemaphore timeline, uint64_t signal)
{
	VkSubmitInfo2 submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
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
				VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.value = signal,
				.semaphore = timeline,
				.stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			},
		},
	};
	VK_CHECK(vk.QueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE));
}

INLINE void vkTimelineWait(VkDevice device, uint64_t waitValue, VkSemaphore timeline)
{
	VkSemaphoreWaitInfo semaphoreWaitInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &timeline,
		.pValues = &waitValue,
	};
	VK_CHECK(vk.WaitSemaphores(device, &semaphoreWaitInfo, UINT64_MAX));
}

// I probably want to remove the vk from this to somehow make it clear you are using PFN cached functions
INLINE void vkTimelineSignal(VkDevice device, uint64_t signalValue, VkSemaphore timeline)
{
	VkSemaphoreSignalInfo signalInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
		.semaphore = timeline,
		.value = signalValue,
	};
	VK_CHECK(vk.SignalSemaphore(device, &signalInfo));
}

INLINE void* vkSharedMemoryPtr(VkSharedMemory shareMemory)
{
	assert(shareMemory.state == VK_SHARED_MEMORY_STATE_BOUND && "Shared buffer not bound!");
	return ((char*)pMappedMemory[shareMemory.type]) + shareMemory.offset;
}
INLINE void* vkSharedBufferPtr(VkSharedBuffer shareBuffer)
{
	return vkSharedMemoryPtr(shareBuffer.memory);
}
INLINE void vkBindSharedBuffer(VkSharedBuffer* pBuffer)
{
	CHECK(pBuffer->memory.state == VK_SHARED_MEMORY_STATE_UNITIALIZED, "Shared buffer has not been requested!");
	CHECK(pBuffer->memory.state == VK_SHARED_MEMORY_STATE_BOUND, "Shared buffer already bound!");
	CHECK(pBuffer->memory.state == VK_SHARED_MEMORY_STATE_FREED, "Shared buffer has been freed!");
	CHECK(pBuffer->memory.state != VK_SHARED_MEMORY_STATE_REQUESTED, "Shared buffer has been requested!");
	CHECK(pBuffer->memory.size <= 0, "Shared buffer has no size initialized!");
	CHECK(pBuffer->memory.type <= 0 && pBuffer->memory.type >= VK_MAX_MEMORY_TYPES, "Shared buffer has no memory initialized!");
	VK_CHECK(vkBindBufferMemory(vk.context.device, pBuffer->buffer, deviceMemory[pBuffer->memory.type], pBuffer->memory.offset));
	pBuffer->memory.state = VK_SHARED_MEMORY_STATE_BOUND;
}

// probably move to math lib and take copy to pointer out
INLINE void vkUpdateGlobalSetViewProj(cam camera, pose cameraPose, VkGlobalSetState* pState, VkGlobalSetState* pMapped)
{
	pState->framebufferSize = IVEC2(DEFAULT_WIDTH, DEFAULT_HEIGHT);
	pState->proj = Mat4PerspectiveVulkanReverseZ(camera.yFovRad, DEFAULT_WIDTH / DEFAULT_HEIGHT, camera.zNear, camera.zFar);
	pState->invProj = mat4Inv(pState->proj);
	pState->invView = mat4FromPosRot(cameraPose.pos, cameraPose.rot);
	pState->view = mat4Inv(pState->invView);
	pState->viewProj = mat4Mul(pState->proj, pState->view);
	pState->invViewProj = mat4Inv(pState->viewProj);
	memcpy(pMapped, pState, sizeof(VkGlobalSetState));
}
INLINE void vkUpdateGlobalSetView(pose* pCameraTransform, VkGlobalSetState* pState, VkGlobalSetState* pMapped)
{
	pState->invView = mat4FromPosRot(pCameraTransform->pos, pCameraTransform->rot);
	pState->view = mat4Inv(pState->invView);
	pState->viewProj = mat4Mul(pState->proj, pState->view);
	pState->invViewProj = mat4Inv(pState->viewProj);
	memcpy(pMapped, pState, sizeof(VkGlobalSetState));
}
INLINE void vkUpdateObjectSet(pose* pTransform, VkObjectSetState* pState, VkObjectSetState* pSphereObjectSetMapped)
{
	pTransform->rot = QuatFromEuler(pTransform->euler);
	pState->model = mat4FromPosRot(pTransform->pos, pTransform->rot);
	memcpy(pSphereObjectSetMapped, pState, sizeof(VkObjectSetState));
}

// this should go in mid math
INLINE void midProcessCameraMouseInput(double deltaTime, vec2 mouseDelta, pose* pCameraTransform)
{
	pCameraTransform->euler.y -= mouseDelta.x * deltaTime * 0.4f;
	pCameraTransform->euler.x += mouseDelta.y * deltaTime * 0.4f;
	pCameraTransform->rot = QuatFromEuler(pCameraTransform->euler);
}

// move[] = Forward, Back, Left, Right
INLINE void midProcessCameraKeyInput(double deltaTime, bool move[4], pose* pCameraTransform)
{
	vec3  localTranslate = vec3Rot(pCameraTransform->rot, (vec3){.x = move[3] - move[2], .y = move[5] - move[4], .z = move[1] - move[0]});
	float moveSensitivity = deltaTime * 0.8f;
	for (int i = 0; i < 3; ++i) pCameraTransform->pos.vec[i] += localTranslate.vec[i] * moveSensitivity;
}

INLINE void vkCmdImageBarriers(VkCommandBuffer commandBuffer, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers)
{
	vkCmdPipelineBarrier2(commandBuffer, &(VkDependencyInfo){VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}

INLINE void vkCmdImageBarrier(VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* pImageMemoryBarrier)
{
	vkCmdPipelineBarrier2(commandBuffer, &(VkDependencyInfo){VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = pImageMemoryBarrier});
}

////////////////////////////////////
//// Mid Vulkan Methods Declarations
////
typedef enum VkDedicatedMemory {
	VK_DEDICATED_MEMORY_FALSE,
	VK_DEDICATED_MEMORY_IF_PREFERRED,
	VK_DEDICATED_MEMORY_FORCE_TRUE,
	VK_DEDICATED_MEMORY_COUNT,
} VkDedicatedMemory;
typedef struct VkRequestAllocationInfo {
	VkMemoryPropertyFlags memoryPropertyFlags;
	VkDeviceSize          size;
	VkBufferUsageFlags    usage;
	VkLocality            locality;
	VkDedicatedMemory     dedicated;
} VkRequestAllocationInfo;
void vkAllocateDescriptorSet(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet);
void vkCreateAllocateBindMapBuffer(VkMemoryPropertyFlags memPropFlags, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer, void** ppMapped);
void vkUpdateBufferViaStaging(const void* srcData, VkDeviceSize dstOffset, VkDeviceSize bufferSize, VkBuffer buffer);
void vkCreateSharedBuffer(const VkRequestAllocationInfo* pRequest, VkSharedBuffer* pBuffer);

typedef struct VkMeshCreateInfo {
	uint32_t        indexCount;
	uint32_t        vertexCount;
	const uint16_t* pIndices;
	const vert*     pVertices;
} VkMeshCreateInfo;
void vkCreateSharedMesh(const VkMeshCreateInfo* pCreateInfo, VkSharedMesh* pMesh);
void vkBindUpdateSharedMesh(const VkMeshCreateInfo* pCreateInfo, VkSharedMesh* pMesh);

void vkBeginAllocationRequests();
void vkEndAllocationRequests();

typedef struct VkmInitializeDesc {
	// should use this... ? but need to decide on this vs vulkan configurator
	// I think I should get rid of this until I need a solution...
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
void vkInitializeInstance();

typedef enum VkSupport {
	VKM_SUPPORT_OPTIONAL,
	VKM_SUPPORT_YES,
	VKM_SUPPORT_NO,
	VKM_SUPPORT_COUNT,
} VkSupport;
static const char* string_Support[] = {
	[VKM_SUPPORT_OPTIONAL] = "SUPPORT_OPTIONAL",
	[VKM_SUPPORT_YES] = "SUPPORT_YES",
	[VKM_SUPPORT_NO] = "SUPPORT_NO",
};
typedef struct VkmQueueFamilyCreateInfo {
	VkSupport                supportsGraphics;
	VkSupport                supportsCompute;
	VkSupport                supportsTransfer;
	VkQueueGlobalPriorityKHR globalPriority;
	uint32_t                 queueCount;  // probably get rid of this, we will multiplex queues automatically and presume only 1
	const float*             pQueuePriorities;
} VkmQueueFamilyCreateInfo;
typedef struct VkContextCreateInfo {
	VkmQueueFamilyCreateInfo queueFamilyCreateInfos[VK_QUEUE_FAMILY_TYPE_COUNT];
} VkContextCreateInfo;
void vkCreateContext(const VkContextCreateInfo* pContextCreateInfo);

void vkCreateShaderModuleFromPath(const char* pShaderPath, VkShaderModule* pShaderModule);

// these might be VkDepthNormal instead of Basic?
typedef struct VkDepthFramebufferTextureCreateInfo {
	VkExtent3D extent;
	VkLocality locality;
} VkDepthFramebufferTextureCreateInfo;
void vkCreateDepthFramebufferTextures(const VkDepthFramebufferTextureCreateInfo* pCreateInfo, VkDepthFramebufferTexture* pFrameBuffer);
//void vkCreateDepthNormalFramebufferTextures(const VkDepthNormalFramebufferTextureCreateInfo* pCreateInfo, VkDepthNormalFramebufferTexture* pFrameBuffer);

typedef struct VkDepthFramebufferCreateInfo {
	u32 width;
	u32 height;
	u32 layerCount;
} VkDepthFramebufferCreateInfo;
void vkCreateDepthFramebuffer(const VkDepthFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer);
//void vkCreateDepthNormalFramebuffer(const VkDepthNormalFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer);

void vkCreateGraphics();
void vkCreateLineGraphics();

// GeometryPipe and get rid of Basic?
void vkCreateTrianglePipe(
	const char*      vertShaderPath,
	const char*      fragShaderPath,
	VkRenderPass     renderPass,
	VkPipelineLayout layout,
	VkPipeline*      pPipe);
void vkCreateTessellationPipe(
	const char*      vertShaderPath,
	const char*      tescShaderPath,
	const char*      teseShaderPath,
	const char*      fragShaderPath,
	VkRenderPass     renderPass,
	VkPipelineLayout layout,
	VkPipeline*      pPipe);
void vkCreateTaskMeshPipe(
	const char*      taskShaderPath,
	const char*      meshShaderPath,
	const char*      fragShaderPath,
	VkRenderPass     renderPass,
	VkPipelineLayout layout,
	VkPipeline*      pPipe);
void vkCreateComputePipe(
	const char*      shaderPath,
	VkPipelineLayout layout,
	VkPipeline*      pPipe);

void vkCreateSwapContext(VkSurfaceKHR surface, VkQueueFamilyType presentQueueFamily, VkSwapContext* pSwap);

typedef struct VkDedicatedTextureCreateInfo {
	const char*                        debugName;
	VkImageAspectFlags                 aspectMask;
	VkLocality                         locality;
	VkExternalMemoryHandleTypeFlagBits handleType;
	VK_EXTERNAL_HANDLE_PLATFORM        importHandle;
	const VkImageCreateInfo*           pImageCreateInfo;  // probably don't want this to be a ptr
} VkDedicatedTextureCreateInfo;
void vkCreateDedicatedTexture(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture);
void vkCreateDedicatedTextureFromFile(const char* pPath, VkDedicatedTexture* pTexture);
void vkDestroyDedicatedTexture(VkDedicatedTexture* pTexture);

typedef struct vkSemaphoreCreateInfoExt {
	const char*                 debugName;
	VkSemaphoreType             semaphoreType;
	VkLocality                  locality;
	VK_EXTERNAL_HANDLE_PLATFORM importHandle;
} vkSemaphoreCreateInfoExt;
void vkCreateSemaphoreExt(const vkSemaphoreCreateInfoExt* pCreateInfo, VkSemaphore* pSemaphore);

typedef struct VkExternalFenceCreateInfo {
	const char*                 debugName;
	VkLocality                  locality;
	VK_EXTERNAL_HANDLE_PLATFORM importHandle;
} VkExternalFenceCreateInfo;
void vkCreateExternalFence(const VkExternalFenceCreateInfo* pCreateInfo, VkFence* pFence);

void vkCreateMesh(const VkMeshCreateInfo* pCreateInfo, VkMesh* pMesh);

VK_EXTERNAL_HANDLE_PLATFORM vkGetMemoryExternalHandle(VkDeviceMemory memory);
VK_EXTERNAL_HANDLE_PLATFORM vkGetFenceExternalHandle(VkFence fence);
VK_EXTERNAL_HANDLE_PLATFORM vkGetSemaphoreExternalHandle(VkSemaphore semaphore);

#define VK_OBJECT_TYPE(obj) _Generic((obj),                                \
	VkBuffer: VK_OBJECT_TYPE_BUFFER,                                       \
	VkImage: VK_OBJECT_TYPE_IMAGE,                                         \
	VkInstance: VK_OBJECT_TYPE_INSTANCE,                                   \
	VkPhysicalDevice: VK_OBJECT_TYPE_PHYSICAL_DEVICE,                      \
	VkDevice: VK_OBJECT_TYPE_DEVICE,                                       \
	VkQueue: VK_OBJECT_TYPE_QUEUE,                                         \
	VkSemaphore: VK_OBJECT_TYPE_SEMAPHORE,                                 \
	VkCommandBuffer: VK_OBJECT_TYPE_COMMAND_BUFFER,                        \
	VkFence: VK_OBJECT_TYPE_FENCE,                                         \
	VkDeviceMemory: VK_OBJECT_TYPE_DEVICE_MEMORY,                          \
	VkEvent: VK_OBJECT_TYPE_EVENT,                                         \
	VkQueryPool: VK_OBJECT_TYPE_QUERY_POOL,                                \
	VkBufferView: VK_OBJECT_TYPE_BUFFER_VIEW,                              \
	VkImageView: VK_OBJECT_TYPE_IMAGE_VIEW,                                \
	VkShaderModule: VK_OBJECT_TYPE_SHADER_MODULE,                          \
	VkPipelineCache: VK_OBJECT_TYPE_PIPELINE_CACHE,                        \
	VkPipelineLayout: VK_OBJECT_TYPE_PIPELINE_LAYOUT,                      \
	VkRenderPass: VK_OBJECT_TYPE_RENDER_PASS,                              \
	VkPipeline: VK_OBJECT_TYPE_PIPELINE,                                   \
	VkDescriptorSetLayout: VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,           \
	VkSampler: VK_OBJECT_TYPE_SAMPLER,                                     \
	VkDescriptorPool: VK_OBJECT_TYPE_DESCRIPTOR_POOL,                      \
	VkDescriptorSet: VK_OBJECT_TYPE_DESCRIPTOR_SET,                        \
	VkFramebuffer: VK_OBJECT_TYPE_FRAMEBUFFER,                             \
	VkCommandPool: VK_OBJECT_TYPE_COMMAND_POOL,                            \
	VkDescriptorUpdateTemplate: VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, \
	VkSurfaceKHR: VK_OBJECT_TYPE_SURFACE_KHR,                              \
	VkSwapchainKHR: VK_OBJECT_TYPE_SWAPCHAIN_KHR,                          \
	VkDisplayKHR: VK_OBJECT_TYPE_DISPLAY_KHR,                              \
	VkDisplayModeKHR: VK_OBJECT_TYPE_DISPLAY_MODE_KHR,                     \
	default: VK_OBJECT_TYPE_UNKNOWN)

#define VK_SET_DEBUG(_)             vkSetDebugName(VK_OBJECT_TYPE(_), (u64)(_), #_)
#define VK_SET_DEBUG_NAME(_, _name) vkSetDebugName(VK_OBJECT_TYPE(_), (u64)(_), _name)

void vkSetDebugName(VkObjectType objectType, uint64_t objectHandle, const char* pDebugName);

VkCommandBuffer vkBeginImmediateCommandBuffer(VkQueueFamilyType queueFamilyType);
void            vkEndImmediateCommandBuffer(VkQueueFamilyType queueFamilyType, VkCommandBuffer cmd);
VkCommandBuffer vkBeginImmediateTransferCommandBuffer();
void            vkEndImmediateTransferCommandBuffer(VkCommandBuffer cmd);

#ifdef _WIN32
typedef struct VkExternalPlatformTexture {
	ID3D12Resource* texture;
	HANDLE          handle;
} VkExternalPlatformTexture;
void vkCreateExternalPlatformTexture(const VkImageCreateInfo* pCreateInfo, VkExternalPlatformTexture* pTexture);
void vkDestroyExternalPlatformTexture(VkExternalPlatformTexture* pTexture);

void vkCreateVulkanSurface(HINSTANCE hInstance, HWND hWnd, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

typedef struct VkExternalTexture {
	VkDedicatedTexture texture;
#if _WIN32
	VkExternalPlatformTexture platform;
#endif
} VkExternalTexture;


//////////////////////////////
//// Mid Vulkan Implementation
//////////////////////////////
#if defined(MID_VULKAN_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

#include "stb_image.h"  // abstract this out?

////////////////////
//// Global Context
////
CACHE_ALIGN Vk                               vk = {};
_Thread_local CACHE_ALIGN MidVkThreadContext threadContext = {};
VkDebugUtilsMessengerEXT                     debugUtilsMessenger = VK_NULL_HANDLE;

/////////////
//// Utility
////
// move to common?
static void vkReadFile(const char* pPath, size_t* pFileLength, char** ppFileContents)
{
	FILE* file = fopen(pPath, "rb");
	CHECK(file == NULL, "File can't be opened!");
	fseek(file, 0, SEEK_END);
	*pFileLength = ftell(file);
	rewind(file);
	*ppFileContents = malloc(*pFileLength * sizeof(char));
	size_t readCount = fread(*ppFileContents, *pFileLength, 1, file);
	CHECK(readCount == 0, "Failed to read file!");
	fclose(file);
}

static VkBool32 VkmDebugUtilsCallback(
	const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	const VkDebugUtilsMessageTypeFlagsEXT        messageType,
	const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
	void*                                        pUserData)
{
	switch (messageSeverity) {
		default:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("%s\n", pCallbackData->pMessage); return VK_FALSE;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   PANIC(pCallbackData->pMessage); return VK_FALSE;
	}
}

//////////////////////////////
//// Immediate Command Buffers
////
// One shot command buffers for convenience in non-performance-critical commands. Like initial buffer priming.
VkCommandBuffer vkBeginImmediateCommandBuffer(VkQueueFamilyType queueFamilyType)
{
	VkCommandBufferAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vk.context.queueFamilies[queueFamilyType].pool,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &allocateInfo, &cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)cmd, "ImmediateCommandBuffer");
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
	return cmd;
}

void vkEndImmediateCommandBuffer(VkQueueFamilyType queueFamilyType, VkCommandBuffer cmd)
{
	VK_CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};
	VK_CHECK(vkQueueSubmit(vk.context.queueFamilies[queueFamilyType].queue, 1, &info, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(vk.context.queueFamilies[queueFamilyType].queue));
	vkFreeCommandBuffers(vk.context.device, vk.context.queueFamilies[queueFamilyType].pool, 1, &cmd);
}

VkCommandBuffer vkBeginImmediateTransferCommandBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].pool,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &allocateInfo, &cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)cmd, "ImmediateTransferCommandBuffer");
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
	return cmd;
}

void vkEndImmediateTransferCommandBuffer(VkCommandBuffer cmd)
{
	VK_CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};
	VK_CHECK(vkQueueSubmit(vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].queue, 1, &info, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].queue));
	vkFreeCommandBuffers(vk.context.device, vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].pool, 1, &cmd);
}

//////////////
//// Pipelines
////
#define COLOR_WRITE_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

void vkCreateShaderModuleFromPath(const char* pShaderPath, VkShaderModule* pShaderModule)
{
	size_t size;
	char*  pCode;
	vkReadFile(pShaderPath, &size, &pCode);
	VkShaderModuleCreateInfo info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = (uint32_t*)pCode,
	};
	VK_CHECK(vkCreateShaderModule(vk.context.device, &info, VK_ALLOC, pShaderModule));
	vkSetDebugName(VK_OBJECT_TYPE_SHADER_MODULE, (u64)*pShaderModule, pShaderPath);
	free(pCode);
}

static void CreateDepthRenderPass()
{
	VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
		.pAttachments    = (VkAttachmentDescription2[]){
			[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = {
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format         = VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
				.samples        = VK_SAMPLE_COUNT_1_BIT,
				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout    = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			},
			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = {
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format         = VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH],
				.samples        = VK_SAMPLE_COUNT_1_BIT,
				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout    = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			},
		},
		.subpassCount = 1,
		.pSubpasses   = (VkSubpassDescription2[]){
			{
				VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
				.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments    = (VkAttachmentReference2[]){
					[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = {
						VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.attachment = VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR,
						.layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					},
				},
				.pDepthStencilAttachment = &(VkAttachmentReference2){
					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
					.attachment = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH,
					.layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				},
			},
		},
		.dependencyCount = 2,
		.pDependencies   = (VkSubpassDependency2[]){
			// from here https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#combined-graphicspresent-queue
			{
				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
				.srcSubpass      = VK_SUBPASS_EXTERNAL,
				.dstSubpass      = 0,
				.srcStageMask    = VK_PIPELINE_STAGE_TRANSFER_BIT,
				.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask   = VK_ACCESS_TRANSFER_READ_BIT,
				.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = 0,
			},
			{
				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
				.srcSubpass      = 0,
				.dstSubpass      = VK_SUBPASS_EXTERNAL,
				.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask    = VK_PIPELINE_STAGE_TRANSFER_BIT,
				.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask   = VK_ACCESS_TRANSFER_READ_BIT,
				.dependencyFlags = 0,
			},
		},
	};
	VK_CHECK(vkCreateRenderPass2(vk.context.device, &renderPassCreateInfo2, VK_ALLOC, &vk.context.depthRenderPass));
	VK_SET_DEBUG(vk.context.depthRenderPass);
}

//static void CreateDepthNormalRenderPass()
//{
//	VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
//		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
//		.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COUNT,
//		.pAttachments    = (VkAttachmentDescription2[]){
//			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR] = {
//				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
//				.format         = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR],
//				.samples        = VK_SAMPLE_COUNT_1_BIT,
//				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
//				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
//				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
//				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
//				.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
//				.finalLayout    = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//			},
//			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL] = {
//				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
//				.format         = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL],
//				.samples        = VK_SAMPLE_COUNT_1_BIT,
//				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
//				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
//				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
//				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
//				.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
//				.finalLayout    = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//			},
//			[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH] = {
//				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
//				.format         = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH],
//				.samples        = VK_SAMPLE_COUNT_1_BIT,
//				.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
//				.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
//				.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
//				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
//				.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
//				.finalLayout    = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//			},
//		},
//		.subpassCount = 1,
//		.pSubpasses   = (VkSubpassDescription2[]){
//			{
//				VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
//				.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
//				.colorAttachmentCount = 2,
//				.pColorAttachments    = (VkAttachmentReference2[]){
//					[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR] = {
//						VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
//						.attachment = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR,
//						.layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//					},
//					[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL] = {
//						VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
//						.attachment = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL,
//						.layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//					},
//				},
//				.pDepthStencilAttachment = &(VkAttachmentReference2){
//					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
//					.attachment = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH,
//					.layout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
//					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
//				},
//			},
//		},
//		.dependencyCount = 2,
//		.pDependencies   = (VkSubpassDependency2[]){
//			// from here https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#combined-graphicspresent-queue
//			{
//				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
//				.srcSubpass      = VK_SUBPASS_EXTERNAL,
//				.dstSubpass      = 0,
//				.srcStageMask    = VK_PIPELINE_STAGE_TRANSFER_BIT,
//				.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//				.srcAccessMask   = VK_ACCESS_TRANSFER_READ_BIT,
//				.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//				.dependencyFlags = 0,
//			},
//			{
//				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
//				.srcSubpass      = 0,
//				.dstSubpass      = VK_SUBPASS_EXTERNAL,
//				.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//				.dstStageMask    = VK_PIPELINE_STAGE_TRANSFER_BIT,
//				.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//				.dstAccessMask   = VK_ACCESS_TRANSFER_READ_BIT,
//				.dependencyFlags = 0,
//			},
//		},
//	};
//	VK_CHECK(vkCreateRenderPass2(vk.context.device, &renderPassCreateInfo2, VK_ALLOC, &vk.context.basicPass));
//	VK_SET_DEBUG(vk.context.basicPass);
//}

static void CreateSamplers()
{
	VkSamplerCreateInfo linearCreateInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.maxLod = 16.0,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	};
	VK_CHECK(vkCreateSampler(vk.context.device, &linearCreateInfo, VK_ALLOC, &vk.context.linearSampler));
	VK_SET_DEBUG(vk.context.linearSampler);

	VkSamplerCreateInfo nearestCreateInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.maxLod = 16.0,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	};
	VK_CHECK(vkCreateSampler(vk.context.device, &nearestCreateInfo, VK_ALLOC, &vk.context.nearestSampler));
	VK_SET_DEBUG(vk.context.nearestSampler);
}

//// Basic Pipeline

enum {
	VK_PIPE_VERTEX_BINDING_INDEX,
	VK_PIPE_VERTEX_BINDING_COUNT,
};
enum {
	VK_PIPE_VERTEX_ATTRIBUTE_POSITION_INDEX,
	VK_PIPE_VERTEX_ATTRIBUTE_NORMAL_INDEX,
	VK_PIPE_VERTEX_ATTRIBUTE_UV_INDEX,
	VK_PIPE_VERTEX_ATTRIBUTE_COUNT,
};

#define DEFAULT_ROBUSTNESS_STATE                                                             \
	(VkPipelineRobustnessCreateInfoEXT){                                                     \
		VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,                               \
		.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT, \
		.uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT, \
		.vertexInputs   = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT, \
		.images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT,   \
	}
#define DEFAULT_VERTEX_INPUT_STATE                                                \
	(VkPipelineVertexInputStateCreateInfo){                                       \
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,                \
		.vertexBindingDescriptionCount = VK_PIPE_VERTEX_BINDING_COUNT,            \
		.pVertexBindingDescriptions    = (VkVertexInputBindingDescription[]){     \
			{                                                                     \
				.binding   = VK_PIPE_VERTEX_BINDING_INDEX,                        \
				.stride    = sizeof(vert),                                        \
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,                         \
			},                                                                    \
		},                                                                        \
		.vertexAttributeDescriptionCount = VK_PIPE_VERTEX_ATTRIBUTE_COUNT,        \
		.pVertexAttributeDescriptions    = (VkVertexInputAttributeDescription[]){ \
			{                                                                     \
				.location = VK_PIPE_VERTEX_ATTRIBUTE_POSITION_INDEX,              \
				.binding  = VK_PIPE_VERTEX_BINDING_INDEX,                         \
				.format   = VK_FORMAT_R32G32B32_SFLOAT,                           \
				.offset   = offsetof(vert, pos),                                  \
			},                                                                    \
			{                                                                     \
				.location = VK_PIPE_VERTEX_ATTRIBUTE_NORMAL_INDEX,                \
				.binding  = VK_PIPE_VERTEX_BINDING_INDEX,                         \
				.format   = VK_FORMAT_R32G32B32_SFLOAT,                           \
				.offset   = offsetof(vert, norm),                                 \
			},                                                                    \
			{                                                                     \
				.location = VK_PIPE_VERTEX_ATTRIBUTE_UV_INDEX,                    \
				.binding  = VK_PIPE_VERTEX_BINDING_INDEX,                         \
				.format   = VK_FORMAT_R32G32_SFLOAT,                              \
				.offset   = offsetof(vert, uv),                                   \
			},                                                                    \
		},                                                                        \
	}
#define DEFAULT_VIEWPORT_STATE                                 \
	(VkPipelineViewportStateCreateInfo){                       \
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, \
		.viewportCount = 1,                                    \
		.scissorCount = 1,                                     \
	}
#ifdef VK_DEBUG_WIREFRAME
#define FILL_MODE VK_POLYGON_MODE_LINE
#else
#define FILL_MODE VK_POLYGON_MODE_FILL
#endif
#define DEFAULT_RASTERIZATION_STATE                                 \
	(VkPipelineRasterizationStateCreateInfo){                       \
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, \
		.polygonMode = FILL_MODE,                                   \
		.frontFace   = VK_FRONT_FACE_CLOCKWISE,                     \
		.lineWidth   = 1.0f,                                        \
	}
#define DEFAULT_DEPTH_STENCIL_STATE                                 \
	(VkPipelineDepthStencilStateCreateInfo){                        \
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, \
		.depthTestEnable  = VK_TRUE,                                \
		.depthWriteEnable = VK_TRUE,                                \
		.depthCompareOp   = VK_COMPARE_OP_GREATER,                  \
		.maxDepthBounds   = 1.0f,                                   \
	}
#define DEFAULT_DYNAMIC_STATE                                 \
	(VkPipelineDynamicStateCreateInfo){                       \
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, \
		.dynamicStateCount = 2,                               \
		.pDynamicStates    = (VkDynamicState[]){              \
			VK_DYNAMIC_STATE_VIEWPORT,                        \
			VK_DYNAMIC_STATE_SCISSOR,                         \
		},                                                    \
	}
#define DEFAULT_COLOR_BLEND_ATTACHMENT_STATE_ALPHA_BLEND  \
	(VkPipelineColorBlendAttachmentState){                \
		.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | \
		                       VK_COLOR_COMPONENT_G_BIT | \
		                       VK_COLOR_COMPONENT_B_BIT,  \
		.blendEnable         = VK_FALSE,                  \
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,       \
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,      \
		.colorBlendOp        = VK_BLEND_OP_ADD,           \
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,       \
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,      \
		.alphaBlendOp        = VK_BLEND_OP_ADD,           \
	}
#define DEFAULT_COLOR_BLEND_ATTACHMENT_STATE_OPAQUE \
	(VkPipelineColorBlendAttachmentState){          \
		.colorWriteMask = COLOR_WRITE_MASK_RGBA,    \
	}
#define DEFAULT_COLOR_BLEND_STATE_OPAQUE                             \
	(VkPipelineColorBlendStateCreateInfo){                           \
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,    \
		.logicOp = VK_LOGIC_OP_COPY,                                 \
		.attachmentCount = 1,                                        \
		.pAttachments = (VkPipelineColorBlendAttachmentState[]){     \
			/* Color */ DEFAULT_COLOR_BLEND_ATTACHMENT_STATE_OPAQUE, \
		},                                                           \
	}
#define DEFAULT_COLOR_BLEND_STATE_ALPHA_BLEND                             \
	(VkPipelineColorBlendStateCreateInfo){                                \
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,         \
		.logicOp = VK_LOGIC_OP_COPY,                                      \
		.attachmentCount = 2,                                             \
		.pAttachments = (VkPipelineColorBlendAttachmentState[]){          \
			/* Color */ DEFAULT_COLOR_BLEND_ATTACHMENT_STATE_ALPHA_BLEND, \
		},                                                                \
	}

static void CreateBasicPipeLayout()
{
	VkDescriptorSetLayout pSetLayouts[] = {
		[VK_PIPE_SET_INDEX_GLOBAL]   = vk.context.globalSetLayout,
		[VK_PIPE_SET_INDEX_MATERIAL] = vk.context.materialSetLayout,
		[VK_PIPE_SET_INDEX_OBJECT]   = vk.context.objectSetLayout,
	};
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = VK_PIPE_SET_INDEX_COUNT,
		.pSetLayouts    = pSetLayouts,
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, &vk.context.trianglePipeLayout));
	VK_SET_DEBUG(vk.context.trianglePipeLayout);
}

static void CreateGlobalSetLayout()
{
	VkDescriptorSetLayoutCreateInfo info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = VK_SET_BIND_INDEX_COUNT,
		.pBindings    = &(VkDescriptorSetLayoutBinding){
			.binding         = VK_SET_BIND_INDEX_GLOBAL_BUFFER,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags =
				// these need to be enabled by some flags...
				VK_SHADER_STAGE_VERTEX_BIT                  |
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
				VK_SHADER_STAGE_COMPUTE_BIT                 |
				VK_SHADER_STAGE_FRAGMENT_BIT                |
				VK_SHADER_STAGE_MESH_BIT_EXT                |
				VK_SHADER_STAGE_TASK_BIT_EXT,
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, &vk.context.globalSetLayout));
	VK_SET_DEBUG(vk.context.globalSetLayout);
}

static void CreateMaterialSetLayout()
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = VK_SET_BIND_INDEX_MATERIAL_COUNT,
		.pBindings    = (VkDescriptorSetLayoutBinding[]){
			{
				.binding         = VK_SET_BIND_INDEX_MATERIAL_COLOR,
				.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding            = VK_SET_BIND_INDEX_MATERIAL_IMAGE,
				.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount    = 1,
				.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, &vk.context.materialSetLayout));
	VK_SET_DEBUG(vk.context.materialSetLayout);

	createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, &vk.context.materialPushSetLayout));
	VK_SET_DEBUG(vk.context.materialPushSetLayout);
}

static void CreateObjectSetLayout()
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &(VkDescriptorSetLayoutBinding){
			.binding = VK_SET_BIND_INDEX_OBJECT_BUFFER,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
	                      VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, &vk.context.objectSetLayout));
	VK_SET_DEBUG(vk.context.objectSetLayout);
}

void vkCreateTrianglePipe(const char* vertShaderPath, const char* fragShaderPath, VkRenderPass renderPass, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule vertShader;
	vkCreateShaderModuleFromPath(vertShaderPath, &vertShader);
	VkShaderModule fragShader;
	vkCreateShaderModuleFromPath(fragShaderPath, &fragShader);
	VkGraphicsPipelineCreateInfo pipelineInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &DEFAULT_ROBUSTNESS_STATE,
		.stageCount = 2,
		.pStages = (VkPipelineShaderStageCreateInfo[]){
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragShader,
				.pName = "main",
			},
		},
		.pVertexInputState = &DEFAULT_VERTEX_INPUT_STATE,
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.pViewportState = &DEFAULT_VIEWPORT_STATE,
		.pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
		.pColorBlendState = &DEFAULT_COLOR_BLEND_STATE_OPAQUE,
		.pDynamicState = &DEFAULT_DYNAMIC_STATE,
		.layout = layout,
		.renderPass = renderPass,
	};
	VK_CHECK(vkCreateGraphicsPipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));
	vkDestroyShaderModule(vk.context.device, fragShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, vertShader, VK_ALLOC);
}

void vkCreateTessellationPipe(const char* vertShaderPath, const char* tescShaderPath, const char* teseShaderPath, const char* fragShaderPath, VkRenderPass renderPass, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule vertShader;
	vkCreateShaderModuleFromPath(vertShaderPath, &vertShader);
	VkShaderModule tescShader;
	vkCreateShaderModuleFromPath(tescShaderPath, &tescShader);
	VkShaderModule teseShader;
	vkCreateShaderModuleFromPath(teseShaderPath, &teseShader);
	VkShaderModule fragShader;
	vkCreateShaderModuleFromPath(fragShaderPath, &fragShader);
	VkGraphicsPipelineCreateInfo pipelineInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &DEFAULT_ROBUSTNESS_STATE,
		.stageCount = 4,
		.pStages = (VkPipelineShaderStageCreateInfo[]){
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				.module = tescShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				.module = teseShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragShader,
				.pName = "main",
			},
		},
		.pVertexInputState = &DEFAULT_VERTEX_INPUT_STATE,
		.pTessellationState = &(VkPipelineTessellationStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.patchControlPoints = 4,
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
		},
		.pViewportState = &DEFAULT_VIEWPORT_STATE,
		.pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
		.pColorBlendState = &DEFAULT_COLOR_BLEND_STATE_OPAQUE,
		.pDynamicState = &DEFAULT_DYNAMIC_STATE,
		.layout = layout,
		.renderPass = renderPass,
	};
	VK_CHECK(vkCreateGraphicsPipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));
	vkDestroyShaderModule(vk.context.device, fragShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, tescShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, teseShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, vertShader, VK_ALLOC);
}

void vkCreateTaskMeshPipe(const char* taskShaderPath, const char* meshShaderPath, const char* fragShaderPath, VkRenderPass renderPass, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule taskShader;
	vkCreateShaderModuleFromPath(taskShaderPath, &taskShader);
	VkShaderModule meshShader;
	vkCreateShaderModuleFromPath(meshShaderPath, &meshShader);
	VkShaderModule fragShader;
	vkCreateShaderModuleFromPath(fragShaderPath, &fragShader);
	VkGraphicsPipelineCreateInfo pipelineInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &DEFAULT_ROBUSTNESS_STATE,
		.stageCount = 3,
		.pStages = (VkPipelineShaderStageCreateInfo[]){
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_TASK_BIT_EXT,
				.module = taskShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_MESH_BIT_EXT,
				.module = meshShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragShader,
				.pName = "main",
			},
		},
		.pViewportState = &DEFAULT_VIEWPORT_STATE,
		.pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
		.pColorBlendState = &DEFAULT_COLOR_BLEND_STATE_OPAQUE,
		.pDynamicState = &DEFAULT_DYNAMIC_STATE,
		.layout = layout,
		.renderPass = renderPass,
	};
	VK_CHECK(vkCreateGraphicsPipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));
	vkDestroyShaderModule(vk.context.device, fragShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, taskShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, meshShader, VK_ALLOC);
}

//// Line Pipeline
enum {
	VK_PIPE_VERTEX_BINDING_LINE_INDEX,
	VK_PIPE_VERTEX_BINDING_LINE_COUNT,
};
enum {
	VK_PIPE_VERTEX_ATTRIBUTE_LINE_POSITION_INDEX,
	VK_PIPE_VERTEX_ATTRIBUTE_LINE_COUNT,
};

static void CreateLinePipeLayout()
{
	VkDescriptorSetLayout pSetLayouts[VK_PIPE_SET_INDEX_LINE_COUNT] = {
		[VK_PIPE_SET_INDEX_LINE_GLOBAL] = vk.context.globalSetLayout,
	};
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = VK_PIPE_SET_INDEX_LINE_COUNT,
		.pSetLayouts = pSetLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = (VkPushConstantRange[]){
			{.stageFlags = VK_SHADER_STAGE_LINE,
				.offset = 0,
				.size = sizeof(VkLineMaterialState)},
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, &vk.context.linePipeLayout));
}

void vkCreateLinePipe(const char* vertShaderPath, const char* fragShaderPath, VkRenderPass renderPass, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule vertShader;
	vkCreateShaderModuleFromPath(vertShaderPath, &vertShader);
	VkShaderModule fragShader;
	vkCreateShaderModuleFromPath(fragShaderPath, &fragShader);

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		&DEFAULT_ROBUSTNESS_STATE,
		.stageCount = 2,
		.pStages = (VkPipelineShaderStageCreateInfo[]){
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertShader,
				.pName = "main",
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragShader,
				.pName = "main",
			},
		},
		.pVertexInputState = &(VkPipelineVertexInputStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = VK_PIPE_VERTEX_BINDING_LINE_COUNT,
			.pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
				{
					.binding = VK_PIPE_VERTEX_BINDING_LINE_INDEX,
					.stride = sizeof(vec3),
					.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
				},
			},
			.vertexAttributeDescriptionCount = VK_PIPE_VERTEX_ATTRIBUTE_LINE_COUNT,
			.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
				{.binding = VK_PIPE_VERTEX_BINDING_LINE_INDEX, .location = VK_PIPE_VERTEX_ATTRIBUTE_LINE_POSITION_INDEX, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
			},
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		},
		.pViewportState = &DEFAULT_VIEWPORT_STATE,
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			&(VkPipelineRasterizationLineStateCreateInfoEXT){
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,
				.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT,
				.stippledLineEnable = VK_FALSE,
			},
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.lineWidth = 4.0f,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
		.pColorBlendState = &DEFAULT_COLOR_BLEND_STATE_OPAQUE,
		.pDynamicState = &(VkPipelineDynamicStateCreateInfo){
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 3,
			.pDynamicStates = (VkDynamicState[]){
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_LINE_WIDTH,
			},
		},
		.layout = layout,
		.renderPass = renderPass,
	};
	VK_CHECK(vkCreateGraphicsPipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));

	vkDestroyShaderModule(vk.context.device, fragShader, VK_ALLOC);
	vkDestroyShaderModule(vk.context.device, vertShader, VK_ALLOC);
}

//// Compute Pipeline
void vkCreateComputePipe(const char* shaderPath, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule shader;
	vkCreateShaderModuleFromPath(shaderPath, &shader);
	VkComputePipelineCreateInfo pipelineInfo = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = shader,
			.pName = "main",
		},
		.layout = layout,
	};
	VK_CHECK(vkCreateComputePipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));
	vkDestroyShaderModule(vk.context.device, shader, VK_ALLOC);
}

void vkCreateGraphics()
{
	CreateSamplers();
	CreateDepthRenderPass();

	VkDepthFramebufferCreateInfo framebufferInfo = {
		.width = DEFAULT_WIDTH,
		.height = DEFAULT_HEIGHT,
		.layerCount = 1,
	};
	vkCreateDepthFramebuffer(&framebufferInfo, &vk.context.depthFramebuffer);
	VK_SET_DEBUG(vk.context.depthFramebuffer);

	CreateGlobalSetLayout();
	CreateMaterialSetLayout();
	CreateObjectSetLayout();

	CreateBasicPipeLayout();
	vkCreateTrianglePipe(
		"./shaders/basic_material.vert.spv",
		"./shaders/basic_material.frag.spv",
		vk.context.depthRenderPass,
		vk.context.trianglePipeLayout,
		&vk.context.trianglePipe);
	VK_SET_DEBUG(vk.context.trianglePipe);
}

void vkCreateLineGraphics()
{
	CreateLinePipeLayout();
	vkCreateLinePipe("./shaders/line.vert.spv",
		"./shaders/line.frag.spv",
		vk.context.depthRenderPass,
		vk.context.linePipeLayout,
		&vk.context.linePipe);
}

////////////////
//// Descriptors
////
// get rid of this don't wrap methods that don't actually simplify the structs
void vkAllocateDescriptorSet(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet)
{
	VkDescriptorSetAllocateInfo allocateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = pSetLayout,
	};
	VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &allocateInfo, pSet));
}

///////////
//// Memory
////
static void LogFlags(const char* prefix, const char* postfix, VkFlags flags, const char* (*string_Method)(VkFlags flags))
{
	int index = 0;
	while (flags) {
		if (flags & 1)
			LOG("%s%s%s", prefix, string_Method(1U << index), postfix);

		++index;
		flags >>= 1;
	}
}

// could be combined with above
static void PrintMemoryPropertyFlags(VkMemoryPropertyFlags propFlags)
{
	int index = 0;
	while (propFlags) {
		if (propFlags & 1)
			LOG("%s ", strlen("VK_MEMORY_PROPERTY_") + string_VkMemoryPropertyFlagBits(1U << index));

		++index;
		propFlags >>= 1;
	}
}

static uint32_t FindMemoryTypeIndex(
	uint32_t              memoryTypeCount,
	VkMemoryType*         pMemoryTypes,
	uint32_t              memoryTypeBits,
	VkMemoryPropertyFlags memPropFlags)
{
	for (uint32_t i = 0; i < memoryTypeCount; i++) {
		bool hasTypeBits = memoryTypeBits & 1 << i;
		bool hasPropertyFlags = (pMemoryTypes[i].propertyFlags & memPropFlags) == memPropFlags;
		if (hasTypeBits && hasPropertyFlags) {
			return i;
		}
	}
	PrintMemoryPropertyFlags(memPropFlags);
	LOG("\n");
	PANIC("Failed to find memory with properties!");
	return -1;
}

// todo should go in thread context
// but should this be forced by thread local
static __thread size_t  requestedMemoryAllocSize[VK_MAX_MEMORY_TYPES] = {};
static __thread size_t  externalRequestedMemoryAllocSize[VK_MAX_MEMORY_TYPES] = {};
__thread VkDeviceMemory deviceMemory[VK_MAX_MEMORY_TYPES] = {};
__thread void*          pMappedMemory[VK_MAX_MEMORY_TYPES] = {};

void vkBeginAllocationRequests()
{
	LOG("Begin Memory Allocation Requests.\n");
	// what do I do here? should I enable a mechanic to do this twice? or on pass in memory?
	for (u32 memTypeIndex = 0; memTypeIndex < VK_MAX_MEMORY_TYPES; ++memTypeIndex) {
		requestedMemoryAllocSize[memTypeIndex] = 0;
	}
}
void vkEndAllocationRequests()
{
	printf("End Memory Allocation Requests.\n");
	VkPhysicalDeviceMemoryProperties2 memProps2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
	vkGetPhysicalDeviceMemoryProperties2(vk.context.physicalDevice, &memProps2);
	for (u32 memTypeIndex = 0; memTypeIndex < VK_MAX_MEMORY_TYPES; ++memTypeIndex) {
		if (requestedMemoryAllocSize[memTypeIndex] == 0) continue;
		VkMemoryAllocateInfo memAllocInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = requestedMemoryAllocSize[memTypeIndex],
			.memoryTypeIndex = memTypeIndex,
		};
		VK_CHECK(vkAllocateMemory(vk.context.device, &memAllocInfo, VK_ALLOC, &deviceMemory[memTypeIndex]));

		VkMemoryPropertyFlags propFlags = memProps2.memoryProperties.memoryTypes[memTypeIndex].propertyFlags;
		bool                  hasDeviceLocal = (propFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		bool                  hasHostVisible = (propFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		bool                  hasHostCoherent = (propFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		if (hasDeviceLocal && hasHostVisible && hasHostCoherent)
			VK_CHECK(vkMapMemory(vk.context.device, deviceMemory[memTypeIndex], 0, requestedMemoryAllocSize[memTypeIndex], 0, &pMappedMemory[memTypeIndex]));
#ifdef VK_DEBUG_MEMORY_ALLOC
		printf("Shared MemoryType: %d Allocated: %zu Mapped %d", memTypeIndex, requestedMemoryAllocSize[memTypeIndex], pMappedMemory[memTypeIndex] != NULL);
		PrintMemoryPropertyFlags(propFlags);
#endif
	}
}

static void AllocateMemory(const VkMemoryRequirements* pMemReqs, VkMemoryPropertyFlags propFlags, VkLocality locality, VkExternalMemoryHandleTypeFlagBits importHandleType, HANDLE importHandle, const VkMemoryDedicatedAllocateInfo* pDedicatedAllocInfo, VkDeviceMemory* pDeviceMemory)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(vk.context.physicalDevice, &memProps);
	uint32_t memTypeIndex = FindMemoryTypeIndex(memProps.memoryTypeCount, memProps.memoryTypes, pMemReqs->memoryTypeBits, propFlags);

#if _WIN32
	VkExportMemoryWin32HandleInfoKHR exportMemPlatformInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		.pNext = pDedicatedAllocInfo,
		.dwAccess = GENERIC_ALL,  // Doesn't seem to effect actual texture access. Only NT handle.
	};
	VkImportMemoryWin32HandleInfoKHR importMemAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		.pNext = pDedicatedAllocInfo,
		.handleType = importHandleType,
		.handle = importHandle,
	};
#endif

	VkExportMemoryAllocateInfo exportMemAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
		.pNext = &exportMemPlatformInfo,
		.handleTypes = importHandleType,
	};
	VkMemoryAllocateInfo memAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = VK_LOCALITY_INTERPROCESS_EXPORTED(locality) ? &exportMemAllocInfo : VK_LOCALITY_INTERPROCESS_IMPORTED(locality) ? &importMemAllocInfo
	                                                                                                                             : (void*)pDedicatedAllocInfo,
		.allocationSize = pMemReqs->size,
		.memoryTypeIndex = memTypeIndex,
	};
	VK_CHECK(vkAllocateMemory(vk.context.device, &memAllocInfo, VK_ALLOC, pDeviceMemory));

#ifdef VK_DEBUG_MEMORY_ALLOC
	printf("%sMemoryType: %d Allocated: %zu ", pDedicatedAllocInfo != NULL ? "Dedicated " : "", memTypeIndex, pMemReqs->size);
	PrintMemoryPropertyFlags(propFlags);
#endif
}

static void CreateAllocBuffer(VkMemoryPropertyFlags memPropFlags, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkLocality locality, VkDeviceMemory* pMemory, VkBuffer* pBuffer)
{
	VkBufferCreateInfo bufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = usage,
	};
	VK_CHECK(vkCreateBuffer(vk.context.device, &bufferCreateInfo, VK_ALLOC, pBuffer));

	VkMemoryDedicatedRequirements dedicatedReqs = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
	VkMemoryRequirements2         memReqs2 = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicatedReqs};
	vkGetBufferMemoryRequirements2(vk.context.device, &(VkBufferMemoryRequirementsInfo2){.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = *pBuffer}, &memReqs2);

	bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
	bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VK_DEBUG_MEMORY_ALLOC
	if (requiresDedicated)
		printf("Dedicated allocation is required for this buffer.\n");
	else if (prefersDedicated)
		printf("Dedicated allocation is preferred for this buffer.\n");
#endif

	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .buffer = *pBuffer};
	// for some reason dedicated and external allocations are crashing on 4090 after important in other process, so lets just leave is that as requires to be dedicated for now
	//	AllocateMemory(&memReqs2.memoryRequirements, memPropFlags, locality, NULL,
	//				   (requiresDedicated || prefersDedicated) && !MID_LOCALITY_INTERPROCESS(locality) ? &dedicatedAllocInfo : NULL,
	//				   pDeviceMem);
	AllocateMemory(&memReqs2.memoryRequirements, memPropFlags, locality, 0, NULL, requiresDedicated ? &dedicatedAllocInfo : NULL, pMemory);
}

static void CreateAllocBindBuffer(VkMemoryPropertyFlags memPropFlags, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer)
{
	CreateAllocBuffer(memPropFlags, bufferSize, usage, locality, pDeviceMem, pBuffer);
	VK_CHECK(vkBindBufferMemory(vk.context.device, *pBuffer, *pDeviceMem, 0));
}

void vkCreateAllocateBindMapBuffer(VkMemoryPropertyFlags memPropFlags, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer, void** ppMapped)
{
	CreateAllocBindBuffer(memPropFlags, bufferSize, usage, locality, pDeviceMem, pBuffer);
	VK_CHECK(vkMapMemory(vk.context.device, *pDeviceMem, 0, bufferSize, 0, ppMapped));
}

void vkCreateSharedBuffer(const VkRequestAllocationInfo* pRequest, VkSharedBuffer* pBuffer)
{
	VkBufferCreateInfo bufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = pRequest->size,
		.usage = pRequest->usage,
	};
	VK_CHECK(vkCreateBuffer(vk.context.device, &bufferCreateInfo, VK_ALLOC, &pBuffer->buffer));

	VkBufferMemoryRequirementsInfo2 bufMemReqInfo2 = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = pBuffer->buffer};
	VkMemoryDedicatedRequirements   dedicatedReqs = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, .pNext = NULL};
	VkMemoryRequirements2           memReqs2 = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicatedReqs};
	vkGetBufferMemoryRequirements2(vk.context.device, &bufMemReqInfo2, &memReqs2);

	bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
	bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VK_DEBUG_MEMORY_ALLOC
	if (requiresDedicated)
		PANIC("Trying to allocate buffer to shared memory that requires dedicated allocation!");
	else if (prefersDedicated)
		printf("Warning! Trying to allocate buffer to shared memory that prefers dedicated allocation!");
#endif

	VkPhysicalDeviceMemoryProperties2 memProps2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
	vkGetPhysicalDeviceMemoryProperties2(vk.context.physicalDevice, &memProps2);
	pBuffer->memory.type = FindMemoryTypeIndex(memProps2.memoryProperties.memoryTypeCount, memProps2.memoryProperties.memoryTypes, memReqs2.memoryRequirements.memoryTypeBits, pRequest->memoryPropertyFlags);
	pBuffer->memory.offset = requestedMemoryAllocSize[pBuffer->memory.type] + (requestedMemoryAllocSize[pBuffer->memory.type] % memReqs2.memoryRequirements.alignment);
	pBuffer->memory.size = memReqs2.memoryRequirements.size;

	requestedMemoryAllocSize[pBuffer->memory.type] += memReqs2.memoryRequirements.size;

	pBuffer->memory.state = VK_SHARED_MEMORY_STATE_REQUESTED;

#ifdef VK_DEBUG_MEMORY_ALLOC
	printf("Request Shared MemoryType: %d Allocation: %zu ", pMemory->type, memReqs2.memoryRequirements.size);
	PrintMemoryPropertyFlags(pRequest->memoryPropertyFlags);
#endif
}

static void CreateStagingBuffer(const void* srcData, VkDeviceSize bufferSize, VkDeviceMemory* pStagingMemory, VkBuffer* pStagingBuffer)
{
	void* dstData;
	CreateAllocBindBuffer(VK_MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_LOCALITY_CONTEXT, pStagingMemory, pStagingBuffer);
	VK_CHECK(vkMapMemory(vk.context.device, *pStagingMemory, 0, bufferSize, 0, &dstData));
	memcpy(dstData, srcData, bufferSize);
	vkUnmapMemory(vk.context.device, *pStagingMemory);
}
void vkUpdateBufferViaStaging(const void* srcData, VkDeviceSize dstOffset, VkDeviceSize bufferSize, VkBuffer buffer)
{
	VkBuffer       stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateStagingBuffer(srcData, bufferSize, &stagingBufferMemory, &stagingBuffer);
	VkCommandBuffer commandBuffer = vkBeginImmediateTransferCommandBuffer();
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.dstOffset = dstOffset, .size = bufferSize});
	vkEndImmediateTransferCommandBuffer(commandBuffer);
	vkFreeMemory(vk.context.device, stagingBufferMemory, VK_ALLOC);
	vkDestroyBuffer(vk.context.device, stagingBuffer, VK_ALLOC);
}
void vkCreateSharedMesh(const VkMeshCreateInfo* pCreateInfo, VkSharedMesh* pMesh)
{
	pMesh->offsets.vertexCount = pCreateInfo->vertexCount;
	uint32_t vertexBufferSize = sizeof(vert) * pMesh->offsets.vertexCount;
	pMesh->offsets.vertexOffset = 0;
	pMesh->offsets.indexCount = pCreateInfo->indexCount;
	uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->offsets.indexCount;
	pMesh->offsets.indexOffset = vertexBufferSize;

	VkRequestAllocationInfo AllocRequest = {
		.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.size = pMesh->offsets.indexOffset + indexBufferSize,
		.usage = VK_BUFFER_USAGE_MESH,
		.locality = VK_LOCALITY_CONTEXT,
		.dedicated = VK_DEDICATED_MEMORY_FALSE,
	};
	vkCreateSharedBuffer(&AllocRequest, &pMesh->sharedBuffer);
}
void vkBindUpdateSharedMesh(const VkMeshCreateInfo* pCreateInfo, VkSharedMesh* pMesh)
{
	// Ensure size is same
	VkBufferMemoryRequirementsInfo2 bufMemReqInfo2 = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = pMesh->sharedBuffer.buffer};
	VkMemoryRequirements2           memReqs2 = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetBufferMemoryRequirements2(vk.context.device, &bufMemReqInfo2, &memReqs2);
	CHECK(pMesh->sharedBuffer.memory.size != memReqs2.memoryRequirements.size, "Trying to create mesh with a requested allocated of a different size.");

	// bind populate
	VK_CHECK(vkBindBufferMemory(vk.context.device, pMesh->sharedBuffer.buffer, deviceMemory[pMesh->sharedBuffer.memory.type], pMesh->sharedBuffer.memory.offset));
	vkUpdateBufferViaStaging(pCreateInfo->pIndices, pMesh->offsets.indexOffset, sizeof(uint16_t) * pMesh->offsets.indexCount, pMesh->sharedBuffer.buffer);
	vkUpdateBufferViaStaging(pCreateInfo->pVertices, pMesh->offsets.vertexOffset, sizeof(vert) * pMesh->offsets.vertexCount, pMesh->sharedBuffer.buffer);

	pMesh->sharedBuffer.memory.state = VK_SHARED_MEMORY_STATE_BOUND;
}
void vkCreateMesh(const VkMeshCreateInfo* pCreateInfo, VkMesh* pMesh)
{
	pMesh->offsets.indexCount = pCreateInfo->indexCount;
	pMesh->offsets.vertexCount = pCreateInfo->vertexCount;
	uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->offsets.indexCount;
	uint32_t vertexBufferSize = sizeof(vert) * pMesh->offsets.vertexCount;
	pMesh->offsets.indexOffset = 0;
	pMesh->offsets.vertexOffset = indexBufferSize + (indexBufferSize % sizeof(vert));

	CreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pMesh->offsets.vertexOffset + vertexBufferSize, VK_BUFFER_USAGE_MESH, VK_LOCALITY_CONTEXT, &pMesh->mem, &pMesh->buf);
	vkUpdateBufferViaStaging(pCreateInfo->pIndices, pMesh->offsets.indexOffset, indexBufferSize, pMesh->buf);
	vkUpdateBufferViaStaging(pCreateInfo->pVertices, pMesh->offsets.vertexOffset, vertexBufferSize, pMesh->buf);
}

//----------------------------------------------------------------------------------
// Images
//----------------------------------------------------------------------------------

// could all image methods be condensed to one?
static void CreateImageView(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture)
{
	VkImageViewCreateInfo imageViewCreateInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = pTexture->image,
		.viewType = (VkImageViewType)pCreateInfo->pImageCreateInfo->imageType,
		.format = pCreateInfo->pImageCreateInfo->format,
		.subresourceRange = {
			.aspectMask = pCreateInfo->aspectMask,
			.levelCount = pCreateInfo->pImageCreateInfo->mipLevels,
			.layerCount = pCreateInfo->pImageCreateInfo->arrayLayers,
		},
	};
	VK_CHECK(vkCreateImageView(vk.context.device, &imageViewCreateInfo, VK_ALLOC, &pTexture->view));
}
static void CreateAllocImage(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture)
{
	VK_CHECK(vkCreateImage(vk.context.device, pCreateInfo->pImageCreateInfo, VK_ALLOC, &pTexture->image));

	bool requiresExternalDedicated = false;
	if (VK_LOCALITY_INTERPROCESS(pCreateInfo->locality)) {
		VkPhysicalDeviceExternalImageFormatInfo externalImageInfo = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
			.handleType = pCreateInfo->handleType,
		};
		VkPhysicalDeviceImageFormatInfo2 imageInfo = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
			.pNext  = &externalImageInfo,
			.format = pCreateInfo->pImageCreateInfo->format,
			.type   = pCreateInfo->pImageCreateInfo->imageType,
			.tiling = pCreateInfo->pImageCreateInfo->tiling,
			.usage  = pCreateInfo->pImageCreateInfo->usage,
		};
		VkExternalImageFormatProperties externalImageProperties = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
		};
		VkImageFormatProperties2 imageProperties = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
			.pNext = &externalImageProperties,
		};
		VK_CHECK(vkGetPhysicalDeviceImageFormatProperties2(vk.context.physicalDevice, &imageInfo, &imageProperties));

		LOG("Importing Texture: %s %d %d\n", string_VkFormat(pCreateInfo->pImageCreateInfo->format), pCreateInfo->pImageCreateInfo->extent.width, pCreateInfo->pImageCreateInfo->extent.height);
		LOG("  externalMemoryFeatures:\n");
		LogFlags("    ", "\n", externalImageProperties.externalMemoryProperties.externalMemoryFeatures, string_VkExternalMemoryFeatureFlagBits);
		LOG("  exportFromImportedHandleTypes:\n");
		LogFlags("    ", "\n", externalImageProperties.externalMemoryProperties.exportFromImportedHandleTypes, string_VkExternalMemoryHandleTypeFlagBits);
		LOG("  compatibleHandleTypes:\n");
		LogFlags("    ", "\n", externalImageProperties.externalMemoryProperties.compatibleHandleTypes, string_VkExternalMemoryHandleTypeFlagBits);

		requiresExternalDedicated = externalImageProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;
	}

	VkImageMemoryRequirementsInfo2 imgMemReqInfo2 = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
		.image = pTexture->image,
	};
	VkMemoryDedicatedRequirements dedicatedReqs = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
	};
	VkMemoryRequirements2 memReqs2 = {
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicatedReqs,
	};
	vkGetImageMemoryRequirements2(vk.context.device, &imgMemReqInfo2, &memReqs2);

	bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
	bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VK_DEBUG_MEMORY_ALLOC
	if (requiresDedicated)
		printf("Dedicated allocation is required for this image.\n");
	if (prefersDedicated)
		printf("Dedicated allocation is preferred for this image.\n");
	if (requiresExternalDedicated)
		printf("Dedicated external allocation is required for this image.\n");
#endif

	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.image = pTexture->image,
	};
	AllocateMemory(
		&memReqs2.memoryRequirements,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		pCreateInfo->locality,
		pCreateInfo->handleType,
		pCreateInfo->importHandle,
		requiresDedicated || requiresExternalDedicated ? &dedicatedAllocInfo : NULL,
		&pTexture->memory);
}
static void CreateAllocBindImage(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture)
{
	CreateAllocImage(pCreateInfo, pTexture);
	VK_CHECK(vkBindImageMemory(vk.context.device, pTexture->image, pTexture->memory, 0));
}
static void CreateAllocateBindImageView(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture)
{
	CreateAllocBindImage(pCreateInfo, pTexture);
	CreateImageView(pCreateInfo, pTexture);
}

void vkCreateDedicatedTexture(const VkDedicatedTextureCreateInfo* pCreateInfo, VkDedicatedTexture* pTexture)
{
	CreateAllocateBindImageView(pCreateInfo, pTexture);
	vkSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pTexture->image, pCreateInfo->debugName);
}

void vkCreateDedicatedTextureFromFile(const char* pPath, VkDedicatedTexture* pTexture)
{
	int      texChannels, width, height;
	stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
	CHECK(width == 0 || height == 0, "Image windowHeight or windowWidth is equal to zero.")

	VkDedicatedTextureCreateInfo createInfo = {
		.debugName = pPath,
		.pImageCreateInfo = &(VkImageCreateInfo){
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_B8G8R8A8_SRGB,
			.extent = (VkExtent3D){width, height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		},
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	};
	vkCreateDedicatedTexture(&createInfo, pTexture);

	VkDeviceSize   imageBufferSize = width * height * 4;
	VkBuffer       stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateStagingBuffer(pImagePixels, imageBufferSize, &stagingBufferMemory, &stagingBuffer);
	stbi_image_free(pImagePixels);

	VkCommandBuffer commandBuffer = vkBeginImmediateTransferCommandBuffer();

	VkImageMemoryBarrier2 beginCopyBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = pTexture->image,
		.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		VK_IMAGE_BARRIER_SRC_UNDEFINED,
		VK_IMAGE_BARRIER_DST_TRANSFER_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
	};
	vkCmdImageBarriers(commandBuffer, 1, &beginCopyBarrier);

	VkBufferImageCopy region = {
		.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
		.imageExtent = {width, height, 1},
	};
	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageMemoryBarrier2 endCopyBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = pTexture->image,
		.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		VK_IMAGE_BARRIER_SRC_TRANSFER_WRITE,
		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = VK_ACCESS_2_NONE,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
	};
	vkCmdImageBarriers(commandBuffer, 1, &endCopyBarrier);

	vkEndImmediateTransferCommandBuffer(commandBuffer);

	vkFreeMemory(vk.context.device, stagingBufferMemory, VK_ALLOC);
	vkDestroyBuffer(vk.context.device, stagingBuffer, VK_ALLOC);
}

void vkDestroyDedicatedTexture(VkDedicatedTexture* pTexture)
{
	CHECK_NOT_EQUAL(pTexture, NULL);
	CHECK_NOT_EQUAL(pTexture->view, NULL);
	CHECK_NOT_EQUAL(pTexture->image, NULL);
	CHECK_NOT_EQUAL(pTexture->memory, NULL);
	vkDestroyImageView(vk.context.device, pTexture->view, VK_ALLOC);
	vkDestroyImage(vk.context.device, pTexture->image, VK_ALLOC);
	vkFreeMemory(vk.context.device, pTexture->memory, VK_ALLOC);
	pTexture->view = NULL;
	pTexture->image = NULL;
	pTexture->memory = NULL;
}


static struct {
	IDXGIFactory4* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device*  device;
} d3d12;
static DXGI_FORMAT vkDXGIFormat(VkFormat format)
{
	switch (format) {
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:  return DXGI_FORMAT_R8_TYPELESS;

		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT: return DXGI_FORMAT_R16_TYPELESS;

		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:  return DXGI_FORMAT_R8G8_TYPELESS;

		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT: return DXGI_FORMAT_R32_TYPELESS;

		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT: return DXGI_FORMAT_R16G16_TYPELESS;

		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:  return DXGI_FORMAT_R8G8B8A8_TYPELESS;

		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:  return DXGI_FORMAT_B8G8R8A8_TYPELESS;

		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT: return DXGI_FORMAT_R32G32_TYPELESS;

		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT: return DXGI_FORMAT_R32G32B32_TYPELESS;

		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_TYPELESS;

		case VK_FORMAT_D16_UNORM:          return DXGI_FORMAT_R16_TYPELESS;
		case VK_FORMAT_D32_SFLOAT:         return DXGI_FORMAT_R32_TYPELESS;
		case VK_FORMAT_D24_UNORM_S8_UINT:  return DXGI_FORMAT_R24G8_TYPELESS;
		case VK_FORMAT_D32_SFLOAT_S8_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;

		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:  return DXGI_FORMAT_BC1_TYPELESS;

		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC2_SRGB_BLOCK:  return DXGI_FORMAT_BC2_TYPELESS;

		case VK_FORMAT_BC3_UNORM_BLOCK:
		case VK_FORMAT_BC3_SRGB_BLOCK:  return DXGI_FORMAT_BC3_TYPELESS;

		case VK_FORMAT_BC4_UNORM_BLOCK:
		case VK_FORMAT_BC4_SNORM_BLOCK: return DXGI_FORMAT_BC4_TYPELESS;

		case VK_FORMAT_BC5_UNORM_BLOCK:
		case VK_FORMAT_BC5_SNORM_BLOCK: return DXGI_FORMAT_BC5_TYPELESS;

		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK: return DXGI_FORMAT_BC6H_TYPELESS;

		case VK_FORMAT_BC7_UNORM_BLOCK:
		case VK_FORMAT_BC7_SRGB_BLOCK:           return DXGI_FORMAT_BC7_TYPELESS;
		case VK_FORMAT_B5G6R5_UNORM_PACK16:      return DXGI_FORMAT_B5G6R5_UNORM;
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:    return DXGI_FORMAT_B5G5R5A1_UNORM;
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:    return DXGI_FORMAT_B4G4R4A4_UNORM;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return DXGI_FORMAT_R10G10B10A2_TYPELESS;

		default: return DXGI_FORMAT_UNKNOWN;
	}
}
static bool vkDepthFormat(VkFormat format)
{
	switch (format) {
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default: return false;
	}
}

static void CheckDXGI()
{
	if (d3d12.adapter == NULL) {
		UINT flags = DXGI_CREATE_FACTORY_DEBUG;
		DX_CHECK(CreateDXGIFactory2(flags, &IID_IDXGIFactory4, (void**)&d3d12.factory));
		for (UINT i = 0; IDXGIFactory4_EnumAdapters1(d3d12.factory, i, &d3d12.adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 desc;
			DX_CHECK(IDXGIAdapter1_GetDesc1(d3d12.adapter, &desc));
			wprintf(L"DXGI Adapter %d Name: %ls Description: %ld:%lu\n",
				i, desc.Description, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart);
			break;  // add logic to choose?
		}

		DX_CHECK(D3D12CreateDevice((IUnknown*)d3d12.adapter, D3D_FEATURE_LEVEL_11_1, &IID_ID3D12Device, (void**)&d3d12.device));
	}
}

void vkCreateExternalPlatformTexture(const VkImageCreateInfo* pCreateInfo, VkExternalPlatformTexture* pTexture)
{
	CHECK_EQUAL(vkDepthFormat(pCreateInfo->format), false, "ExternalPlatformTexture cannot be depth!");
	CHECK_EQUAL(pCreateInfo->mipLevels, 1, "ExternalPlatformTexture cannot have mips!");
	CheckDXGI();

	auto format = vkDXGIFormat(pCreateInfo->format);
	D3D12_RESOURCE_DESC textureDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = pCreateInfo->extent.width,
		.Height = pCreateInfo->extent.height,
		.DepthOrArraySize = pCreateInfo->arrayLayers,
		.MipLevels = pCreateInfo->mipLevels,
		.Format = format,
		.SampleDesc.Count = pCreateInfo->samples,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS,
	};
	D3D12_HEAP_PROPERTIES heapProperties = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1,
		.VisibleNodeMask = 1,
	};
	DX_CHECK(ID3D12Device_CreateCommittedResource(
		d3d12.device,
		&heapProperties,
		D3D12_HEAP_FLAG_SHARED,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		NULL,
		&IID_ID3D12Resource,
		(void**)&pTexture->texture));
	LOG("Created DX12 Texture: %s %d %d\n", string_VkFormat(pCreateInfo->format), pCreateInfo->extent.width, pCreateInfo->extent.height);

	DX_CHECK(ID3D12Device_CreateSharedHandle(
		d3d12.device,
		(ID3D12DeviceChild*)pTexture->texture,
		NULL,
		GENERIC_ALL,
		NULL,
		&pTexture->handle));

	// I think I only need to keep the device alive?
	// I should have explicit ExternalPlatform Init and Destroy methods
	//	IDXGIFactory4_Release(d3d12.factory);
	//	IDXGIAdapter1_Release(d3d12.adapter);
	//	ID3D12Device_Release(d3d12.device);
}

void vkDestroyExternalPlatformTexture(VkExternalPlatformTexture* pTexture)
{
	CHECK_NOT_EQUAL(pTexture, NULL)
	CHECK_NOT_EQUAL(pTexture->texture, NULL);
	CHECK_NOT_EQUAL(pTexture->handle, NULL);
	CHECK_WIN32(CloseHandle(pTexture->handle));
	DX_CHECK(ID3D12Resource_Release(pTexture->texture));
	pTexture->texture = NULL;
	pTexture->handle = NULL;
}

void vkCreateDepthFramebuffer(const VkDepthFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
{
	VkFramebufferCreateInfo framebufferCreateInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		&(VkFramebufferAttachmentsCreateInfo){
			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
			.attachmentImageInfoCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
			.pAttachmentImageInfos    = (VkFramebufferAttachmentImageInfo[]){
				[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] = {
					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
					.usage           = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
					.width           = pCreateInfo->width,
					.height          = pCreateInfo->height,
					.layerCount      = pCreateInfo->layerCount,
					.viewFormatCount = 1,
					.pViewFormats    = &VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
				},
				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH] = {
					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
					.usage           = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH],
					.width           = pCreateInfo->width,
					.height          = pCreateInfo->height,
					.layerCount      = pCreateInfo->layerCount,
					.viewFormatCount = 1,
					.pViewFormats    = &VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH],
				},
			},
		},
		.flags           = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
		.renderPass      = vk.context.depthRenderPass,
		.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_COUNT,
		.width           = pCreateInfo->width,
		.height          = pCreateInfo->height,
		.layers          = pCreateInfo->layerCount,
	};
	VK_CHECK(vkCreateFramebuffer(vk.context.device, &framebufferCreateInfo, VK_ALLOC, pFramebuffer));
}

//void vkCreateDepthNormalFramebuffer(const VkDepthNormalFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
//{
//	VkFramebufferCreateInfo framebufferCreateInfo = {
//		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
//		&(VkFramebufferAttachmentsCreateInfo){
//			VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
//			.attachmentImageInfoCount = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COUNT,
//			.pAttachmentImageInfos    = (VkFramebufferAttachmentImageInfo[]){
//				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR] = {
//					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
//					.usage           = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR],
//					.width           = pCreateInfo->width,
//					.height          = pCreateInfo->height,
//					.layerCount      = pCreateInfo->layerCount,
//					.viewFormatCount = 1,
//					.pViewFormats    = &VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR],
//				},
//				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL] = {
//					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
//					.usage           = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL],
//					.width           = pCreateInfo->width,
//					.height          = pCreateInfo->height,
//					.layerCount      = pCreateInfo->layerCount,
//					.viewFormatCount = 1,
//					.pViewFormats    = &VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL],
//				},
//				[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH] = {
//					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
//					.usage           = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH],
//					.width           = pCreateInfo->width,
//					.height          = pCreateInfo->height,
//					.layerCount      = pCreateInfo->layerCount,
//					.viewFormatCount = 1,
//					.pViewFormats    = &VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH],
//				},
//			},
//		},
//		.flags           = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
//		.renderPass      = vk.context.basicPass,
//		.attachmentCount = VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COUNT,
//		.width           = pCreateInfo->width,
//		.height          = pCreateInfo->height,
//		.layers          = pCreateInfo->layerCount,
//	};
//	VK_CHECK(vkCreateFramebuffer(vk.context.device, &framebufferCreateInfo, VK_ALLOC, pFramebuffer));
//}

void vkCreateDepthFramebufferTextures(const VkDepthFramebufferTextureCreateInfo* pCreateInfo, VkDepthFramebufferTexture* pFrameBuffer)
{
	VkDedicatedTextureCreateInfo colorCreateInfo = {
		.debugName = "ColorFramebuffer",
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.locality = pCreateInfo->locality,
		.pImageCreateInfo = &(VkImageCreateInfo){
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM : NULL,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
			.extent = pCreateInfo->extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
		},
	};
	vkCreateDedicatedTexture(&colorCreateInfo, &pFrameBuffer->color);
	VkDedicatedTextureCreateInfo depthCreateInfo = {
		.debugName = "DepthFramebuffer",
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
		.locality = pCreateInfo->locality,
		.pImageCreateInfo = &(VkImageCreateInfo){
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM : NULL,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH],
			.extent = pCreateInfo->extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTH],
		},
	};
	vkCreateDedicatedTexture(&depthCreateInfo, &pFrameBuffer->depth);
}

//void vkCreateDepthNormalFramebufferTextures(const VkDepthNormalFramebufferTextureCreateInfo* pCreateInfo, VkBasicFramebufferTexture* pFrameBuffer)
//{
//	VkDedicatedTextureCreateInfo colorCreateInfo = {
//		.debugName = "ColorFramebuffer",
//		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//		.locality = pCreateInfo->locality,
//		.pImageCreateInfo = &(VkImageCreateInfo){
//			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//			.pNext = VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM : NULL,
//			.imageType = VK_IMAGE_TYPE_2D,
//			.format = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR],
//			.extent = pCreateInfo->extent,
//			.mipLevels = 1,
//			.arrayLayers = 1,
//			.samples = VK_SAMPLE_COUNT_1_BIT,
//			.usage = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_COLOR],
//		},
//	};
//	vkCreateDedicatedTexture(&colorCreateInfo, &pFrameBuffer->color);
//	VkDedicatedTextureCreateInfo normalCreateInfo = {
//		.debugName = "NormalFramebuffer",
//		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//		.locality = pCreateInfo->locality,
//		.pImageCreateInfo = &(VkImageCreateInfo){
//			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//			.pNext = VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM : NULL,
//			.imageType = VK_IMAGE_TYPE_2D,
//			.format = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL],
//			.extent = pCreateInfo->extent,
//			.mipLevels = 1,
//			.arrayLayers = 1,
//			.samples = VK_SAMPLE_COUNT_1_BIT,
//			.usage = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_NORMAL],
//		},
//	};
//	vkCreateDedicatedTexture(&normalCreateInfo, &pFrameBuffer->normal);
//	VkDedicatedTextureCreateInfo depthCreateInfo = {
//		.debugName = "DepthFramebuffer",
//		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
//		.locality = pCreateInfo->locality,
//		.pImageCreateInfo = &(VkImageCreateInfo){
//			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//			.pNext = VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM : NULL,
//			.imageType = VK_IMAGE_TYPE_2D,
//			.format = VK_RENDER_PASS_DEPTHNORMAL_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH],
//			.extent = pCreateInfo->extent,
//			.mipLevels = 1,
//			.arrayLayers = 1,
//			.samples = VK_SAMPLE_COUNT_1_BIT,
//			.usage = VK_RENDER_PASS_DEPTHNORMAL_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_DEPTHNORMAL_DEPTH],
//		},
//	};
//	vkCreateDedicatedTexture(&depthCreateInfo, &pFrameBuffer->depth);
//}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------

static uint32_t FindQueueIndex(VkPhysicalDevice physicalDevice, const VkmQueueFamilyCreateInfo* pQueueDesc)
{
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

	for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex) {

		bool graphicsSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
		bool computeSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
		bool transferSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;

		if (pQueueDesc->supportsGraphics == VKM_SUPPORT_YES && !graphicsSupport) continue;
		if (pQueueDesc->supportsGraphics == VKM_SUPPORT_NO && graphicsSupport) continue;
		if (pQueueDesc->supportsCompute == VKM_SUPPORT_YES && !computeSupport) continue;
		if (pQueueDesc->supportsCompute == VKM_SUPPORT_NO && computeSupport) continue;
		if (pQueueDesc->supportsTransfer == VKM_SUPPORT_YES && !transferSupport) continue;
		if (pQueueDesc->supportsTransfer == VKM_SUPPORT_NO && transferSupport) continue;

		if (pQueueDesc->globalPriority != 0) {
			bool globalPrioritySupported = false;
			for (u32 i = 0; i < queueFamilyGlobalPriorityProperties[queueFamilyIndex].priorityCount; ++i) {
				if (queueFamilyGlobalPriorityProperties[queueFamilyIndex].priorities[i] == pQueueDesc->globalPriority) {
					globalPrioritySupported = true;
					break;
				}
			}

			if (!globalPrioritySupported)
				continue;
		}

		return queueFamilyIndex;
	}

	fprintf(stderr, "Can't find queue family: graphics=%s compute=%s transfer=%s globalPriority=%s... ",
		string_Support[pQueueDesc->supportsGraphics],
		string_Support[pQueueDesc->supportsCompute],
		string_Support[pQueueDesc->supportsTransfer],
		string_VkQueueGlobalPriority(pQueueDesc->globalPriority));
	PANIC("Can't find queue family");
	return -1;
}

void vkInitializeInstance()
{
	{
		//		const char* ppEnabledLayerNames[] = {
		//			//        "VK_LAYER_KHRONOS_validation",
		//			//        "VK_LAYER_LUNARG_monitor"
		//			//        "VK_LAYER_RENDERDOC_Capture",
		//		};
		const char* ppEnabledInstanceExtensionNames[] = {
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_PLATFORM_SURFACE_EXTENSION_NAME,
		};
		VkInstanceCreateInfo instanceCreationInfo = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &(VkApplicationInfo){
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = "Moxaic",
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "Vulkan",
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_VERSION,
			},
			//			.enabledLayerCount = COUNT(ppEnabledLayerNames),
			//			.ppEnabledLayerNames = ppEnabledLayerNames,
			.enabledExtensionCount = COUNT(ppEnabledInstanceExtensionNames),
			.ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
		};
		VK_CHECK(vkCreateInstance(&instanceCreationInfo, VK_ALLOC, &vk.instance));
		printf("instance Vulkan API version: %d.%d.%d.%d\n",
			VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
			VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
			VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
			VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
	}

	{   // do I want to do this or rely on vulkan configurator?
		//        VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = {};
		//        // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		//        // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		//        messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		//        messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//        VkDebugUtilsMessageTypeFlagsEXT messageType = {};
		//        // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
		//        messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		//        messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		//        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
		//            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		//            .messageSeverity = messageSeverity,
		//            .messageType = messageType,
		//            .pfnUserCallback = VkmDebugUtilsCallback,
		//        };
		//        VKM_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
		//        VKM_REQUIRE(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, VKM_ALLOC, &debugUtilsMessenger));
	}
}

void vkCreateContext(const VkContextCreateInfo* pContextCreateInfo)
{
	/// PhysicalDevice
	{
		u32 deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &deviceCount, NULL));
		VkPhysicalDevice devices[deviceCount];
		VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices));

		vk.context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
		VkPhysicalDeviceProperties2 physicalDeviceProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = NULL};
		vkGetPhysicalDeviceProperties2(vk.context.physicalDevice, &physicalDeviceProperties);
		printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
		printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
			VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
			VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
			VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
			VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));

		printf("minUniformBufferOffsetAlignment: %llu\n", physicalDeviceProperties.properties.limits.minUniformBufferOffsetAlignment);
		printf("minStorageBufferOffsetAlignment: %llu\n", physicalDeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
		CHECK(physicalDeviceProperties.properties.apiVersion < VK_VERSION, "Insufficient Vulkan API Version");

		VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT physicalDeviceSwapchainMaintenance1Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
			.pNext = NULL,
		};
		VkPhysicalDeviceLineRasterizationFeaturesEXT physicalDeviceLineRasterizationFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,
			.pNext = &physicalDeviceSwapchainMaintenance1Features,
		};
		VkPhysicalDevicePipelineRobustnessFeaturesEXT physicalDevicePipelineRobustnessFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT,
			.pNext = &physicalDeviceLineRasterizationFeatures,
		};
		VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT physicalDevicePageableDeviceLocalMemoryFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
			.pNext = &physicalDevicePipelineRobustnessFeatures,
		};
		VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
			.pNext = &physicalDevicePageableDeviceLocalMemoryFeatures,
		};
		VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
			.pNext = &physicalDeviceGlobalPriorityQueryFeatures,
		};
		VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			.pNext = &physicalDeviceRobustness2Features,
		};
		VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &physicalDeviceMeshShaderFeatures,
		};
		VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &physicalDeviceVulkan13Features,
		};
		VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &physicalDeviceVulkan12Features,
		};
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &physicalDeviceVulkan11Features,
		};
		vkGetPhysicalDeviceFeatures2(vk.context.physicalDevice, &physicalDeviceFeatures);

#define CHECK_AVAILABLE(feature) CHECK(feature == VK_FALSE, #feature " unavailable!");

		//		CHECK_AVAILABLE(physicalDeviceSwapchainMaintenance1Features.swapchainMaintenance1);

		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.rectangularLines);
		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.bresenhamLines);
		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.smoothLines);
		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.stippledRectangularLines);
		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.stippledBresenhamLines);
		CHECK_AVAILABLE(physicalDeviceLineRasterizationFeatures.stippledSmoothLines);

		CHECK_AVAILABLE(physicalDeviceRobustness2Features.nullDescriptor);
		CHECK_AVAILABLE(physicalDeviceVulkan13Features.synchronization2);
		CHECK_AVAILABLE(physicalDeviceVulkan12Features.timelineSemaphore);

#undef CHECK_AVAILABLE
	}

	/// Device
	{
		// Features
		VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT physicalDeviceSwapchainMaintenance1Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
			.swapchainMaintenance1 = VK_FALSE,
		};
		VkPhysicalDeviceLineRasterizationFeaturesEXT physicalDeviceLineRasterizationFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,
			&physicalDeviceSwapchainMaintenance1Features,
			.rectangularLines = VK_TRUE,
			.bresenhamLines = VK_TRUE,
			.smoothLines = VK_TRUE,
			.stippledRectangularLines = VK_TRUE,
			.stippledBresenhamLines = VK_TRUE,
			.stippledSmoothLines = VK_TRUE,
		};
		VkPhysicalDevicePipelineRobustnessFeaturesEXT physicalDevicePipelineRobustnessFeaturesEXT = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT,
			&physicalDeviceLineRasterizationFeatures,
			.pipelineRobustness = VK_TRUE,
		};
		VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT physicalDevicePageableDeviceLocalMemoryFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
			&physicalDevicePipelineRobustnessFeaturesEXT,
			.pageableDeviceLocalMemory = VK_TRUE,
		};
		VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
			&physicalDevicePageableDeviceLocalMemoryFeatures,
			.globalPriorityQuery = VK_TRUE,
		};
		VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
			&physicalDeviceGlobalPriorityQueryFeatures,
			.robustBufferAccess2 = VK_TRUE,
			.robustImageAccess2 = VK_TRUE,
			.nullDescriptor = VK_TRUE,
		};
		VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			&physicalDeviceRobustness2Features,
			.taskShader = VK_TRUE,
			.meshShader = VK_TRUE,
		};
		VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			&physicalDeviceMeshShaderFeatures,
			.synchronization2 = VK_TRUE,
		};
		VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			&physicalDeviceVulkan13Features,
			.samplerFilterMinmax = VK_TRUE,
			.hostQueryReset = VK_TRUE,
			.imagelessFramebuffer = VK_TRUE,
			.timelineSemaphore = VK_TRUE,
		};
		VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			&physicalDeviceVulkan12Features,
			.storageBuffer16BitAccess = VK_FALSE,
		};
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			&physicalDeviceVulkan11Features,
			.features = {
				.tessellationShader = VK_TRUE,
				.robustBufferAccess = VK_TRUE,
				.shaderImageGatherExtended = VK_TRUE,
#ifdef VKM_DEBUG_WIREFRAME
				.fillModeNonSolid = VK_TRUE,
#endif
			},
		};

		// Queues
		uint32_t activeQueueIndex = 0;
		uint32_t activeQueueCount = 0;
		for (int i = 0; i < VK_QUEUE_FAMILY_TYPE_COUNT; ++i)
			activeQueueCount += pContextCreateInfo->queueFamilyCreateInfos[i].queueCount > 0;

		VkDeviceQueueCreateInfo                  activeQueueCreateInfos[activeQueueCount];
		VkDeviceQueueGlobalPriorityCreateInfoEXT activeQueueGlobalPriorityCreateInfos[activeQueueCount];
		for (int queueFamilyTypeIndex = 0; queueFamilyTypeIndex < VK_QUEUE_FAMILY_TYPE_COUNT; ++queueFamilyTypeIndex) {
			if (pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].queueCount == 0)
				continue;

			vk.context.queueFamilies[queueFamilyTypeIndex].index = FindQueueIndex(vk.context.physicalDevice, &pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex]);

			activeQueueGlobalPriorityCreateInfos[activeQueueIndex] = (VkDeviceQueueGlobalPriorityCreateInfoEXT){
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
				.globalPriority = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].globalPriority,
			};
			activeQueueCreateInfos[activeQueueIndex] = (VkDeviceQueueCreateInfo){
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].globalPriority != 0 ? &activeQueueGlobalPriorityCreateInfos[activeQueueIndex] : NULL,
				.queueFamilyIndex = vk.context.queueFamilies[queueFamilyTypeIndex].index,
				.queueCount = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].queueCount,
				.pQueuePriorities = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].pQueuePriorities,
			};

			activeQueueIndex++;
		}

		// Extensions
		const char* ppEnabledDeviceExtensionNames[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
			VK_EXT_MESH_SHADER_EXTENSION_NAME,
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
			VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME,
			VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME,
			VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME,
			VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
			VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
			VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
			VK_EXTERNAL_MEMORY_EXTENSION_NAME,
			VK_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
			VK_EXTERNAL_FENCE_EXTENSION_NAME,
			VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
			VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME,
			VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
			VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
			VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
		};
		VkDeviceCreateInfo deviceCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			&physicalDeviceFeatures,
			.queueCreateInfoCount = activeQueueCount,
			.pQueueCreateInfos = activeQueueCreateInfos,
			.enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
			.ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
		};
		VK_CHECK(vkCreateDevice(vk.context.physicalDevice, &deviceCreateInfo, VK_ALLOC, &vk.context.device));
	}

	for (int i = 0; i < VK_QUEUE_FAMILY_TYPE_COUNT; ++i) {
		if (pContextCreateInfo->queueFamilyCreateInfos[i].queueCount == 0)
			continue;

		vkGetDeviceQueue(vk.context.device, vk.context.queueFamilies[i].index, 0, &vk.context.queueFamilies[i].queue);
		vkSetDebugName(VK_OBJECT_TYPE_QUEUE, (uint64_t)vk.context.queueFamilies[i].queue, string_QueueFamilyType[i]);
		VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = vk.context.queueFamilies[i].index,
		};
		VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &vk.context.queueFamilies[i].pool));
	}

	{
		// do I want to switch to this? probably
#define PFN_FUNC(_func)                                                              \
	vk._func = (PFN_##vk##_func)vkGetDeviceProcAddr(vk.context.device, "vk" #_func); \
	CHECK(vk._func == NULL, "Couldn't load " #_func)
		PFN_FUNCS
#undef PFN_FUNC
	}
}

void vkCreateSwapContext(VkSurfaceKHR surface, VkQueueFamilyType presentQueueFamily, VkSwapContext* pSwap)
{
	VkBool32 presentSupport = VK_FALSE;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk.context.physicalDevice, vk.context.queueFamilies[presentQueueFamily].index, surface, &presentSupport));
	CHECK(presentSupport == VK_FALSE, "Queue can't present to surface!")

	VkSwapchainCreateInfoKHR info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = VK_SWAP_COUNT,
		.imageFormat = VK_SWAP_FORMAT,
		.imageExtent = {DEFAULT_WIDTH, DEFAULT_HEIGHT},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_TRUE,
	};
	VK_CHECK(vkCreateSwapchainKHR(vk.context.device, &info, VK_ALLOC, &pSwap->chain));

	uint32_t swapCount;
	VK_CHECK(vkGetSwapchainImagesKHR(vk.context.device, pSwap->chain, &swapCount, NULL));
	CHECK(swapCount != VK_SWAP_COUNT, "Resulting viewSwaps image count does not match requested viewSwaps count!");
	VkImage images[VK_SWAP_COUNT];
	VK_CHECK(vkGetSwapchainImagesKHR(vk.context.device, pSwap->chain, &swapCount, images));

	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		pSwap->frames[i].image = images[i];
		vkSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)images[i], "SwapImage");

		VkImageViewCreateInfo viewCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = pSwap->frames[i].image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_SWAP_FORMAT,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		};
		VK_CHECK(vkCreateImageView(vk.context.device, &viewCreateInfo, VK_ALLOC, &pSwap->frames[i].view));
		vkSetDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)pSwap->frames[i].view, "SwapImageView");

		VkSemaphoreCreateInfo acquireSwapSemaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = 0};
		VK_CHECK(vkCreateSemaphore(vk.context.device, &acquireSwapSemaphoreCreateInfo, VK_ALLOC, &pSwap->frames[i].completeSemaphore));
		vkSetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)pSwap->frames[i].completeSemaphore, "SwapCompleteSemaphore");

		VK_CHECK(vkCreateSemaphore(vk.context.device, &acquireSwapSemaphoreCreateInfo, VK_ALLOC, &pSwap->acquireSemaphores[i]));
		vkSetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)pSwap->acquireSemaphores[i], "SwapAcquireSemaphore");
	}
}

void vkCreateExternalFence(const VkExternalFenceCreateInfo* pCreateInfo, VkFence* pFence)
{
#if _WIN32
	VkExportFenceWin32HandleInfoKHR exportPlatformInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR,
		// TODO are these the best security options? Read seems to affect it and solves issue of child corrupting semaphore on crash... but not 100%
		.dwAccess = pCreateInfo->locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY ? GENERIC_READ : GENERIC_ALL,
	};
#endif
	VkExportFenceCreateInfo exportInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
		.pNext = &exportPlatformInfo,
		.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_PLATFORM,
	};
	VkFenceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? &exportInfo : NULL,
	};
	VK_CHECK(vkCreateFence(vk.context.device, &info, VK_ALLOC, pFence));
	vkSetDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)*pFence, pCreateInfo->debugName);
	switch (pCreateInfo->locality) {
		default:                                          break;
		case VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE:
		case VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY:  {
#if _WIN32
			VkImportFenceWin32HandleInfoKHR importWin32HandleInfo = {
				.sType = VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR,
				.fence = *pFence,
				.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_PLATFORM,
				.handle = pCreateInfo->importHandle,
			};
			VK_INSTANCE_FUNC(ImportFenceWin32HandleKHR);
			VK_CHECK(ImportFenceWin32HandleKHR(vk.context.device, &importWin32HandleInfo));
#endif
			break;
		}
	}
}

// I need a word that represents a more comprehensive Create more function
void vkCreateSemaphoreExt(const vkSemaphoreCreateInfoExt* pCreateInfo, VkSemaphore* pSemaphore)
{
#if _WIN32
	VkExportSemaphoreWin32HandleInfoKHR exportPlatformInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
		// TODO are these the best security options? Read seems to affect it and solves issue of child corrupting semaphore on crash... but not 100%
		.dwAccess = pCreateInfo->locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY ? GENERIC_READ : GENERIC_ALL,
	};
#endif
	VkExportSemaphoreCreateInfo exportInfo = {
		.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
		.pNext = &exportPlatformInfo,
		.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_PLATFORM,
	};
	VkSemaphoreTypeCreateInfo typeInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = VK_LOCALITY_INTERPROCESS(pCreateInfo->locality) ? &exportInfo : NULL,
		.semaphoreType = pCreateInfo->semaphoreType,
	};
	VkSemaphoreCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &typeInfo,
	};
	VK_CHECK(vkCreateSemaphore(vk.context.device, &info, VK_ALLOC, pSemaphore));
	vkSetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)*pSemaphore, pCreateInfo->debugName);
	switch (pCreateInfo->locality) {
		default:                                          break;
		case VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE:
		case VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY:  {
#if _WIN32
			VkImportSemaphoreWin32HandleInfoKHR importWin32HandleInfo = {
				.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
				.semaphore = *pSemaphore,
				.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_PLATFORM,
				.handle = pCreateInfo->importHandle,
			};
			VK_INSTANCE_FUNC(ImportSemaphoreWin32HandleKHR);
			VK_CHECK(ImportSemaphoreWin32HandleKHR(vk.context.device, &importWin32HandleInfo));
#endif
			break;
		}
	}
}

//void vkCreateGlobalSet(VkGlobalSet* pSet)
//{
//	vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.globalSetLayout, &pSet->set);
//	vkCreateAllocateBindMapBuffer(VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_LOCALITY_CONTEXT, &pSet->memory, &pSet->buffer, (void**)&pSet->pMapped);
//	vkUpdateDescriptorSets(vk.context.device, 1, &VK_SET_WRITE_GLOBAL_BUFFER(pSet->set, pSet->buffer), 0, NULL);
//}

void vkSetDebugName(VkObjectType objectType, u64 objectHandle, const char* pDebugName)
{
	VkDebugUtilsObjectNameInfoEXT debugInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = objectType,
		.objectHandle = objectHandle,
		.pObjectName = pDebugName,
	};
	VK_INSTANCE_FUNC(SetDebugUtilsObjectNameEXT);
	VK_CHECK(SetDebugUtilsObjectNameEXT(vk.context.device, &debugInfo));
}

VK_EXTERNAL_HANDLE_PLATFORM vkGetMemoryExternalHandle(VkDeviceMemory memory)
{
	VK_INSTANCE_FUNC(GetMemoryWin32HandleKHR);
	VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
		.memory = memory,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM,
	};
	HANDLE handle;
	VK_CHECK(GetMemoryWin32HandleKHR(vk.context.device, &getWin32HandleInfo, &handle));
	return handle;
}
VK_EXTERNAL_HANDLE_PLATFORM vkGetFenceExternalHandle(VkFence fence)
{
	VK_INSTANCE_FUNC(GetFenceWin32HandleKHR);
	VkFenceGetWin32HandleInfoKHR getWin32HandleInfo = {
		VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
		.fence = fence,
		.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_PLATFORM,
	};
	HANDLE handle;
	VK_CHECK(GetFenceWin32HandleKHR(vk.context.device, &getWin32HandleInfo, &handle));
	return handle;
}
VK_EXTERNAL_HANDLE_PLATFORM vkGetSemaphoreExternalHandle(VkSemaphore semaphore)
{
	VK_INSTANCE_FUNC(GetSemaphoreWin32HandleKHR);
	VkSemaphoreGetWin32HandleInfoKHR getWin32HandleInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
		.semaphore = semaphore,
		.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_PLATFORM,
	};
	HANDLE handle;
	VK_CHECK(GetSemaphoreWin32HandleKHR(vk.context.device, &getWin32HandleInfo, &handle));
	return handle;
}

#ifdef _WIN32
void vkCreateVulkanSurface(HINSTANCE hInstance, HWND hWnd, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
	VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
		VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = hInstance,
		.hwnd = hWnd,
	};
	VK_INSTANCE_FUNC(CreateWin32SurfaceKHR);
	VK_CHECK(CreateWin32SurfaceKHR(vk.instance, &win32SurfaceCreateInfo, pAllocator, pSurface));

	VkSurfaceCapabilitiesKHR capabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.context.physicalDevice, *pSurface, &capabilities));

	LOG("Created Win32 Surface:\n");
	LOG("  minImageCount: %d\n", capabilities.minImageCount);
	LOG("  maxImageCount: %d\n", capabilities.maxImageCount);
	LOG("  currentExtent: %d %d\n", capabilities.currentExtent.width, capabilities.currentExtent.height);
	LOG("  minImageExtent: %d %d\n", capabilities.minImageExtent.width, capabilities.minImageExtent.height);
	LOG("  maxImageExtent: %d %d\n", capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
	LOG("  maxImageArrayLayers: %d\n", capabilities.maxImageArrayLayers);
	LOG("  supportedTransforms:\n");
	LogFlags("    ", "\n", capabilities.currentTransform, string_VkSurfaceTransformFlagBitsKHR);
	LOG("  currentTransform: %s\n", string_VkSurfaceTransformFlagBitsKHR(capabilities.currentTransform));
	LOG("  supportedTransforms:\n");
	LogFlags("    ", "\n", capabilities.supportedCompositeAlpha, string_VkCompositeAlphaFlagBitsKHR);
	LOG("  supportedTransforms:\n");
	LogFlags("    ", "\n", capabilities.supportedUsageFlags, string_VkImageUsageFlagBits);
}
#endif

#endif