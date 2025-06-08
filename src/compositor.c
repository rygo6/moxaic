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

#define GRID_QUAD_SQUARE_SIZE  2
#define GRID_QUAD_COUNT  4
#define GRID_SUBGROUP_SQUARE_SIZE  4
#define GRID_SUBGROUP_COUNT           16
#define GRID_WORKGROUP_SQUARE_SIZE 8
#define GRID_WORKGROUP_SUBGROUP_COUNT 64

//////////
//// Pipes
////

constexpr VkShaderStageFlags MXC_COMPOSITOR_MODE_STAGE_FLAGS[] = {
	[MXC_COMPOSITOR_MODE_QUAD] =       VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TASK_MESH] =   VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_COMPUTE] =     VK_SHADER_STAGE_COMPUTE_BIT,
};

//// Node Pipe
enum {
	SET_BIND_INDEX_NODE_STATE,
	SET_BIND_INDEX_NODE_COLOR,
	SET_BIND_INDEX_NODE_GBUFFER,
	SET_BIND_INDEX_NODE_COUNT
};

static void CreateNodeSetLayout(MxcCompositorMode mode, VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = SET_BIND_INDEX_NODE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_NODE_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
			},
			{
				SET_BIND_INDEX_NODE_COLOR,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.nearestSampler,
			},
			{
				SET_BIND_INDEX_NODE_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.nearestSampler,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
}
#define BIND_WRITE_NODE_STATE(_set, _buf)                        \
	(VkWriteDescriptorSet)                                       \
	{                                                            \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
			.dstSet = _set,                                      \
			.dstBinding = SET_BIND_INDEX_NODE_STATE,             \
			.descriptorCount = 1,                                \
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
			.pBufferInfo = &(VkDescriptorBufferInfo){            \
				.buffer = _buf,                                  \
				.range = sizeof(MxcNodeCompositorSetState),      \
			},                                                   \
	}
#define BIND_WRITE_NODE_COLOR(_set, _view, _layout)                      \
	(VkWriteDescriptorSet)                                               \
	{                                                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                          \
			.dstSet = _set,                                              \
			.dstBinding = SET_BIND_INDEX_NODE_COLOR,                     \
			.descriptorCount = 1,                                        \
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
			.pImageInfo = &(VkDescriptorImageInfo){                      \
				.imageView = _view,                                      \
				.imageLayout = _layout,                                  \
			},                                                           \
	}
#define BIND_WRITE_NODE_GBUFFER(_set, _view, _layout)                    \
	(VkWriteDescriptorSet)                                               \
	{                                                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                          \
			.dstSet = _set,                                              \
			.dstBinding = SET_BIND_INDEX_NODE_GBUFFER,                   \
			.descriptorCount = 1,                                        \
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
			.pImageInfo = &(VkDescriptorImageInfo){                      \
				.imageView = _view,                                      \
				.imageLayout = _layout,                                  \
			},                                                           \
	}

enum {
	PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL,
	PIPE_SET_INDEX_NODE_GRAPHICS_NODE,
	PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
};
static void CreateGraphicsNodePipeLayout(
	VkDescriptorSetLayout nodeSetLayout,
	VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_GRAPHICS_NODE]   = nodeSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
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
#define BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(set, view)         \
	(VkWriteDescriptorSet) {                                     \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
            .dstSet = set,                                       \
			.dstBinding = SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT, \
			.descriptorCount = 1,                                \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  \
			.pImageInfo = &(VkDescriptorImageInfo){              \
				.imageView = view,                               \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,          \
			},                                                   \
	}
#define BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(set, view)         \
	(VkWriteDescriptorSet) {                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
            .dstSet = set,                                      \
			.dstBinding = SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT, \
			.descriptorCount = 1,                               \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
			.pImageInfo = &(VkDescriptorImageInfo){             \
				.imageView = view,                              \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
	}

enum {
	PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL,
	PIPE_SET_INDEX_NODE_COMPUTE_NODE,
	PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT,
	PIPE_SET_INDEX_NODE_COMPUTE_COUNT,
};
static void CreateNodeComputePipeLayout(
	VkDescriptorSetLayout nodeSetLayout,
	VkDescriptorSetLayout computeOutputSetLayout,
	VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_COMPUTE_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_COMPUTE_NODE]   = nodeSetLayout,
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

#define BIND_WRITE_GBUFFER_PROCESS_STATE(_view)                  \
	(VkWriteDescriptorSet)                                       \
	{                                                            \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
			.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_STATE,  \
			.descriptorCount = 1,                                \
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
			.pBufferInfo = &(VkDescriptorBufferInfo){            \
				.buffer = _view,                                 \
				.range = sizeof(MxcProcessState),                \
			},                                                   \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(_view)                      \
	(VkWriteDescriptorSet)                                               \
	{                                                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                          \
			.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,      \
			.descriptorCount = 1,                                        \
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
			.pImageInfo = &(VkDescriptorImageInfo){                      \
				.imageView = _view,                                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
			},                                                           \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_MIP(_view)                        \
	(VkWriteDescriptorSet)                                               \
	{                                                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                          \
			.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_MIP,        \
			.descriptorCount = 1,                                        \
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
			.pImageInfo = &(VkDescriptorImageInfo){                      \
				.imageView = _view,                                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
			},                                                           \
	}
#define BIND_WRITE_GBUFFER_PROCESS_DST(_view)                   \
	(VkWriteDescriptorSet)                                      \
	{                                                           \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
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
	SET_BIND_INDEX_FINAL_BLIT_COUNT
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
	(VkWriteDescriptorSet)                                                          \
	{                                                                               \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                                     \
			.dstBinding = SET_BIND_INDEX_FINAL_BLIT_SRC,                            \
			.descriptorCount = BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COUNT,               \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                     \
			.pImageInfo = (VkDescriptorImageInfo[]){                                \
				[BIND_ARRAY_INDEX_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER] = {          \
					.imageView = _graphicsView,                                     \
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                         \
				},                                                                  \
				[BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COMPUTE_FRAMEBUFFER] = {           \
					.imageView = _computeView,                                      \
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                         \
				},                                                                  \
			},                                                                      \
	}
#define BIND_WRITE_FINAL_BLIT_DST(_view)                        \
	(VkWriteDescriptorSet)                                      \
	{                                                           \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
			.dstBinding = SET_BIND_INDEX_FINAL_BLIT_DST,        \
			.descriptorCount = 1,                               \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
			.pImageInfo = (VkDescriptorImageInfo[])             \
		{                                                       \
			{                                                   \
				.imageView = _view,                             \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
		}                                                       \
	}

enum {
	PIPE_SET_INDEX_FINAL_BLIT_INOUT,
	PIPE_SET_INDEX_FINAL_BLIT_COUNT,
};
typedef VkPipelineLayout FinalBlitPipeLayout;
static void CreateFinalBlitPipeLayout(FinalBlitSetLayout layout, FinalBlitPipeLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_FINAL_BLIT_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_FINAL_BLIT_INOUT] = layout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

////////
//// Run
////
void mxcCompositorNodeRun(const MxcCompositorContext* pCstCtx, const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst)
{
	//// Local Extract
	EXTRACT_FIELD(&vk.context, device);
	EXTRACT_FIELD(&vk.context, basicPass);

	EXTRACT_FIELD(pCstCtx, gfxCmd);
	EXTRACT_FIELD(pCstCtx, timeline);

	EXTRACT_FIELD(pInfo, compMode);

	EXTRACT_FIELD(pCst, gfxFrame);
	EXTRACT_FIELD(pCst, globalSet);
	EXTRACT_FIELD(pCst, compOutSet);

	EXTRACT_FIELD(pCst, nodePipeLayout);
	EXTRACT_FIELD(pCst, nodeQuadPipe);

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
	EXTRACT_FIELD(pCstCtx, swapCtx);

	auto quadMeshOffsets = pCst->quadMesh.offsets;
	auto quadMeshBuf = pCst->quadMesh.buf;

	auto quadPatchOffsets = pCst->quadMesh.offsets;
	auto quadPatchBuf = pCst->quadMesh.buf;

	auto pLineBuf = &pCst->lineBuf;

	auto gfxFrameColorView = pCst->gfxFrameTex.color.view;
	auto gfxFrameNormalView = pCst->gfxFrameTex.normal.view;
	auto gfxFrameDepthView = pCst->gfxFrameTex.depth.view;
	auto compFrameColorView = pCst->compFrameColorTex.view;

	auto gfxFrameColorImg = pCst->gfxFrameTex.color.image;
	auto compFrameColorImg = pCst->compFrameColorTex.image;

	auto pGlobSetMapped = vkSharedMemoryPtr(pCst->globalBuf.mem);

	// We copy everything locally. Set null to ensure not used!
	pCstCtx = NULL;
	pInfo = NULL;
	pCst = NULL;


	//// Local State
	bool timestampRecorded = false;

	u64 baseCycleValue = 0;

	MidCamera globCam = {
		.yFovRad = RAD_FROM_DEG(45.0f),
		.zNear = 0.1f,
		.zFar = 100.0f,
	};
	MidPose globCamPose = {
		.position = VEC3(0.0f, 0.0f, 2.0f),
		.euler = VEC3(0.0f, 0.0f, 0.0f),
	};
	globCamPose.rotation = QuatFromEuler(globCamPose.euler);

	VkGlobalSetState  globSetState = (VkGlobalSetState){};
	vkUpdateGlobalSetViewProj(globCam, globCamPose, &globSetState, pGlobSetMapped);

#define COMPOSITOR_DST_GRAPHICS_READ                                         \
	.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
					VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
					VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                            \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

	// Local Barriers
	VkImageMemoryBarrier2 localAcquireBarriers[] = {
		{
			// Color
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			COMPOSITOR_DST_GRAPHICS_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
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
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Gbuffer Mip
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};

	// GraphicsBarriers
	VkImageMemoryBarrier2 graphicAcquireBarriers[] = {
		{
			// Color
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			COMPOSITOR_DST_GRAPHICS_READ,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
			.dstQueueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Depth
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
			.dstQueueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Gbuffer Mip
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};
	VkImageMemoryBarrier2 graphicProcessEndBarriers[] = {
		{
			// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
			COMPOSITOR_DST_GRAPHICS_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};

	// ComputeBarriers
	VkImageMemoryBarrier2 computeAcquireBarriers[] = {
		{
			// Color
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
			.dstQueueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Depth
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
			.dstQueueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{
			// Gbuffer Mip
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};
	VkImageMemoryBarrier2 computeProcessEndBarriers[] = {
		{
			// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};

	bool lineHover = false;

	/////////
	//// Loop
run_loop:

	timestampRecorded = false;

	vkTimelineWait(device, baseCycleValue + MXC_CYCLE_PROCESS_INPUT, timeline);

	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcWindowInput.mouseDelta, &globCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcWindowInput.move, &globCamPose);

	vkUpdateGlobalSetView(&globCamPose, &globSetState, pGlobSetMapped);

	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, timeline);
	CmdResetBegin(gfxCmd);


	/////////////////////////////
	//// Update Node States Cycle
	for (int iNode = 0; iNode < nodeCt; ++iNode) {
		auto pNodeShrd = activeNodeShrd[iNode];
		auto pNodeCstData = &nodeCstLocal[iNode];

		pNodeShrd->rootPose.position = VEC3(0, 0, 0);
		pNodeCstData->nodeSetState.model = Mat4FromPosRot(pNodeShrd->rootPose.position, pNodeShrd->rootPose.rotation);

		// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
		// shared: 569 - semaphore: 315416 ratio: 554.333919
		u64 nodeTimeline = atomic_load_explicit(&pNodeShrd->timelineValue, memory_order_acquire);
		if (nodeTimeline <= pNodeCstData->lastTimelineValue)
			continue;

		//// Acquire new framebuffers from node
		{
			int  iNodeSwap = !(nodeTimeline % VK_SWAP_COUNT);
			auto pNodeSwap = &pNodeCstData->swaps[iNodeSwap];

			VkImageMemoryBarrier2* pAcqrBars;
			VkImageMemoryBarrier2* pEndBars;
			VkImageLayout          finalLayout;
			// we want to seperates this into compute and graphics loops
			switch (pNodeShrd->compositorMode) {

				case MXC_COMPOSITOR_MODE_QUAD:
				case MXC_COMPOSITOR_MODE_TESSELATION:
				case MXC_COMPOSITOR_MODE_TASK_MESH:
					pAcqrBars = graphicAcquireBarriers;
					pEndBars = graphicProcessEndBarriers;
					finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					break;
				case MXC_COMPOSITOR_MODE_COMPUTE:
					pAcqrBars = computeAcquireBarriers;
					pEndBars = computeProcessEndBarriers;
					finalLayout = VK_IMAGE_LAYOUT_GENERAL;
					break;
				default:
					PANIC("Compositor mode not implemented!");
			}

			pAcqrBars[0].image = pNodeSwap->color;
			pAcqrBars[1].image = pNodeSwap->depth;
			pAcqrBars[2].image = pNodeSwap->gBuffer;
			pAcqrBars[3].image = pNodeSwap->gBufferMip;
			CmdPipelineImageBarriers2(gfxCmd, 4, pAcqrBars);

			vk.ResetQueryPool(device, timeQryPool, 0, 2);
			vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, 0);

			// TODO this needs ot be construct the UL LR uv that was rendered into
			// gBuffer size should be dynamically chosen?
			ivec2 extent = IVEC2(pNodeShrd->swapWidth, pNodeShrd->swapHeight);
			u32   pixelCt = extent.x * extent.y;
			u32   groupCt = pixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;

			ivec2 mipExtent = {extent.vec >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT};
			u32   mipPixelCt = mipExtent.x * mipExtent.y;
			u32   mipGroupCt = MAX(mipPixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT, 1);

			auto pProcState = &pNodeShrd->processState;
			pProcState->cameraNearZ = globCam.zNear;
			pProcState->cameraFarZ = globCam.zFar;
			memcpy(pProcessStateMapped, pProcState, sizeof(MxcProcessState));

			vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessBlitUpPipe);

			VkWriteDescriptorSet mipPushSets[] = {
				BIND_WRITE_GBUFFER_PROCESS_STATE(processSetBuf),
				BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pNodeSwap->depthView),
				BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwap->gBufferMipView),
			};
			vk.CmdPushDescriptorSetKHR(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT, COUNT(mipPushSets), mipPushSets);
			vk.CmdDispatch(gfxCmd, 1, mipGroupCt, 1);

			CMD_IMAGE_BARRIERS(gfxCmd, {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeSwap->gBufferMip,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});

			VkWriteDescriptorSet pushSets[] = {
				BIND_WRITE_GBUFFER_PROCESS_SRC_MIP(pNodeSwap->gBufferMipView),
				BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwap->gBufferView),
			};
			vk.CmdPushDescriptorSetKHR(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT, COUNT(pushSets), pushSets);
			vk.CmdDispatch(gfxCmd, 1, groupCt, 1);

			pEndBars[0].image = pNodeSwap->gBuffer;
			CmdPipelineImageBarriers2(gfxCmd, 1, pEndBars);

			vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQryPool, 1);
			timestampRecorded = true;

			VkWriteDescriptorSet writeSets[] = {
				BIND_WRITE_NODE_COLOR(pNodeCstData->nodeSet, pNodeSwap->colorView, finalLayout),
				BIND_WRITE_NODE_GBUFFER(pNodeCstData->nodeSet, pNodeSwap->gBufferView, finalLayout),
			};
			vkUpdateDescriptorSets(device, COUNT(writeSets), writeSets, 0, NULL);
		}

		pNodeCstData->lastTimelineValue = nodeTimeline;

		//// Calc new node uniform and shared data
		{
			// Move the globalSetState that was previously used to render into the nodeSetState to use in cst
			auto pGlobSetState = &pNodeShrd->globalSetState;
			auto pNodeSetState = &pNodeCstData->nodeSetState;
			// Offset by cst node pos/rot
			mat4 invModel = Mat4Inv(pNodeSetState->model);
			pNodeSetState->view = Mat4Mul(pGlobSetState->view, invModel);
			pNodeSetState->proj = pGlobSetState->proj;
			pNodeSetState->viewProj = Mat4Mul(pNodeSetState->proj, pNodeSetState->view);
			pNodeSetState->invView = Mat4Inv(pNodeSetState->view);
			pNodeSetState->invProj = pGlobSetState->invProj;
			pNodeSetState->invViewProj = Mat4Inv(pNodeSetState->viewProj);
			pNodeSetState->ulUV = pNodeShrd->ulClipUV;
			pNodeSetState->lrUV = pNodeShrd->lrClipUV;

			memcpy(pNodeCstData->pSetMapped, &pNodeCstData->nodeSetState, sizeof(MxcNodeCompositorSetState));

			enum : u8 {
				CORNER_LUB,
				CORNER_LUF,
				CORNER_LDB,
				CORNER_LDF,
				CORNER_RUB,
				CORNER_RUF,
				CORNER_RDB,
				CORNER_RDF,
				CORNER_COUNT,
			};
			constexpr u8 cubeCornerSegments[] = {
			CORNER_LUF, CORNER_LUB,
			CORNER_LUB, CORNER_RUB,
			CORNER_RUB, CORNER_RUF,
			CORNER_RUF, CORNER_LUF,

			CORNER_RDF, CORNER_RDB,
			CORNER_RDB, CORNER_LDB,
			CORNER_LDB, CORNER_LDF,
			CORNER_LDF, CORNER_RDF,

			CORNER_LUF, CORNER_LDF,
			CORNER_LUB, CORNER_LDB,
			CORNER_RUF, CORNER_RDF,
			CORNER_RUB, CORNER_RDB,
			};

			float radius = pNodeShrd->compositorRadius;
			vec3 corners[CORNER_COUNT] = {
				[CORNER_LUB] = VEC3(-radius, -radius, -radius),
				[CORNER_LUF] = VEC3(-radius, -radius, radius),
				[CORNER_LDB] = VEC3(-radius, radius, -radius),
				[CORNER_LDF] = VEC3(-radius, radius, radius),
				[CORNER_RUB] = VEC3(radius, -radius, -radius),
				[CORNER_RUF] = VEC3(radius, -radius, radius),
				[CORNER_RDB] = VEC3(radius, radius, -radius),
				[CORNER_RDF] = VEC3(radius, radius, radius),
			};
			vec2 uvCorners[CORNER_COUNT];

			vec2 uvMin = VEC2(FLT_MAX, FLT_MAX);
			vec2 uvMax = VEC2(FLT_MIN, FLT_MIN);
			for (int i = 0; i < CORNER_COUNT; ++i) {
				vec4 model = VEC4(corners[i], 1.0f);
				vec4 world = Vec4MulMat4(pNodeCstData->nodeSetState.model, model);
				vec4 clip = Vec4MulMat4(globSetState.view, world);
				vec3 ndc = Vec4WDivide(Vec4MulMat4(globSetState.proj, clip));
				vec2 uv = UVFromNDC(ndc);
				uvMin.x = MIN(uvMin.x, uv.x);
				uvMin.y = MIN(uvMin.y, uv.y);
				uvMax.x = MAX(uvMax.x, uv.x);
				uvMax.y = MAX(uvMax.y, uv.y);
				uvCorners[i] = uv;
			}

			vec2 mouseUV = mxcWindowInput.mouseUV;

			vkLineClear(pLineBuf);
			lineHover = false;
			for (u32 i = 0; i < COUNT(cubeCornerSegments); i += 2) {
				vkLineAdd(pLineBuf, corners[cubeCornerSegments[i]], corners[cubeCornerSegments[i+1]]);
				if (Vec2PointOnLineSegment(mouseUV, uvCorners[cubeCornerSegments[i]], uvCorners[cubeCornerSegments[i+1]], 0.0005f))
					lineHover = true;
			}

			// maybe I should only copy camera pose info and generate matrix on other thread? oxr only wants the pose
			pNodeShrd->cameraPose = globCamPose;
			pNodeShrd->camera = globCam;

			pNodeShrd->left.active = false;
			pNodeShrd->left.gripPose = globCamPose;
			pNodeShrd->left.aimPose = globCamPose;
			pNodeShrd->left.selectClick = mxcWindowInput.leftMouseButton;

			pNodeShrd->right.active = true;
			pNodeShrd->right.gripPose = globCamPose;
			pNodeShrd->right.aimPose = globCamPose;
			pNodeShrd->right.selectClick = mxcWindowInput.leftMouseButton;

			// write current global set state to node's global set state to use for next node render with new the framebuffer size
			memcpy(&pNodeShrd->globalSetState, &globSetState, sizeof(VkGlobalSetState) - sizeof(ivec2));

			vec2 uvMinClamp = Vec2Clamp(uvMin, 0.0f, 1.0f);
			vec2 uvMaxClamp = Vec2Clamp(uvMax, 0.0f, 1.0f);
			vec2 uvDiff = {uvMaxClamp.vec - uvMinClamp.vec};

			pNodeShrd->globalSetState.framebufferSize = IVEC2(uvDiff.x * DEFAULT_WIDTH, uvDiff.y * DEFAULT_HEIGHT);
			pNodeShrd->ulClipUV = uvMinClamp;
			pNodeShrd->lrClipUV = uvMaxClamp;
			atomic_thread_fence(memory_order_release);
		}
	}

	////////////////////////////
	/// Graphics Recording Cycle
	{
		vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, timeline);

		vkCmdSetViewport(gfxCmd, 0, 1, &(VkViewport){.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f});
		vkCmdSetScissor(gfxCmd, 0, 1, &(VkRect2D){.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}});
		CmdBeginRenderPass(gfxCmd, basicPass, gfxFrame, VK_PASS_CLEAR_COLOR, gfxFrameColorView, gfxFrameNormalView, gfxFrameDepthView);

		//// Graphic Node Commands
		{
			vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodeQuadPipe);
			vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL, 1, &globalSet, 0, NULL);

			for (int i = 0; i < nodeCt; ++i) {
				auto pNodShrd = activeNodeShrd[i];

				// these should be different 'active' arrays so all of a similiar type can run at once and we dont have to switch
				switch (pNodShrd->compositorMode) {
					case MXC_COMPOSITOR_MODE_QUAD:
						vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCstLocal[i].nodeSet, 0, NULL);
						vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadMeshBuf}, (VkDeviceSize[]){quadMeshOffsets.vertexOffset});
						vk.CmdBindIndexBuffer(gfxCmd, quadMeshBuf, 0, VK_INDEX_TYPE_UINT16);
						vk.CmdDrawIndexed(gfxCmd, quadMeshOffsets.indexCount, 1, 0, 0, 0);
						break;
					case MXC_COMPOSITOR_MODE_TESSELATION:
						vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCstLocal[i].nodeSet, 0, NULL);
						vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadPatchBuf}, (VkDeviceSize[]){quadPatchOffsets.vertexOffset});
						vk.CmdBindIndexBuffer(gfxCmd, quadPatchBuf, quadPatchOffsets.indexOffset, VK_INDEX_TYPE_UINT16);
						vk.CmdDrawIndexed(gfxCmd, quadPatchOffsets.indexCount, 1, 0, 0, 0);
						break;
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCstLocal[i].nodeSet, 0, NULL);
						vk.CmdDrawMeshTasksEXT(gfxCmd, 1, 1, 1);
						break;
					default:
						break;
				}
			}
		}

		//// Graphic Line Commands
		// TODO this could be another thread and run at a lower rate
		{
			vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipe);
			vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipeLayout, VK_PIPE_SET_INDEX_LINE_GLOBAL, 1, &globalSet, 0, NULL);

//			for (int i = 0; i < pLineBuf->count; ++i) {
//
//			}

			auto lineState = lineHover ?
								 (VkLineMaterialState){.primaryColor = VEC4(0, 1, 0, 0.5f)} :
								 (VkLineMaterialState){.primaryColor = VEC4(1, 1, 1, 0.5f)};
			vkCmdPushLineMaterial(gfxCmd, lineState);

			vkCmdSetLineWidth(gfxCmd, 1.0f);

			memcpy(pLineBuf->pMapped, &pLineBuf->state, sizeof(vec3) * pLineBuf->count);

			vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){pLineBuf->buffer.buf}, (VkDeviceSize[]){0});
			vkCmdDraw(gfxCmd, pLineBuf->count, 1, 0, 0);
		}

		vk.CmdEndRenderPass(gfxCmd);
	}

//		ResetQueryPool(device, timeQueryPool, 0, 2);
//		CmdWriteTimestamp2(graphCmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

	////////////////////////////
	//// Compute Recording Cycle
	// We really must separate into Compute and Graphics lists
	{
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL, 1, &globalSet, 0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT, 1, &compOutSet, 0, NULL);

//		vkCmdClearColorImage(graphCmd, cmptFbColorImg, VK_IMAGE_LAYOUT_GENERAL, &VK_PASS_CLEAR_COLOR, 1, &VK_COLOR_SUBRESOURCE_RANGE);

		for (int iNode = 0; iNode < nodeCt; ++iNode) {
			auto pNodShrd = activeNodeShrd[iNode];

			// these should be different 'active' arrays so all of a similiar type can run at once and we dont have to switch
			// really all the nodes need to be set in UBO array and the compute shader do this loop
			switch (pNodShrd->compositorMode) {
				case MXC_COMPOSITOR_MODE_COMPUTE:
					vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_NODE, 1, &nodeCstLocal[iNode].nodeSet, 0, NULL);

					// can be saved
					ivec2 extent = IVEC2(DEFAULT_WIDTH, DEFAULT_HEIGHT);
					int   pixelCt = extent.x * extent.y;
					int   groupCt = pixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;
//						int   quadGroupCt = pixlCt / GRID_SUBGROUP_CAPACITY / GRID_WORKGROUP_SUBGROUP_COUNT / GRID_QUAD_COUNT;

					vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeCompPipe);
					vk.CmdDispatch(gfxCmd, 1, groupCt, 1);

//					// TODO THIS SHOULD BE HAPPENING EVERY FOR LOOP ITERATION
//					VkImageMemoryBarrier2 postCmptBarr[] = {
//						{
//							VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//							.image = pNodeSwap->color,
//							.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//							VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
//							VK_IMAGE_BARRIER_DST_COMPUTE_READ,
//							VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//						},
//						{
//							VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//							.image = pNodeSwap->gBuffer,
//							.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//							VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
//							VK_IMAGE_BARRIER_DST_COMPUTE_READ,
//							VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//						},
//					};
//					CmdPipelineImageBarriers2(graphCmd, COUNT(postCmptBarr), postCmptBarr);

					vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodePostCompPipe);
					vk.CmdDispatch(gfxCmd, 1, groupCt, 1);

					break;
				default:
					break;
			}
		}
	}

	// should have separate compute and graphics queries
//		CmdWriteTimestamp2(graphCmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

	/////////////////////
	//// Blit Framebuffer
	{
		u32 swapIndex; vk.AcquireNextImageKHR(device, swapCtx.chain, UINT64_MAX, swapCtx.acquireSemaphore, VK_NULL_HANDLE, &swapIndex);
		atomic_store_explicit(&compositorContext.swapIdx, swapIndex, memory_order_release);
		VkImage swapImage = swapCtx.images[compositorContext.swapIdx];
		VkImageView swapView = swapCtx.views[compositorContext.swapIdx];

		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = gfxFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = swapImage,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_UNDEFINED,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE
			},
		);

		ivec2 extent = IVEC2(DEFAULT_WIDTH, DEFAULT_HEIGHT);
		u32   pixelCt = extent.x * extent.y;
		u32   groupCt = pixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipe);
		VkWriteDescriptorSet blitPushSets[] = {
			BIND_WRITE_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER(gfxFrameColorView, compFrameColorView),
			BIND_WRITE_FINAL_BLIT_DST(swapView),
		};
		vk.CmdPushDescriptorSetKHR(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipeLayout, PIPE_SET_INDEX_FINAL_BLIT_INOUT, COUNT(blitPushSets), blitPushSets);
		vk.CmdDispatch(gfxCmd, 1, groupCt, 1);

		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = swapImage,
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = compFrameColorImg,
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
			},
		);
	}

	vk.EndCommandBuffer(gfxCmd);

	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, timeline);

	baseCycleValue += MXC_CYCLE_COUNT;

	{ // wait for end and output query, probably don't need this wait if not querying?
		vkTimelineWait(device, baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, timeline);

		if (timestampRecorded) {
			uint64_t timestampsNS[2];
			vk.GetQueryPoolResults(device, timeQryPool, 0, 2, sizeof(uint64_t) * 2, timestampsNS, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			double timestampsMS[2];
			for (uint32_t i = 0; i < 2; ++i) {
				timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
			}
			timeQueryMs = timestampsMS[1] - timestampsMS[0];
		}
	}

	CHECK_RUNNING;
	goto run_loop;
}

///////////
//// Create
////
void mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCompositor)
{
	/// Graphics Pipes
	{
		CreateNodeSetLayout(pInfo->compMode, &pCompositor->nodeSetLayout);
		CreateGraphicsNodePipeLayout(pCompositor->nodeSetLayout, &pCompositor->nodePipeLayout);

		vkCreateQuadMesh(0.5f, &pCompositor->quadMesh);
		vkCreateQuadPatchMeshSharedMemory(&pCompositor->quadPatchMesh);

		// we need all of these modes available, this should be flags
		switch (pInfo->compMode) {
			case MXC_COMPOSITOR_MODE_QUAD:
				vkCreateBasicPipe("./shaders/basic_comp.vert.spv",
								  "./shaders/basic_comp.frag.spv",
								  vk.context.basicPass,
								  pCompositor->nodePipeLayout,
								  &pCompositor->nodeQuadPipe);
				break;
			case MXC_COMPOSITOR_MODE_TESSELATION:
				vkCreateBasicTessellationPipe("./shaders/tess_comp.vert.spv",
											  "./shaders/tess_comp.tesc.spv",
											  "./shaders/tess_comp.tese.spv",
											  "./shaders/tess_comp.frag.spv",
											  vk.context.basicPass,
											  pCompositor->nodePipeLayout,
											  &pCompositor->nodeQuadPipe);

				break;
			case MXC_COMPOSITOR_MODE_TASK_MESH:
				vkCreateBasicTaskMeshPipe("./shaders/mesh_comp.task.spv",
										  "./shaders/mesh_comp.mesh.spv",
										  "./shaders/mesh_comp.frag.spv",
										  vk.context.basicPass,
										  pCompositor->nodePipeLayout,
										  &pCompositor->nodeQuadPipe);
				break;
			default: PANIC("Compositor mode not supported!");
		}

		VkRequestAllocationInfo lineRequest = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(vec3) * 64,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.locality = VK_LOCALITY_CONTEXT,
			.dedicated = VK_DEDICATED_MEMORY_FALSE,
		};
		vkCreateSharedBuffer(&lineRequest, &pCompositor->lineBuf.buffer);
	}

	/// Compute Pipe
	{
		CreateComputeOutputSetLayout(&pCompositor->compOutputSetLayout);
		CreateNodeComputePipeLayout(pCompositor->nodeSetLayout,
									pCompositor->compOutputSetLayout,
									&pCompositor->nodeCompPipeLayout);
		vkCreateComputePipe("./shaders/compute_compositor.comp.spv",
							pCompositor->nodeCompPipeLayout,
							&pCompositor->nodeCompPipe);
		vkCreateComputePipe("./shaders/compute_post_compositor_basic.comp.spv",
							pCompositor->nodeCompPipeLayout,
							&pCompositor->nodePostCompPipe);
	}

	/// Pools
	{
		VkQueryPoolCreateInfo queryInfo = {
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = 2,
		};
		VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, VK_ALLOC, &pCompositor->timeQryPool));

		VkDescriptorPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = MXC_NODE_CAPACITY * 3,
			.poolSizeCount = 3,
			.pPoolSizes = (VkDescriptorPoolSize[]){
				{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = MXC_NODE_CAPACITY},
				{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MXC_NODE_CAPACITY},
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = MXC_NODE_CAPACITY},
			},
		};
		VK_CHECK(vkCreateDescriptorPool(vk.context.device, &poolInfo, VK_ALLOC, &threadContext.descriptorPool));
	}

	/// Global
	{
		vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.globalSetLayout, &pCompositor->globalSet);
		VkRequestAllocationInfo requestInfo = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(VkGlobalSetState),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		};
		vkCreateSharedBuffer(&requestInfo, &pCompositor->globalBuf);
	}

	/// GBuffer Process
	{
		CreateGBufferProcessSetLayout(&pCompositor->gbufProcessSetLayout);
		CreateGBufferProcessPipeLayout(pCompositor->gbufProcessSetLayout, &pCompositor->gbufProcessPipeLayout);
		vkCreateComputePipe("./shaders/compositor_gbuffer_blit_mip_step.comp.spv", pCompositor->gbufProcessPipeLayout, &pCompositor->gbufProcessBlitUpPipe);

		vkCreateSharedBuffer(
			&(VkRequestAllocationInfo){
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(MxcProcessState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			},
			&pCompositor->processSetBuffer);
	}

	/// Final Blit
	{
		CreateFinalBlitSetLayout(&pCompositor->finalBlitSetLayout);
		CreateFinalBlitPipeLayout(pCompositor->finalBlitSetLayout, &pCompositor->finalBlitPipeLayout);
		vkCreateComputePipe("./shaders/compositor_final_blit.comp.spv", pCompositor->finalBlitPipeLayout, &pCompositor->finalBlitPipe);
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
		vkCreateSharedBuffer(&requestInfo, &nodeCstLocal[i].nodeSetBuffer);

		VkDescriptorSetAllocateInfo setCreateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &pCompositor->nodeSetLayout,
		};
		vkAllocateDescriptorSets(vk.context.device, &setCreateInfo, &nodeCstLocal[i].nodeSet);
		vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)nodeCstLocal[i].nodeSet, "NodeSet");
	}

	/// Graphics Framebuffers
	{
		VkBasicFramebufferTextureCreateInfo framebufferTextureInfo = {
			.debugName = "CompositeFramebufferTexture",
			.locality = VK_LOCALITY_CONTEXT,
			.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
		};
		vkCreateBasicFramebufferTextures(&framebufferTextureInfo, &pCompositor->gfxFrameTex);
		VkBasicFramebufferCreateInfo framebufferInfo = {
			.debugName = "CompositeFramebuffer",
			.renderPass = vk.context.basicPass,
		};
		vkCreateBasicFramebuffer(&framebufferInfo, &pCompositor->gfxFrame);
	}

	/// Compute Output
	{
		VkTextureCreateInfo atomicCreateInfo = {
			.debugName = "ComputeAtomicFramebuffer",
			.pImageCreateInfo = &(VkImageCreateInfo){
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
		vkCreateTexture(&atomicCreateInfo, &pCompositor->compFrameAtomicTex);

		VkTextureCreateInfo colorCreateInfo = {
			.debugName = "ComputeColorFramebuffer",
			.pImageCreateInfo = &(VkImageCreateInfo){
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
		vkCreateTexture(&colorCreateInfo, &pCompositor->compFrameColorTex);

		VkDescriptorSetAllocateInfo setInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &pCompositor->compOutputSetLayout,
		};
		VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &pCompositor->compOutSet));
		vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pCompositor->compOutSet, "ComputeOutputSet");

		VkWriteDescriptorSet writeSets[] = {
			BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(pCompositor->compOutSet, pCompositor->compFrameAtomicTex.view),
			BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(pCompositor->compOutSet, pCompositor->compFrameColorTex.view),
		};
		vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);

		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS)
		{
			VkImageMemoryBarrier2 barrs[] = {
				(VkImageMemoryBarrier2){
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = pCompositor->compFrameAtomicTex.image,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_UNDEFINED,
					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
				(VkImageMemoryBarrier2){
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = pCompositor->compFrameColorTex.image,
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

void mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst)
{
	switch (pInfo->compMode) {
		case MXC_COMPOSITOR_MODE_QUAD:
			break;
		case MXC_COMPOSITOR_MODE_TESSELATION:
			break;
		case MXC_COMPOSITOR_MODE_TASK_MESH:
			break;
		default: PANIC("Compositor mode not supported!");
	}

	vkBindUpdateQuadPatchMesh(0.5f, &pCst->quadPatchMesh);

	vkBindSharedBuffer(&pCst->lineBuf.buffer);
	pCst->lineBuf.pMapped = vkSharedBufferPtr(pCst->lineBuf.buffer);

	vkBindSharedBuffer(&pCst->globalBuf);

	vkBindSharedBuffer(&pCst->processSetBuffer);
	pCst->pProcessStateMapped = vkSharedBufferPtr(pCst->processSetBuffer);
	*pCst->pProcessStateMapped = (MxcProcessState){};

	vkUpdateDescriptorSets(vk.context.device, 1, &VK_BIND_WRITE_GLOBAL_BUFFER(pCst->globalSet, pCst->globalBuf.buf), 0, NULL);
	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// should I make them all share the same buffer? probably
		// should also share the same descriptor set with an array
		vkBindSharedBuffer(&nodeCstLocal[i].nodeSetBuffer);
		nodeCstLocal[i].pSetMapped = vkSharedBufferPtr(nodeCstLocal[i].nodeSetBuffer);
		*nodeCstLocal[i].pSetMapped = (MxcNodeCompositorSetState){};

		// thes could be push descriptors???
		VkWriteDescriptorSet writeSets[] = {
			BIND_WRITE_NODE_STATE(nodeCstLocal[i].nodeSet, nodeCstLocal[i].nodeSetBuffer.buf),
		};
		vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);
	}
}

void* mxcCompNodeThread(MxcCompositorContext* pContext)
{
	MxcCompositor compositor = {};
	MxcCompositorCreateInfo info = {
		// compute is always running...
		// really this needs to be a flag
		// these need to all be supported simultaneously
		.compMode = MXC_COMPOSITOR_MODE_QUAD,
//		.mode = MXC_COMPOSITOR_MODE_TESSELATION,
//		.mode = MXC_COMPOSITOR_MODE_COMPUTE,
	};

	vkBeginAllocationRequests();
	mxcCreateCompositor(&info, &compositor);
	vkEndAllocationRequests();

	mxcBindUpdateCompositor(&info, &compositor);

	mxcCompositorNodeRun(pContext, &info, &compositor);

	return NULL;
}