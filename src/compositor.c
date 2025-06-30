#include "compositor.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <stdatomic.h>
#include <assert.h>
#include <float.h>
#include <vulkan/vk_enum_string_helper.h>

//////////////
//// Constants
////

#define GRID_QUAD_SQUARE_SIZE         2
#define GRID_QUAD_COUNT               4
#define GRID_SUBGROUP_SQUARE_SIZE     4
#define GRID_SUBGROUP_COUNT           16
#define GRID_WORKGROUP_SQUARE_SIZE    8
#define GRID_WORKGROUP_SUBGROUP_COUNT 64

//////////
//// Pipes
////

//// Node Pipe

enum {
	SET_BIND_INDEX_NODE_STATE,
	SET_BIND_INDEX_NODE_COLOR,
	SET_BIND_INDEX_NODE_GBUFFER,
	SET_BIND_INDEX_NODE_COUNT,
};

constexpr VkShaderStageFlags COMPOSITOR_MODE_STAGE_FLAGS[] = {
	[MXC_COMPOSITOR_MODE_QUAD]        = VK_SHADER_STAGE_VERTEX_BIT |
	                                    VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_SHADER_STAGE_VERTEX_BIT |
	                                    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
	                                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
	                                    VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TASK_MESH]   = VK_SHADER_STAGE_TASK_BIT_EXT |
	                                    VK_SHADER_STAGE_MESH_BIT_EXT |
	                                    VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_COMPUTE]     = VK_SHADER_STAGE_COMPUTE_BIT,
};

static void CreateNodeSetLayout(MxcCompositorMode* pModes, VkDescriptorSetLayout* pLayout)
{
	VkShaderStageFlags stageFlags = 0;
	for (int i = 0; i < MXC_COMPOSITOR_MODE_COUNT; ++i)
		if (pModes[i]) stageFlags |= COMPOSITOR_MODE_STAGE_FLAGS[i];

	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = SET_BIND_INDEX_NODE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_NODE_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
			},
			{
				SET_BIND_INDEX_NODE_COLOR,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
			},
			{
				SET_BIND_INDEX_NODE_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
	vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (u64)*pLayout, "NodeSetLayout");
}
#define BIND_WRITE_NODE_STATE(_set, _buf)                    \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstSet = _set,                                      \
		.dstBinding = SET_BIND_INDEX_NODE_STATE,             \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = _buf,                                  \
			.range = sizeof(MxcNodeCompositorSetState),      \
		},                                                   \
	}
#define BIND_WRITE_NODE_COLOR(_set, _sampler, _view, _layout)        \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = _set,                                              \
		.dstBinding = SET_BIND_INDEX_NODE_COLOR,                     \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.sampler = _sampler,                                     \
			.imageView = _view,                                      \
			.imageLayout = _layout,                                  \
		},                                                           \
	}
#define BIND_WRITE_NODE_GBUFFER(_set, _sampler, _view, _layout)      \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = _set,                                              \
		.dstBinding = SET_BIND_INDEX_NODE_GBUFFER,                   \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.sampler = _sampler,                                     \
			.imageView = _view,                                      \
			.imageLayout = _layout,                                  \
		},                                                           \
	}

enum {
	PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL,
	PIPE_SET_INDEX_NODE_GRAPHICS_NODE,
	PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
};
static void CreateGraphicsNodePipeLayout(VkDescriptorSetLayout nodeSetLayout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_GRAPHICS_NODE] = nodeSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
	vkSetDebugName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (u64)*pPipeLayout, "NodePipeLayout");
}

//// Compute Node Pipe
enum {
	SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
	SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
	SET_BIND_INDEX_NODE_COMPUTE_COUNT,
};

static void CreateComputeOutputSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = SET_BIND_INDEX_NODE_COMPUTE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			{
				SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
}
#define BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(set, view)             \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = set,                                           \
		.dstBinding = SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT, \
		.descriptorCount = 1,                                    \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,      \
		.pImageInfo = &(VkDescriptorImageInfo){                  \
			.imageView = view,                                   \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
		},                                                       \
	}
#define BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(set, view)             \
	(VkWriteDescriptorSet) {                                        \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                     \
		.dstSet = set,                                          \
		.dstBinding = SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT, \
		.descriptorCount = 1,                                   \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,     \
		.pImageInfo = &(VkDescriptorImageInfo){                 \
			.imageView = view,                                  \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,             \
		},                                                      \
	}

enum {
	PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL,
	PIPE_SET_INDEX_NODE_COMPUTE_NODE,
	PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT,
	PIPE_SET_INDEX_NODE_COMPUTE_COUNT,
};

static void CreateNodeComputePipeLayout(VkDescriptorSetLayout nodeSetLayout, VkDescriptorSetLayout computeOutputSetLayout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_COMPUTE_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_COMPUTE_NODE] = nodeSetLayout,
			[PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT] = computeOutputSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

//// GBuffer Process Pipe
enum {
	SET_BIND_INDEX_GBUFFER_PROCESS_STATE,
	SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
	SET_BIND_INDEX_GBUFFER_PROCESS_SRC_MIP,
	SET_BIND_INDEX_GBUFFER_PROCESS_DST,
	SET_BIND_INDEX_GBUFFER_PROCESS_COUNT
};

static void CreateGBufferProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_INDEX_GBUFFER_PROCESS_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.nearestSampler,
			},
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_SRC_MIP,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.nearestSampler,
			},
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_DST,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
}

#define BIND_WRITE_GBUFFER_PROCESS_STATE(_view)              \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_STATE,  \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = _view,                                 \
			.range = sizeof(MxcProcessState),                \
		},                                                   \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(_view)                  \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,      \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = _view,                                      \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_MIP(_view)                    \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_MIP,        \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = _view,                                      \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}
#define BIND_WRITE_GBUFFER_PROCESS_DST(_view)               \
	(VkWriteDescriptorSet) {                                \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_DST,   \
		.descriptorCount = 1,                               \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
		.pImageInfo = (VkDescriptorImageInfo[]){            \
			{                                               \
				.imageView = _view,                         \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,     \
			},                                              \
		},                                                  \
	}

enum {
	PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
	PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
};
static void CreateGBufferProcessPipeLayout(VkDescriptorSetLayout layout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT] = layout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

//// Final Blit Pipe
enum {
	SET_BIND_INDEX_FINAL_BLIT_SRC,
	SET_BIND_INDEX_FINAL_BLIT_DST,
	SET_BIND_INDEX_FINAL_BLIT_COUNT,
};
enum {
	BIND_ARRAY_INDEX_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER,
	BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COMPUTE_FRAMEBUFFER,
	BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COUNT,
};

typedef VkDescriptorSetLayout FinalBlitSetLayout;

static void CreateFinalBlitSetLayout(FinalBlitSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_INDEX_FINAL_BLIT_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_FINAL_BLIT_SRC,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COUNT,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			{
				SET_BIND_INDEX_FINAL_BLIT_DST,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
}

#define BIND_WRITE_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER(_graphicsView, _computeView) \
	(VkWriteDescriptorSet) {                                                        \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                                     \
		.dstBinding = SET_BIND_INDEX_FINAL_BLIT_SRC,                                \
		.descriptorCount = BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COUNT,                   \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                         \
		.pImageInfo = (VkDescriptorImageInfo[]){                                    \
			[BIND_ARRAY_INDEX_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER] = {              \
				.imageView = _graphicsView,                                         \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                             \
			},                                                                      \
			[BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COMPUTE_FRAMEBUFFER] = {               \
				.imageView = _computeView,                                          \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                             \
			},                                                                      \
		},                                                                          \
	}
#define BIND_WRITE_FINAL_BLIT_DST(_view)                                                              \
	(VkWriteDescriptorSet) {                                                                          \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                                                       \
		.dstBinding = SET_BIND_INDEX_FINAL_BLIT_DST,                                                  \
		.descriptorCount = 1,                                                                         \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = (VkDescriptorImageInfo[]) { \
			{                                                                                         \
				.imageView = _view,                                                                   \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                                               \
			},                                                                                        \
		}                                                                                             \
	}

enum {
	PIPE_SET_INDEX_FINAL_BLIT_GLOBAL,
	PIPE_SET_INDEX_FINAL_BLIT_INOUT,
	PIPE_SET_INDEX_FINAL_BLIT_COUNT,
};

typedef VkPipelineLayout FinalBlitPipeLayout;

static void CreateFinalBlitPipeLayout(FinalBlitSetLayout inOutLayout, FinalBlitPipeLayout* pPipeLayout) {
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_FINAL_BLIT_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_FINAL_BLIT_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_FINAL_BLIT_INOUT] = inOutLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

/////////////////
//// Time Queries
enum {
	TIME_QUERY_GBUFFER_PROCESS_BEGIN,
	TIME_QUERY_GBUFFER_PROCESS_END,
	TIME_QUERY_QUAD_RENDER_BEGIN,
	TIME_QUERY_QUAD_RENDER_END,
	TIME_QUERY_TESS_RENDER_BEGIN,
	TIME_QUERY_TESS_RENDER_END,
	TIME_QUERY_TASKMESH_RENDER_BEGIN,
	TIME_QUERY_TASKMESH_RENDER_END,
	TIME_QUERY_COMPUTE_RENDER_BEGIN,
	TIME_QUERY_COMPUTE_RENDER_END,
	TIME_QUERY_COUNT,
};

/////////////
//// Barriers
#define COMPOSITOR_DST_GRAPHICS_READ                                          \
	.dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
	                 VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
	                 VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
	                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                             \
	.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

VkImageMemoryBarrier2 localProcessAcquireBarriers[] = {
	{
		// Color
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		COMPOSITOR_DST_GRAPHICS_READ,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Depth
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_READ,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		.image = VK_NULL_HANDLE,
		.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer Mip
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
};

// GraphicsBarriers
VkImageMemoryBarrier2 gfxProcessAcquireBarriers[] = {
	{
		// Color
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		COMPOSITOR_DST_GRAPHICS_READ,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
		.dstQueueFamilyIndex = 0,  // vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Depth
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_READ,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
		.dstQueueFamilyIndex = 0,  //vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer Mip
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
};
VkImageMemoryBarrier2 gfxProcessEndBarriers[] = {
	{
		// Gbuffer
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
		COMPOSITOR_DST_GRAPHICS_READ,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
};

// ComputeBarriers
VkImageMemoryBarrier2 compProcessAcquireBarriers[] = {
	{
		// Color
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_READ,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
		.dstQueueFamilyIndex = 0,  //vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Depth
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_READ,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
		.dstQueueFamilyIndex = 0,  //vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
	{
		// Gbuffer Mip
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
};
VkImageMemoryBarrier2 compProcessEndBarriers[] = {
	{
		// Gbuffer
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.image = VK_NULL_HANDLE,
		VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
		VK_IMAGE_BARRIER_DST_COMPUTE_READ,
		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
	},
};

// I kind of don't like this setup? Maybe need to find different way to write this.
// This isn't thread safe right now. Only 1 compositor thread allowed.
constexpr u32 processAcquireBarrierCount = COUNT(gfxProcessAcquireBarriers);
constexpr u32 processEndBarrierCount = COUNT(gfxProcessEndBarriers);

VkImageMemoryBarrier2* processAcquireBarriers[] = {
	[MXC_COMPOSITOR_MODE_QUAD] = gfxProcessAcquireBarriers,
	[MXC_COMPOSITOR_MODE_TESSELATION] = gfxProcessAcquireBarriers,
	[MXC_COMPOSITOR_MODE_TASK_MESH] = gfxProcessAcquireBarriers,
	[MXC_COMPOSITOR_MODE_COMPUTE] = compProcessAcquireBarriers,
};
VkImageMemoryBarrier2* processEndBarriers[] = {
	[MXC_COMPOSITOR_MODE_QUAD] = gfxProcessEndBarriers,
	[MXC_COMPOSITOR_MODE_TESSELATION] = gfxProcessEndBarriers,
	[MXC_COMPOSITOR_MODE_TASK_MESH] = gfxProcessEndBarriers,
	[MXC_COMPOSITOR_MODE_COMPUTE] = compProcessEndBarriers,
};
VkImageLayout processFinalLayout[] = {
	[MXC_COMPOSITOR_MODE_QUAD] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	[MXC_COMPOSITOR_MODE_TASK_MESH] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	[MXC_COMPOSITOR_MODE_COMPUTE] = VK_IMAGE_LAYOUT_GENERAL,
};

////////
//// Run
////
void mxcCompositorNodeRun(MxcCompositorContext* pCstCtx, MxcCompositor* pCst)
{
	//// Local Extract
	EXTRACT_FIELD(&vk.context, device);
	EXTRACT_FIELD(&vk.context, basicPass);

	EXTRACT_FIELD(pCstCtx, gfxCmd);
	auto compTimeline = pCstCtx->timeline;

	EXTRACT_FIELD(pCst, gfxFrame);
	EXTRACT_FIELD(pCst, globalSet);
	EXTRACT_FIELD(pCst, compOutSet);

	EXTRACT_FIELD(pCst, nodePipeLayout);
	EXTRACT_FIELD(pCst, nodeQuadPipe);
	EXTRACT_FIELD(pCst, nodeTessPipe);
	EXTRACT_FIELD(pCst, nodeTaskMeshPipe);

	EXTRACT_FIELD(pCst, nodeCompPipeLayout);
	EXTRACT_FIELD(pCst, nodeCompPipe);
	EXTRACT_FIELD(pCst, nodePostCompPipe);

	EXTRACT_FIELD(pCst, finalBlitPipe);
	EXTRACT_FIELD(pCst, finalBlitPipeLayout);

	EXTRACT_FIELD(pCst, gbufProcessBlitUpPipe);
	EXTRACT_FIELD(pCst, gbufProcessPipeLayout);
	EXTRACT_FIELD(pCst, pProcessStateMapped);
	auto processSetBuf = pCst->processSetBuffer.buf;

	EXTRACT_FIELD(pCst, timeQryPool);

	auto pSwapCtx = &pCstCtx->swapCtx;

	auto quadMeshOffsets = pCst->quadMesh.offsets;
	auto quadMeshBuf = pCst->quadMesh.buf;

	auto quadPatchOffsets = pCst->quadPatchMesh.offsets;
	auto quadPatchBuf = pCst->quadPatchMesh.sharedBuffer.buf;

	auto pLineBuf = &pCst->lineBuf;

	auto gfxFrameColorView = pCst->gfxFrameTex.color.view;
	auto gfxFrameNormalView = pCst->gfxFrameTex.normal.view;
	auto gfxFrameDepthView = pCst->gfxFrameTex.depth.view;
	auto compFrameColorView = pCst->compFrameColorTex.view;

	auto gfxFrameColorImg = pCst->gfxFrameTex.color.image;

	auto compFrameAtomicImg = pCst->compFrameAtomicTex.image;
	auto compFrameColorImg = pCst->compFrameColorTex.image;

	auto pGlobSetMapped = vkSharedMemoryPtr(pCst->globalBuf.mem);

	u32 mainGraphicsIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

	// We copy everything locally. Set null to ensure not used!
	pCstCtx = NULL;
	pCst = NULL;


	//// Local State


	cam globCam = {
		.yFovRad = RAD_FROM_DEG(45.0f),
		.zNear = 0.1f,
		.zFar = 100.0f,
	};
	pose globCamPose = {
		.pos = VEC3(0.0f, 0.0f, 2.0f),
		.euler = VEC3(0.0f, 0.0f, 0.0f),
	};
	globCamPose.rot = QuatFromEuler(globCamPose.euler);

	VkGlobalSetState globSetState = (VkGlobalSetState){};
	vkUpdateGlobalSetViewProj(globCam, globCamPose, &globSetState, pGlobSetMapped);

CompositeLoop:

	////////////////////////////
	//// MXC_CYCLE_UPDATE_WINDOW_STATE


	////////////////////////////
	//// MXC_CYCLE_PROCESS_INPUT
	vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_PROCESS_INPUT, compTimeline);
	u64 baseCycleValue = compositorContext.baseCycleValue ;

	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcWindowInput.mouseDelta, &globCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcWindowInput.move, &globCamPose);
	vkUpdateGlobalSetView(&globCamPose, &globSetState, pGlobSetMapped);

	////////////////////////////////
	//// MXC_CYCLE_UPDATE_NODE_STATES
	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, compTimeline);

	CmdResetBegin(gfxCmd);
	vk.ResetQueryPool(device, timeQryPool, 0, TIME_QUERY_COUNT);

	//// Iterate Node State Updates
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_GBUFFER_PROCESS_BEGIN);

	ivec2 windowExtent = mxcWindowInput.iDimensions;
	int   windowPixelCt = windowExtent.x * windowExtent.y;
	int   windowGroupCt = windowPixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;

	// this should be a method as the CPU ops do not run every loop. Really it should be a different thread.
	for (u32 iCstMode = MXC_COMPOSITOR_MODE_QUAD; iCstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCstMode) {
		atomic_thread_fence(memory_order_acquire);
		if (activeNodes[iCstMode].ct == 0) continue;

		VkImageMemoryBarrier2* pAcquireBarriers = processAcquireBarriers[iCstMode];
		VkImageMemoryBarrier2* pEndBarriers = processEndBarriers[iCstMode];
		VkImageLayout          finalLayout = processFinalLayout[iCstMode];

		auto pActiveNodes = &activeNodes[iCstMode];
		for (u32 iNode = 0; iNode < pActiveNodes->ct; ++iNode) {
			atomic_thread_fence(memory_order_acquire);
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeShrd = node.pShared[hNode];
			auto pNodeCstData = &node.cstData[hNode];

			//// Update Root Pose
			ATOMIC_FENCE_BLOCK {
				// Update InteractionState and RootPose every cycle no matter what so that app stays responsive to moving.
				// This should probably be in a threaded node.
				vec3 worldDiff = VEC3_ZERO;
				switch (pNodeCstData->interactionState) {
					case NODE_INTERACTION_STATE_SELECT:
						ray priorScreenRay = rayFromScreenUV(mxcWindowInput.priorMouseUV, globSetState.invProj, globSetState.invView, globSetState.invViewProj);
						ray screenRay = rayFromScreenUV(mxcWindowInput.mouseUV, globSetState.invProj, globSetState.invView, globSetState.invViewProj);

						vec4  nodeOrigin = vec4MulMat4(pNodeCstData->nodeSetState.model, VEC4_IDENT);
						Plane plane = {.origin = TO_VEC3(nodeOrigin), .normal = VEC3(0, 0, 1)};

						vec3 hitPoints[2];
						if (rayIntersetPlane(priorScreenRay, plane, &hitPoints[0]) && rayIntersetPlane(screenRay, plane, &hitPoints[1])) {
							worldDiff = vec3Sub(hitPoints[0], hitPoints[1]);
						}

						break;
					default: break;
				}

				pNodeShrd->rootPose.pos.vec -= worldDiff.vec;
				pNodeCstData->nodeSetState.model = mat4FromPosRot(pNodeShrd->rootPose.pos, pNodeShrd->rootPose.rot);
			}

			u64 nodeTimeline = pNodeShrd->timelineValue;
			if (nodeTimeline <= pNodeCstData->lastTimelineValue) continue;

			pNodeCstData->lastTimelineValue = nodeTimeline;
			atomic_thread_fence(memory_order_release);

			//// Acquire new frame from node
			ATOMIC_FENCE_BLOCK {
				pNodeCstData->swapIndex = !(nodeTimeline % VK_SWAP_COUNT);
				auto pNodeSwap = &pNodeCstData->swaps[pNodeCstData->swapIndex];

				// I hate this we need a better way
				pAcquireBarriers[0].image = pNodeSwap->color;
				pAcquireBarriers[0].dstQueueFamilyIndex = mainGraphicsIndex,
				pAcquireBarriers[1].image = pNodeSwap->depth;
				pAcquireBarriers[1].dstQueueFamilyIndex = mainGraphicsIndex,
				pAcquireBarriers[2].image = pNodeSwap->gBuffer;
				pAcquireBarriers[3].image = pNodeSwap->gBufferMip;
				CmdPipelineImageBarriers2(gfxCmd, processAcquireBarrierCount, pAcquireBarriers);

				// TODO this needs to be specifically only the rect which was rendered into
				ivec2 nodeSwapExtent = IVEC2(pNodeShrd->swapWidth, pNodeShrd->swapHeight);
				u32   nodeSwapPixelCt = nodeSwapExtent.x * nodeSwapExtent.y;
				u32   nodeSwapGroupCt = nodeSwapPixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;

				ivec2 mipExtent = {nodeSwapExtent.vec >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT};
				u32   mipPixelCt = mipExtent.x * mipExtent.y;
				u32   mipGroupCt = MAX(mipPixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT, 1);

				auto pProcState = &pNodeShrd->processState;
				pProcState->cameraNearZ = globCam.zNear;
				pProcState->cameraFarZ = globCam.zFar;
				memcpy(pProcessStateMapped, pProcState, sizeof(MxcProcessState));

				vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessBlitUpPipe);

				CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
					BIND_WRITE_GBUFFER_PROCESS_STATE(processSetBuf), BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pNodeSwap->depthView),
					BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwap->gBufferMipView));
				vk.CmdDispatch(gfxCmd, 1, mipGroupCt, 1);

				CMD_IMAGE_BARRIERS(gfxCmd, {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = pNodeSwap->gBufferMip,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_DST_COMPUTE_READ,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				});

				CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
					BIND_WRITE_GBUFFER_PROCESS_SRC_MIP(pNodeSwap->gBufferMipView),
					BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwap->gBufferView));
				vk.CmdDispatch(gfxCmd, 1, nodeSwapGroupCt, 1);

				pEndBarriers[0].image = pNodeSwap->gBuffer;
				CmdPipelineImageBarriers2(gfxCmd, processEndBarrierCount, pEndBarriers);

				CMD_WRITE_SINGLE_SETS(device,
					BIND_WRITE_NODE_COLOR(pNodeCstData->nodeSet, vk.context.nearestSampler, pNodeSwap->colorView, finalLayout),
					BIND_WRITE_NODE_GBUFFER(pNodeCstData->nodeSet, vk.context.nearestSampler, pNodeSwap->gBufferView, finalLayout));
			}

			//// Calc new node uniform and shared data
			ATOMIC_FENCE_BLOCK {
				// Copy previously used global state from node to compositor data for the compositor to use in subsequent reprojections
				memcpy(&pNodeCstData->nodeSetState.view, &pNodeShrd->globalSetState, sizeof(VkGlobalSetState));
				memcpy(&pNodeCstData->nodeSetState.ulUV, &pNodeShrd->clip, sizeof(MxcClip));
				memcpy(pNodeCstData->pSetMapped, &pNodeCstData->nodeSetState, sizeof(MxcNodeCompositorSetState));

				float radius = pNodeShrd->compositorRadius;
				vec3  corners[CORNER_COUNT] = {
                    [CORNER_LUB] = VEC3(-radius, -radius, -radius),
                    [CORNER_LUF] = VEC3(-radius, -radius, radius),
                    [CORNER_LDB] = VEC3(-radius, radius, -radius),
                    [CORNER_LDF] = VEC3(-radius, radius, radius),
                    [CORNER_RUB] = VEC3(radius, -radius, -radius),
                    [CORNER_RUF] = VEC3(radius, -radius, radius),
                    [CORNER_RDB] = VEC3(radius, radius, -radius),
                    [CORNER_RDF] = VEC3(radius, radius, radius),
                };

				vec2 uvMin = VEC2(FLT_MAX, FLT_MAX);
				vec2 uvMax = VEC2(FLT_MIN, FLT_MIN);
				for (int i = 0; i < CORNER_COUNT; ++i) {
					vec4 model = VEC4(corners[i], 1.0f);
					vec4 world = vec4MulMat4(pNodeCstData->nodeSetState.model, model);
					vec4 clip = vec4MulMat4(globSetState.view, world);
					vec3 ndc = Vec4WDivide(vec4MulMat4(globSetState.proj, clip));
					vec2 uv = UVFromNDC(ndc);
					uvMin.x = MIN(uvMin.x, uv.x);
					uvMin.y = MIN(uvMin.y, uv.y);
					uvMax.x = MAX(uvMax.x, uv.x);
					uvMax.y = MAX(uvMax.y, uv.y);
					pNodeCstData->worldCorners[i] = TO_VEC3(world);
					pNodeCstData->uvCorners[i] = uv;
				}
				vec2 uvMinClamp = Vec2Clamp(uvMin, 0.0f, 1.0f);
				vec2 uvMaxClamp = Vec2Clamp(uvMax, 0.0f, 1.0f);
				vec2 uvDiff = {uvMaxClamp.vec - uvMinClamp.vec};

				// This line and interaction logic should probably go into a threaded node.
				// Maybe? With lines potentially staggered and changed at every compositor step, you do want to redraw them every frame.
				// But also you probably couldn't really tell if the lines only updated at 60 fps and was a frame or two off for some node
				bool isHovering = false;
				for (u32 i = 0; i < MXC_CUBE_SEGMENT_COUNT; i += 2) {
					vec3 start = pNodeCstData->worldCorners[MXC_CUBE_SEGMENTS[i]];
					vec3 end = pNodeCstData->worldCorners[MXC_CUBE_SEGMENTS[i + 1]];
					pNodeCstData->worldSegments[i] = start;
					pNodeCstData->worldSegments[i + 1] = end;

					vec2 uvStart = pNodeCstData->uvCorners[MXC_CUBE_SEGMENTS[i]];
					vec2 uvEnd = pNodeCstData->uvCorners[MXC_CUBE_SEGMENTS[i + 1]];
					isHovering |= Vec2PointOnLineSegment(mxcWindowInput.mouseUV, uvStart, uvEnd, 0.0005f);
				}

				switch (pNodeCstData->interactionState) {
					case NODE_INTERACTION_STATE_NONE:
						pNodeCstData->interactionState = isHovering
						                                     ? NODE_INTERACTION_STATE_HOVER
						                                     : NODE_INTERACTION_STATE_NONE;
						break;
					case NODE_INTERACTION_STATE_HOVER:
						pNodeCstData->interactionState = isHovering
						                                     ? mxcWindowInput.leftMouseButton
						                                           ? NODE_INTERACTION_STATE_SELECT
						                                           : NODE_INTERACTION_STATE_HOVER
						                                     : NODE_INTERACTION_STATE_NONE;
						break;
					case NODE_INTERACTION_STATE_SELECT:
						pNodeCstData->interactionState = mxcWindowInput.leftMouseButton
						                                     ? NODE_INTERACTION_STATE_SELECT
						                                     : NODE_INTERACTION_STATE_NONE;
						break;
					default: break;
				}

				// maybe I should only copy camera pose info and generate matrix on other thread? oxr only wants the pose
				pNodeShrd->cameraPose = globCamPose;
				pNodeShrd->cameraPose.pos.vec -= pNodeShrd->rootPose.pos.vec;
				pNodeShrd->camera = globCam;

				pNodeShrd->left.active = false;
				pNodeShrd->left.gripPose = globCamPose;
				pNodeShrd->left.aimPose = globCamPose;
				pNodeShrd->left.selectClick = mxcWindowInput.leftMouseButton;

				pNodeShrd->right.active = true;
				pNodeShrd->right.gripPose = globCamPose;
				pNodeShrd->right.aimPose = globCamPose;
				pNodeShrd->right.selectClick = mxcWindowInput.leftMouseButton;

				// Write current GlobalSetState to NodeShared for node to use in next frame
				// - sizeof(ivec2) so we can fill in framebufferSize constrained to node swap
				memcpy(&pNodeShrd->globalSetState, &globSetState, sizeof(VkGlobalSetState) - sizeof(ivec2));
				pNodeShrd->globalSetState.framebufferSize = IVEC2(uvDiff.x * windowExtent.x, uvDiff.y * windowExtent.y);
				pNodeShrd->clip.ulUV = uvMinClamp;
				pNodeShrd->clip.lrUV = uvMaxClamp;
			}
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_GBUFFER_PROCESS_END);

	////////////////////////////////
	//// MXC_CYCLE_COMPOSITOR_RECORD

	//// Graphics Pipe
	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

	vk.CmdSetViewport(gfxCmd, 0, 1, &(VkViewport){.width = windowExtent.x, .height = windowExtent.y, .maxDepth = 1.0f});
	vk.CmdSetScissor(gfxCmd, 0, 1, &(VkRect2D){.extent = {.width = windowExtent.x, .height = windowExtent.y}});
	CmdBeginRenderPass(gfxCmd, basicPass, gfxFrame, VK_PASS_CLEAR_COLOR, gfxFrameColorView, gfxFrameNormalView, gfxFrameDepthView);

	vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL, 1, &globalSet, 0, NULL);

	bool hasGfx = false;
	bool hasComp = false;

	//// Graphics Quad Node Commands
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_QUAD_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (activeNodes[MXC_COMPOSITOR_MODE_QUAD].ct > 0) {
		hasGfx = true;

		auto pActiveNodes = &activeNodes[MXC_COMPOSITOR_MODE_QUAD];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodeQuadPipe);
		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadMeshBuf}, (VkDeviceSize[]){quadMeshOffsets.vertexOffset});
		vk.CmdBindIndexBuffer(gfxCmd, quadMeshBuf, quadMeshOffsets.indexOffset, VK_INDEX_TYPE_UINT16);

		for (int iNode = 0; iNode < pActiveNodes->ct; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &node.cstData[hNode];
			// TODO this should bne a descriptor array
			vk.CmdBindDescriptorSets(
				gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &pNodeCstData->nodeSet, 0, NULL);
			vk.CmdDrawIndexed(gfxCmd, quadMeshOffsets.indexCount, 1, 0, 0, 0);
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_QUAD_RENDER_END);

	//// Graphics Tesselation Node Commands
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TESS_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (activeNodes[MXC_COMPOSITOR_MODE_TESSELATION].ct > 0) {
		hasGfx = true;

		auto pActiveNodes = &activeNodes[MXC_COMPOSITOR_MODE_TESSELATION];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodeTessPipe);
		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadPatchBuf}, (VkDeviceSize[]){quadPatchOffsets.vertexOffset});
		vk.CmdBindIndexBuffer(gfxCmd, quadPatchBuf, quadPatchOffsets.indexOffset, VK_INDEX_TYPE_UINT16);

		for (int iNode = 0; iNode < pActiveNodes->ct; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &node.cstData[hNode];
			vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &pNodeCstData->nodeSet, 0, NULL);
			vk.CmdDrawIndexed(gfxCmd, quadMeshOffsets.indexCount, 1, 0, 0, 0);
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TESS_RENDER_END);

	//// Graphics Task Mesh Node Commands
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TASKMESH_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (activeNodes[MXC_COMPOSITOR_MODE_TASK_MESH].ct > 0) {
		hasGfx = true;

		auto pActiveNodes = &activeNodes[MXC_COMPOSITOR_MODE_TASK_MESH];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodeTaskMeshPipe);

		for (int iNode = 0; iNode < pActiveNodes->ct; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &node.cstData[hNode];
			vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &pNodeCstData->nodeSet, 0, NULL);
			vk.CmdDrawMeshTasksEXT(gfxCmd, 1, 1, 1);
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TASKMESH_RENDER_END);

	//// Graphic Line Commands
	{
		// TODO this could be another thread and run at a lower rate
		hasGfx = true;

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipeLayout, VK_PIPE_SET_INDEX_LINE_GLOBAL, 1, &globalSet, 0, NULL);

		vkCmdSetLineWidth(gfxCmd, 1.0f);

		for (u32 iCstMode = MXC_COMPOSITOR_MODE_QUAD; iCstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCstMode) {
			atomic_thread_fence(memory_order_acquire);
			int activeNodeCt = activeNodes[iCstMode].ct;
			if (activeNodeCt == 0)
				continue;

			auto pActiveNodes = &activeNodes[iCstMode];
			for (int iNode = 0; iNode < activeNodeCt; ++iNode) {
				auto hNode = pActiveNodes->handles[iNode];
				auto pNodeShrd = node.pShared[hNode];
				auto pNodeCstData = &node.cstData[iNode];

				VkLineMaterialState lineState = {.primaryColor = VEC4(0.5f, 0.5f, 0.5f, 0.5f)};
				switch (pNodeCstData->interactionState) {
					case NODE_INTERACTION_STATE_HOVER:
						lineState = (VkLineMaterialState){.primaryColor = VEC4(0.5f, 0.5f, 1.0f, 0.5f)};
						break;
					case NODE_INTERACTION_STATE_SELECT:
						lineState = (VkLineMaterialState){.primaryColor = VEC4(1.0f, 1.0f, 1.0f, 0.5f)};
						break;
					default: break;
				}

				vkCmdPushLineMaterial(gfxCmd, lineState);

				memcpy(pLineBuf->pMapped, &pNodeCstData->worldSegments, sizeof(vec3) * MXC_CUBE_SEGMENT_COUNT);

				vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){pLineBuf->buffer.buf}, (VkDeviceSize[]){0});
				vkCmdDraw(gfxCmd, MXC_CUBE_SEGMENT_COUNT, 1, 0, 0);
			}
		}
	}

	vk.CmdEndRenderPass(gfxCmd);

	//// Compute Recording Cycle
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_COMPUTE_RENDER_BEGIN);
	if (activeNodes[MXC_COMPOSITOR_MODE_COMPUTE].ct > 0) {
		hasComp = true;

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT, 1, &compOutSet, 0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL, 1, &globalSet, 0, NULL);

		MxcActiveNodes* pActiveNodes = &activeNodes[MXC_COMPOSITOR_MODE_COMPUTE];
		for (int iNode = 0; iNode < pActiveNodes->ct; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &node.cstData[hNode];
			vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_NODE, 1, &pNodeCstData->nodeSet, 0, NULL);
			vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);
		}

		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameAtomicImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			});

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodePostCompPipe);
		vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_COMPUTE_RENDER_END);

	//// Blit Framebuffer
	{
		u32 frameIdx;
		CmdSwapAcquire(device, pSwapCtx, &frameIdx);
		auto pSwap = &pSwapCtx->frames[frameIdx];

		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = gfxFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pSwap->image,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_UNDEFINED,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			});

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipe);
		CMD_BIND_DESCRIPTOR_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipeLayout, PIPE_SET_INDEX_FINAL_BLIT_GLOBAL, globalSet);
		CMD_PUSH_DESCRIPTOR_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipeLayout, PIPE_SET_INDEX_FINAL_BLIT_INOUT,
			BIND_WRITE_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER(hasGfx ? gfxFrameColorView : VK_NULL_HANDLE, hasComp ? compFrameColorView : VK_NULL_HANDLE),
			BIND_WRITE_FINAL_BLIT_DST(pSwap->view));
		vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);

		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pSwap->image,
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_PRESENT,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			});
	}

	vk.EndCommandBuffer(gfxCmd);

	///////////////////////////////
	//// MXC_CYCLE_RENDER_COMPOSITE
	{
		// Signal will submit gfxCmd on main
		vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compTimeline);
	}

	//////////////////////////////////
	//// MXC_CYCLE_UPDATE_WINDOW_STATE
	{
		u64 nextUpdateWindowStateCycle = baseCycleValue + MXC_CYCLE_COUNT + MXC_CYCLE_UPDATE_WINDOW_STATE;
		vkTimelineWait(device, nextUpdateWindowStateCycle, compTimeline);
//		u64 timestampsNS[TIME_QUERY_COUNT];
//		VK_CHECK(vk.GetQueryPoolResults(device, timeQryPool, 0, TIME_QUERY_COUNT, sizeof(u64) * TIME_QUERY_COUNT, timestampsNS, sizeof(u64), VK_QUERY_RESULT_64_BIT));
//		double timestampsMS[TIME_QUERY_COUNT];
//		for (u32 i = 0; i < TIME_QUERY_COUNT; ++i) timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
//		timeQueryMs = timestampsMS[TIME_QUERY_COMPUTE_RENDER_END] - timestampsMS[TIME_QUERY_COMPUTE_RENDER_BEGIN];
	}

	CHECK_RUNNING;
	goto CompositeLoop;
}

///////////
//// Create
////
void mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst)
{
	CreateNodeSetLayout((MxcCompositorMode*)pInfo->enabledCompositorModes, &pCst->nodeSetLayout);
	CreateGraphicsNodePipeLayout(pCst->nodeSetLayout, &pCst->nodePipeLayout);

	/// Graphics Pipes
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_QUAD]) {
		vkCreateQuadMesh(0.5f, &pCst->quadMesh);
		vkCreateBasicPipe(
			"./shaders/basic_comp.vert.spv",
			"./shaders/basic_comp.frag.spv",
			vk.context.basicPass,
			pCst->nodePipeLayout,
			&pCst->nodeQuadPipe);
	}

	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TESSELATION]) {
		vkCreateQuadPatchMeshSharedMemory(&pCst->quadPatchMesh);
		vkCreateBasicTessellationPipe(
			"./shaders/tess_comp.vert.spv",
			"./shaders/tess_comp.tesc.spv",
			"./shaders/tess_comp.tese.spv",
			"./shaders/tess_comp.frag.spv",
			vk.context.basicPass,
			pCst->nodePipeLayout,
			&pCst->nodeTessPipe);
	}

	//	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TASK_MESH]) {
	//		vkCreateBasicTaskMeshPipe("./shaders/mesh_comp.task.spv",
	//								  "./shaders/mesh_comp.mesh.spv",
	//								  "./shaders/mesh_comp.frag.spv",
	//								  vk.context.basicPass,
	//								  pCst->nodePipeLayout,
	//								  &pCst->nodeTaskMeshPipe);
	//	}

	/// Compute Pipe
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TASK_MESH]) {
		CreateComputeOutputSetLayout(&pCst->compOutputSetLayout);

		CreateNodeComputePipeLayout(pCst->nodeSetLayout, pCst->compOutputSetLayout, &pCst->nodeCompPipeLayout);

		vkCreateComputePipe("./shaders/compute_compositor.comp.spv", pCst->nodeCompPipeLayout, &pCst->nodeCompPipe);
		vkCreateComputePipe("./shaders/compute_post_compositor_basic.comp.spv", pCst->nodeCompPipeLayout, &pCst->nodePostCompPipe);
	}

	/// Line Pipe
	{
		VkRequestAllocationInfo lineRequest = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(vec3) * 64,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.locality = VK_LOCALITY_CONTEXT,
			.dedicated = VK_DEDICATED_MEMORY_FALSE,
		};
		vkCreateSharedBuffer(&lineRequest, &pCst->lineBuf.buffer);
	}

	/// Pools
	{
		VkQueryPoolCreateInfo queryInfo = {
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = TIME_QUERY_COUNT,
		};
		VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, VK_ALLOC, &pCst->timeQryPool));

		VkDescriptorPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = MXC_NODE_CAPACITY * 3,
			.poolSizeCount = 3,
			.pPoolSizes = (VkDescriptorPoolSize[]){
				{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = MXC_NODE_CAPACITY},
				{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MXC_NODE_CAPACITY},
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = MXC_NODE_CAPACITY},
			},
		};
		VK_CHECK(vkCreateDescriptorPool(vk.context.device, &poolInfo, VK_ALLOC, &threadContext.descriptorPool));
	}

	/// Global
	{
		vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.globalSetLayout, &pCst->globalSet);
		VkRequestAllocationInfo requestInfo = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(VkGlobalSetState),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		};
		vkCreateSharedBuffer(&requestInfo, &pCst->globalBuf);
	}

	/// GBuffer Process
	{
		CreateGBufferProcessSetLayout(&pCst->gbufProcessSetLayout);
		CreateGBufferProcessPipeLayout(pCst->gbufProcessSetLayout, &pCst->gbufProcessPipeLayout);
		vkCreateComputePipe("./shaders/compositor_gbuffer_blit_mip_step.comp.spv", pCst->gbufProcessPipeLayout, &pCst->gbufProcessBlitUpPipe);

		vkCreateSharedBuffer(
			&(VkRequestAllocationInfo){
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(MxcProcessState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			},
			&pCst->processSetBuffer);
	}

	/// Final Blit
	{
		CreateFinalBlitSetLayout(&pCst->finalBlitSetLayout);
		CreateFinalBlitPipeLayout(pCst->finalBlitSetLayout, &pCst->finalBlitPipeLayout);
		vkCreateComputePipe("./shaders/compositor_final_blit.comp.spv", pCst->finalBlitPipeLayout, &pCst->finalBlitPipe);
	}

	/// Node Sets
	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// Preallocate all Node Set buffers. MxcNodeCompositorSetState * 256 = 130 kb. Small price to pay to ensure contiguous memory on GPU
		// TODO this should be a descriptor array
		VkRequestAllocationInfo requestInfo = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(MxcNodeCompositorSetState),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		};
		vkCreateSharedBuffer(&requestInfo, &node.cstData[i].nodeSetBuffer);

		VkDescriptorSetAllocateInfo setCreateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &pCst->nodeSetLayout,
		};
		vkAllocateDescriptorSets(vk.context.device, &setCreateInfo, &node.cstData[i].nodeSet);
		vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)node.cstData[i].nodeSet, "NodeSet");
	}

	/// Graphics Framebuffers
	{
		VkBasicFramebufferTextureCreateInfo framebufferTextureInfo = {
			.debugName = "CompositeFramebufferTexture",
			.locality = VK_LOCALITY_CONTEXT,
			.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
		};
		vkCreateBasicFramebufferTextures(&framebufferTextureInfo, &pCst->gfxFrameTex);
		VkBasicFramebufferCreateInfo framebufferInfo = {
			.debugName = "CompositeFramebuffer",
			.renderPass = vk.context.basicPass,
		};
		vkCreateBasicFramebuffer(&framebufferInfo, &pCst->gfxFrame);
	}

	/// Compute Output
	{
		VkDedicatedTextureCreateInfo atomicCreateInfo = {
			.debugName = "ComputeAtomicFramebuffer",
			.pImageCreateInfo =
				&(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_R32_UINT,
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_STORAGE_BIT,
				},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = VK_LOCALITY_CONTEXT,
		};
		vkCreateDedicatedTexture(&atomicCreateInfo, &pCst->compFrameAtomicTex);

		VkDedicatedTextureCreateInfo colorCreateInfo = {
			.debugName = "ComputeColorFramebuffer",
			.pImageCreateInfo =
				&(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_R8G8B8A8_UNORM,
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = VK_LOCALITY_CONTEXT,
		};
		vkCreateDedicatedTexture(&colorCreateInfo, &pCst->compFrameColorTex);

		VkDescriptorSetAllocateInfo setInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &pCst->compOutputSetLayout,
		};
		VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &pCst->compOutSet));
		vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pCst->compOutSet, "ComputeOutputSet");

		VK_UPDATE_DESCRIPTOR_SETS(
			BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(pCst->compOutSet, pCst->compFrameAtomicTex.view),
			BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(pCst->compOutSet, pCst->compFrameColorTex.view));

		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS) {
			VkImageMemoryBarrier2 barrs[] = {
				(VkImageMemoryBarrier2){
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = pCst->compFrameAtomicTex.image,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_UNDEFINED,
					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
				(VkImageMemoryBarrier2){
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = pCst->compFrameColorTex.image,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_UNDEFINED,
					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
			};
			vkCmdImageBarriers(cmd, COUNT(barrs), barrs);
		}
	}
}

void mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst) {
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_QUAD]) {}
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TESSELATION]) {
		vkBindUpdateQuadPatchMesh(0.5f, &pCst->quadPatchMesh);
	}
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TASK_MESH]) {}
	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_COMPUTE]) {}

	vkBindSharedBuffer(&pCst->lineBuf.buffer);
	pCst->lineBuf.pMapped = vkSharedBufferPtr(pCst->lineBuf.buffer);

	vkBindSharedBuffer(&pCst->globalBuf);
	VK_UPDATE_DESCRIPTOR_SETS(VK_BIND_WRITE_GLOBAL_BUFFER(pCst->globalSet, pCst->globalBuf.buf));

	vkBindSharedBuffer(&pCst->processSetBuffer);
	pCst->pProcessStateMapped = vkSharedBufferPtr(pCst->processSetBuffer);
	*pCst->pProcessStateMapped = (MxcProcessState){};

	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// should I make them all share the same buffer? probably
		// should also share the same descriptor set with an array
		vkBindSharedBuffer(&node.cstData[i].nodeSetBuffer);
		node.cstData[i].pSetMapped = vkSharedBufferPtr(node.cstData[i].nodeSetBuffer);
		*node.cstData[i].pSetMapped = (MxcNodeCompositorSetState){};

		// this needs to be descriptor array
		VK_UPDATE_DESCRIPTOR_SETS(
			BIND_WRITE_NODE_STATE(node.cstData[i].nodeSet, node.cstData[i].nodeSetBuffer.buf),
			BIND_WRITE_NODE_COLOR(node.cstData[i].nodeSet, vk.context.nearestSampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL),
			BIND_WRITE_NODE_GBUFFER(node.cstData[i].nodeSet, vk.context.nearestSampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL));
	}
}

void* mxcCompNodeThread(MxcCompositorContext* pCtx)
{
	MxcCompositor compositor = {};

	MxcCompositorCreateInfo info = {
		.enabledCompositorModes = {
			[MXC_COMPOSITOR_MODE_QUAD] = true,
			[MXC_COMPOSITOR_MODE_TESSELATION] = true,
			[MXC_COMPOSITOR_MODE_TASK_MESH] = true,
			[MXC_COMPOSITOR_MODE_COMPUTE] = true,
		},
	};

	vkBeginAllocationRequests();
	mxcCreateCompositor(&info, &compositor);
	vkEndAllocationRequests();

	mxcBindUpdateCompositor(&info, &compositor);

	mxcCompositorNodeRun(pCtx, &compositor);

	return NULL;
}
