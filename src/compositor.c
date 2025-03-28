#include "compositor.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <stdatomic.h>
#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>

//////////////
//// Constants
////

#define GRID_SUBGROUP_CAPACITY  16
#define GRID_SUBGROUP_SQUARE_SIZE  4
#define GRID_WORKGROUP_SUBGROUP_COUNT 64

///////////
//// Pipes
////

constexpr VkShaderStageFlags MXC_COMPOSITOR_MODE_STAGE_FLAGS[] = {
	[MXC_COMPOSITOR_MODE_BASIC] =       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TASK_MESH] =   VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_COMPUTE] =     VK_SHADER_STAGE_COMPUTE_BIT,
};

//////////////
//// Node Pipe
enum {
	BIND_INDEX_NODE_STATE,
	BIND_INDEX_NODE_COLOR,
	BIND_INDEX_NODE_GBUFFER,
	BIND_INDEX_NODE_COUNT
};
static void CreateNodeSetLayout(MxcCompositorMode mode, VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = BIND_INDEX_NODE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]) {
			[BIND_INDEX_NODE_STATE] = {
				.binding = BIND_INDEX_NODE_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
			},
			[BIND_INDEX_NODE_COLOR] = {
				.binding = BIND_INDEX_NODE_COLOR,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[BIND_INDEX_NODE_GBUFFER] = {
				.binding = BIND_INDEX_NODE_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.linearSampler,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, pLayout));
}
#define BIND_COMPOSITOR_WRITE_STATE(set, buf)                \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstSet = set,                                       \
		.dstBinding = BIND_INDEX_NODE_STATE,                 \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = buf,                                   \
			.range = sizeof(MxcNodeCompositorSetState),      \
		},                                                   \
	}
#define BIND_COMPOSITOR_WRITE_COLOR(set, view, layout)               \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = set,                                               \
		.dstBinding = BIND_INDEX_NODE_COLOR,                         \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = view,                                       \
			.imageLayout = layout,                                   \
		},                                                           \
	}
#define BIND_COMPOSITOR_WRITE_GBUFFER(set, view, layout)             \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = set,                                               \
		.dstBinding = BIND_INDEX_NODE_GBUFFER,                       \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = view,                                       \
			.imageLayout = layout,                                   \
		},                                                           \
	}

enum {
	PIPE_INDEX_NODE_GRAPHICS_GLOBAL,
	PIPE_INDEX_NODE_GRAPHICS_NODE,
	PIPE_INDEX_NODE_GRAPHICS_COUNT,
};
static void CreateGraphicsNodePipeLayout(
	VkDescriptorSetLayout nodeSetLayout,
	VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_INDEX_NODE_GRAPHICS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_INDEX_NODE_GRAPHICS_GLOBAL] = vk.context.basicPipeLayout.globalSetLayout,
			[PIPE_INDEX_NODE_GRAPHICS_NODE]   = nodeSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

//////////////////////
//// Compute Node Pipe
enum {
	BIND_INDEX_NODE_COMPUTE_OUTPUT,
	BIND_INDEX_NODE_COMPUTE_COUNT,
};
static void CreateComputeOutputSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = BIND_INDEX_NODE_COMPUTE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]) {
			[BIND_INDEX_NODE_COMPUTE_OUTPUT] = {
				.binding = BIND_INDEX_NODE_COMPUTE_OUTPUT,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[MXC_COMPOSITOR_MODE_COMPUTE],
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, pLayout));
}
#define BIND_WRITE_COMPUTE_NODE_OUTPUT(set, view)               \
	(VkWriteDescriptorSet) {                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
            .dstSet = set,                                      \
			.dstBinding = BIND_INDEX_NODE_COMPUTE_OUTPUT,       \
			.descriptorCount = 1,                               \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
			.pImageInfo = &(VkDescriptorImageInfo){             \
				.imageView = view,                              \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
	}

enum {
	PIPE_INDEX_NODE_COMPUTE_GLOBAL,
	PIPE_INDEX_NODE_COMPUTE_NODE,
	PIPE_INDEX_NODE_COMPUTE_OUTPUT,
	PIPE_INDEX_NODE_COMPUTE_COUNT,
};
static void CreateComputeNodePipeLayout(
	VkDescriptorSetLayout nodeSetLayout,
	VkDescriptorSetLayout computeOutputSetLayout,
	VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_INDEX_NODE_COMPUTE_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_INDEX_NODE_COMPUTE_GLOBAL] = vk.context.basicPipeLayout.globalSetLayout,
			[PIPE_INDEX_NODE_COMPUTE_NODE]   = nodeSetLayout,
			[PIPE_INDEX_NODE_COMPUTE_OUTPUT] = computeOutputSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

/////////////////////////
//// GBuffer Process Pipe
enum {
	BIND_INDEX_GBUFFER_PROCESS_STATE,
	BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
	BIND_INDEX_GBUFFER_PROCESS_DST,
	BIND_INDEX_GBUFFER_PROCESS_COUNT
};
static void CreateGBufferProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = BIND_INDEX_GBUFFER_PROCESS_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[BIND_INDEX_GBUFFER_PROCESS_STATE] = {
				.binding = BIND_INDEX_GBUFFER_PROCESS_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			[BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH] = {
				.binding = BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[BIND_INDEX_GBUFFER_PROCESS_DST] = {
				.binding = BIND_INDEX_GBUFFER_PROCESS_DST,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 4,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &nodeSetLayoutCreateInfo, VK_ALLOC, pLayout));
}
#define BIND_WRITE_GBUFFER_PROCESS_STATE(view)                     \
	(VkWriteDescriptorSet) {                                       \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                    \
		.dstBinding = BIND_INDEX_GBUFFER_PROCESS_STATE,            \
		.descriptorCount = 1,                                      \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,       \
		.pBufferInfo = &(VkDescriptorBufferInfo){                  \
			.buffer = view,                                        \
			.range = sizeof(GBufferProcessState),                  \
		},                                                         \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(view)                     \
	(VkWriteDescriptorSet) {                                           \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                        \
		.dstBinding = BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,            \
		.descriptorCount = 1,                                          \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,   \
		.pImageInfo = &(VkDescriptorImageInfo){                        \
			.imageView = view,                                         \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                    \
		},                                                             \
	}
#define BIND_WRITE_GBUFFER_PROCESS_DST(mip_views)                    \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
			.dstBinding = BIND_INDEX_GBUFFER_PROCESS_DST,            \
			.descriptorCount = 4,                                    \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,      \
			.pImageInfo = (VkDescriptorImageInfo[]) {                \
			{                                                        \
				.imageView = mip_views[0],                           \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
			},                                                       \
			{                                                        \
				.imageView = mip_views[1],                           \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
			},                                                       \
			{                                                        \
				.imageView = mip_views[2],                           \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
			},                                                       \
			{                                                        \
				.imageView = mip_views[3],                           \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
			},                                                       \
		}                                                            \
	}

enum {
	PIPE_INDEX_GBUFFER_PROCESS_INOUT,
	PIPE_INDEX_GBUFFER_PROCESS_COUNT,
};
static void CreateGBufferProcessPipeLayout(VkDescriptorSetLayout layout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_INDEX_GBUFFER_PROCESS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_INDEX_GBUFFER_PROCESS_INOUT] = layout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

static void CreateGBufferProcessPipe(const char* shaderPath, VkPipelineLayout layout, VkPipeline* pPipe)
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

////////
//// Run
////
void mxcCompositorNodeRun(const MxcCompositorContext* pCtxt, const MxcCompositorCreateInfo* pInfo, const MxcCompositor* pCompositor)
{
	auto dvc = vk.context.device;

	auto cmd = pCtxt->cmd;
	auto timeline = pCtxt->compositorTimeline;
	u64  baseCycleValue = 0;

	auto mode = pInfo->mode;
	auto renderPass = pCompositor->compRenderPass;
	auto framebuffer = pCompositor->graphicsFramebuffer;
	auto globalSet = pCompositor->globalSet;
	auto computeOutputSet = pCompositor->computeOutputSet;

	auto nodePipeLayout = pCompositor->nodePipeLayout;
	auto nodePipe = pCompositor->nodePipe;
	auto computeNodePipeLayout = pCompositor->computeNodePipeLayout;
	auto computeNodePipe = pCompositor->computeNodePipe;

	auto quadOffsets = pCompositor->quadMesh.offsets;
	auto quadBuffer = pCompositor->quadMesh.buffer;

	auto quadPatchOffsets = pCompositor->quadPatchMesh.offsets;
	auto quadPatchBuffer = pCompositor->quadPatchMesh.sharedBuffer.buffer;

	auto timeQueryPool = pCompositor->timeQueryPool;

	auto swap = pCtxt->swap;

	MidCamera globalCam = {
		.yFovRad = RAD_FROM_DEG(45.0f),
		.zNear = 0.1f,
		.zFar = 100.0f,
	};
	MidPose globalCamPose = {
		.position = {0.0f, 0.0f, 2.0f},
		.euler = {0.0f, 0.0f, 0.0f},
	};
	globalCamPose.rotation = QuatFromEuler(globalCamPose.euler);
	auto globalSetState = (VkGlobalSetState){};
	auto pGlobalSetMapped = vkSharedMemoryPtr(pCompositor->globalBuffer.memory);
	vkUpdateGlobalSetViewProj(globalCam, globalCamPose, &globalSetState, pGlobalSetMapped);
	pCompositor->pGBufferProcessMapped->cameraNearZ = globalCam.zNear;
	pCompositor->pGBufferProcessMapped->cameraFarZ = globalCam.zFar;

	struct {
		VkImageView color;
		VkImageView normal;
		VkImageView depth;
	} graphicsFramebufferView;
	graphicsFramebufferView.color = pCompositor->graphicsFramebufferTexture.color.view;
	graphicsFramebufferView.normal = pCompositor->graphicsFramebufferTexture.normal.view;
	graphicsFramebufferView.depth = pCompositor->graphicsFramebufferTexture.depth.view;

	auto graphicsFramebufferColorImage = pCompositor->graphicsFramebufferTexture.color.image;

//	auto computeFramebufferColorImage = pCompositor->computeFramebufferColorTexture.image;

	#define COMPOSITOR_DST_GRAPHICS_READ                                             \
			.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
							VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
							VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
							VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                            \
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

	// Local Barriers
	VkImageMemoryBarrier2 localAcquireBarriers[] = {
		{ // Color
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			COMPOSITOR_DST_GRAPHICS_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{ // Depth
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
		{	// Gbuffer
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
	VkImageMemoryBarrier2 graphicsAcquireBarriers[] = {
		{ // Color
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
		{ // Depth
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
		{	// Gbuffer
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
	VkImageMemoryBarrier2 graphicsProcessFinishBarriers[] = {
		{	// Gbuffer
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
		{ // Color
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
		{ // Depth
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
		{	// Gbuffer
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
	VkImageMemoryBarrier2 computeProcessFinishBarriers[] = {
		{	// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
			VK_IMAGE_BARRIER_DST_COMPUTE_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};

	int compositorFramebufferIndex = 0;

	// just making sure atomics are only using barriers, not locks
	for (int i = 0; i < nodeCount; ++i) {
		assert(__atomic_always_lock_free(sizeof(localNodesShared[i].timelineValue), &localNodesShared[i].timelineValue));
	}

	// very common ones should be global to potentially share higher level cache
	// but maybe do it anyways because it'd just be better? each func pointer is 8 bytes. 8 can fit on a cache line
	VK_DEVICE_FUNC(CmdPipelineBarrier2)
	VK_DEVICE_FUNC(ResetQueryPool)
	VK_DEVICE_FUNC(GetQueryPoolResults)
	VK_DEVICE_FUNC(CmdWriteTimestamp2)
	VK_DEVICE_FUNC(CmdBeginRenderPass)
	VK_DEVICE_FUNC(CmdBindPipeline)
	VK_DEVICE_FUNC(CmdBlitImage)
	VK_DEVICE_FUNC(CmdBindDescriptorSets)
	VK_DEVICE_FUNC(CmdBindVertexBuffers)
	VK_DEVICE_FUNC(CmdBindIndexBuffer)
	VK_DEVICE_FUNC(CmdDrawIndexed)
	VK_DEVICE_FUNC(CmdEndRenderPass)
	VK_DEVICE_FUNC(EndCommandBuffer)
	VK_DEVICE_FUNC(AcquireNextImageKHR)
	VK_DEVICE_FUNC(CmdDrawMeshTasksEXT)
	VK_DEVICE_FUNC(CmdPushDescriptorSetKHR);
	VK_DEVICE_FUNC(CmdDispatch);

run_loop:

	vkTimelineWait(dvc, baseCycleValue + MXC_CYCLE_PROCESS_INPUT, timeline);
	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globalCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globalCamPose);
	vkUpdateGlobalSetView(&globalCamPose, &globalSetState, pGlobalSetMapped);

	{  // Update Nodes
		vkTimelineSignal(dvc, baseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, timeline);
		vkCmdResetBegin(cmd);

		for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
			auto pNodeShrd = activeNodesShared[nodeIndex];

			// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
			// shared: 569 - semaphore: 315416 ratio: 554.333919
			u64 nodeTimelineVal = atomic_load_explicit(&pNodeShrd->timelineValue, memory_order_acquire);

			if (nodeTimelineVal < 1)
				continue;

			auto pNodeCompData = &nodeCompositorData[nodeIndex];

			// We need logic here. Some node you'd want to allow to move themselves. Other locked in specific place. Other move their offset.
			memcpy(&pNodeCompData->rootPose, &pNodeShrd->rootPose, sizeof(MidPose));
			//nodeCompositorData[i].rootPose.rotation = QuatFromEuler(nodeCompositorData[i].rootPose.euler);

			// update node model mat... this should happen every frame so user can move it in comp
			pNodeCompData->nodeSetState.model = Mat4FromPosRot(pNodeCompData->rootPose.position, pNodeCompData->rootPose.rotation);

			if (nodeTimelineVal <= pNodeCompData->lastTimelineValue)
				continue;

			{  // Acquire new framebuffers from node

				int  nodeSwapIndex = !(nodeTimelineVal % VK_SWAP_COUNT);
				auto pNodeSwaps = &pNodeCompData->swaps[nodeSwapIndex];

				VkImageMemoryBarrier2* pAcquireBarriers;
				VkImageMemoryBarrier2* pFinishBarriers;
				switch (pNodeShrd->compositorMode){

					case MXC_COMPOSITOR_MODE_BASIC:
					case MXC_COMPOSITOR_MODE_TESSELATION:
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						pAcquireBarriers = graphicsAcquireBarriers;
						pFinishBarriers = graphicsProcessFinishBarriers;
						break;
					case MXC_COMPOSITOR_MODE_COMPUTE:
						pAcquireBarriers = computeAcquireBarriers;
						pFinishBarriers = computeProcessFinishBarriers;
						break;
					default:
						PANIC("Compositor mode not implemented!");
				}

				pAcquireBarriers[0].image = pNodeSwaps->color;
				pAcquireBarriers[1].image = pNodeSwaps->depth;
				pAcquireBarriers[2].image = pNodeSwaps->gBuffer;
				CmdPipelineImageBarriers2(cmd, 3, pAcquireBarriers);

//				ivec2 extent = {pNodeShared->globalSetState.framebufferSize.x, pNodeShared->globalSetState.framebufferSize.y};
				auto extent = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT};
				int  pixelCount = extent.x * extent.y;
				int  groupCount = pixelCount / GRID_SUBGROUP_CAPACITY / GRID_SUBGROUP_SQUARE_SIZE / GRID_WORKGROUP_SUBGROUP_COUNT;

				memcpy(&pCompositor->pGBufferProcessMapped->depth, (void*)&pNodeShrd->depthState, sizeof(MxcDepthState));
				VkWriteDescriptorSet initialPushSet[] = {
					BIND_WRITE_GBUFFER_PROCESS_STATE(pCompositor->gBufferProcessSetBuffer.buffer),
					BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pNodeSwaps->depthView),
					BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwaps->gBufferMipViews),
				};
				CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pCompositor->gbufferProcessBlitUpPipe);
				CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pCompositor->gbufferProcessPipeLayout, PIPE_INDEX_GBUFFER_PROCESS_INOUT, COUNT(initialPushSet), initialPushSet);
				CmdDispatch(cmd, 1, groupCount, 1);

				pFinishBarriers[0].image = pNodeSwaps->gBuffer;
				CmdPipelineImageBarriers2(cmd, 1, pFinishBarriers);

				VkWriteDescriptorSet writeSets[] = {
					BIND_COMPOSITOR_WRITE_COLOR(pNodeCompData->nodeSet, pNodeSwaps->colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
					BIND_COMPOSITOR_WRITE_GBUFFER(pNodeCompData->nodeSet, pNodeSwaps->gBufferMipViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				};
				vkUpdateDescriptorSets(dvc, COUNT(writeSets), writeSets, 0, NULL);
			}

			pNodeCompData->lastTimelineValue = nodeTimelineVal;

			{  // Calc new node uniform and shared data

				// move the globalSetState that was previously used to render into the nodeSetState to use in comp
				memcpy(&pNodeCompData->nodeSetState.view, (void*)&pNodeShrd->globalSetState, sizeof(VkGlobalSetState));
				pNodeCompData->nodeSetState.ulUV = pNodeShrd->ulClipUV;
				pNodeCompData->nodeSetState.lrUV = pNodeShrd->lrClipUV;

				memcpy(pNodeCompData->pSetMapped, &pNodeCompData->nodeSetState, sizeof(MxcNodeCompositorSetState));

				float radius = pNodeShrd->compositorRadius;

				vec4 ulbModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
				vec4 ulbWorld = Vec4MulMat4(pNodeCompData->nodeSetState.model, ulbModel);
				vec4 ulbClip = Vec4MulMat4(globalSetState.view, ulbWorld);
				vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbClip));
				vec2 ulbUV = Vec2Clamp(UVFromNDC(ulbNDC), 0.0f, 1.0f);

				vec4 ulfModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
				vec4 ulfWorld = Vec4MulMat4(pNodeCompData->nodeSetState.model, ulfModel);
				vec4 ulfClip = Vec4MulMat4(globalSetState.view, ulfWorld);
				vec3 ulfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulfClip));
				vec2 ulfUV = Vec2Clamp(UVFromNDC(ulfNDC), 0.0f, 1.0f);

				vec2 ulUV = Vec2Min(ulfUV, ulbUV);

				vec4 lrbModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
				vec4 lrbWorld = Vec4MulMat4(pNodeCompData->nodeSetState.model, lrbModel);
				vec4 lrbClip = Vec4MulMat4(globalSetState.view, lrbWorld);
				vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrbClip));
				vec2 lrbUV = Vec2Clamp(UVFromNDC(lrbNDC), 0.0f, 1.0f);

				vec4 lrfModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
				vec4 lrfWorld = Vec4MulMat4(pNodeCompData->nodeSetState.model, lrfModel);
				vec4 lrfClip = Vec4MulMat4(globalSetState.view, lrfWorld);
				vec3 lrfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrfClip));
				vec2 lrfUV = Vec2Clamp(UVFromNDC(lrfNDC), 0.0f, 1.0f);

				vec2 lrUV = Vec2Max(lrbUV, lrfUV);

				vec2 diff = {.vec = lrUV.vec - ulUV.vec};

				// maybe I should only copy camera pose info and generate matrix on other thread? oxr only wants the pose
				pNodeShrd->cameraPose = globalCamPose;
				pNodeShrd->camera = globalCam;

				// write current global set state to node's global set state to use for next node render with new the framebuffer size
				memcpy(&pNodeShrd->globalSetState, &globalSetState, sizeof(VkGlobalSetState) - sizeof(ivec2));
				pNodeShrd->globalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
				pNodeShrd->ulClipUV = ulUV;
				pNodeShrd->lrClipUV = lrUV;
				atomic_thread_fence(memory_order_release);
			}
		}

		{  // Recording Cycle
			vkTimelineSignal(dvc, baseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, timeline);

			ResetQueryPool(dvc, timeQueryPool, 0, 2);
			CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

			CmdBeginRenderPass(cmd, renderPass, framebuffer, VK_PASS_CLEAR_COLOR,
							   graphicsFramebufferView.color,
							   graphicsFramebufferView.normal,
							   graphicsFramebufferView.depth);

			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipe);
			CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_GLOBAL, 1, &globalSet, 0, NULL);

			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeNodePipe);
			CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_GLOBAL, 1, &globalSet, 0, NULL);
			CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_OUTPUT, 1, &computeOutputSet, 0, NULL);

			for (int i = 0; i < nodeCount; ++i) {
				MxcNodeShared* pNodeShared = activeNodesShared[i];

				// find a way to get rid of this load and check
				uint64_t nodeCurrentTimelineSignal = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);;

				// I don't like this but it waits until the node renders something. Prediction should be okay here.
				if (nodeCurrentTimelineSignal < 1)
					continue;

				// these should be different 'active' arrays so all of a similiar type can run at once and we dont have to switch
				switch (mode) {
					case MXC_COMPOSITOR_MODE_BASIC:
						CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositorData[i].nodeSet, 0, NULL);
						CmdBindVertexBuffers(cmd, 0, 1, (VkBuffer[]){quadBuffer}, (VkDeviceSize[]){quadOffsets.vertexOffset});
						CmdBindIndexBuffer(cmd, quadBuffer, 0, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(cmd, quadOffsets.indexCount, 1, 0, 0, 0);
						break;
					case MXC_COMPOSITOR_MODE_TESSELATION:
						CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositorData[i].nodeSet, 0, NULL);
						CmdBindVertexBuffers(cmd, 0, 1, (VkBuffer[]){quadPatchBuffer}, (VkDeviceSize[]){quadPatchOffsets.vertexOffset});
						CmdBindIndexBuffer(cmd, quadPatchBuffer, quadPatchOffsets.indexOffset, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(cmd, quadPatchOffsets.indexCount, 1, 0, 0, 0);
						break;
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositorData[i].nodeSet, 0, NULL);
						CmdDrawMeshTasksEXT(cmd, 1, 1, 1);
						break;
					case MXC_COMPOSITOR_MODE_COMPUTE:
						CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_NODE, 1, &nodeCompositorData[i].nodeSet, 0, NULL);

						// can be saved
						ivec2 extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT};
						int pixelCount = extent.x * extent.y;
						int groupCount = pixelCount / GRID_SUBGROUP_CAPACITY / GRID_SUBGROUP_SQUARE_SIZE / GRID_WORKGROUP_SUBGROUP_COUNT;

						CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeNodePipe);
						CmdDispatch(cmd, 1, groupCount, 1);

						break;
					default: PANIC("CompMode not supported!");
				}
			}
		}

		CmdEndRenderPass(cmd);

		CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

		{  // Blit Framebuffer
			u32 swapIndex; AcquireNextImageKHR(dvc, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &swapIndex);
			atomic_store_explicit(&compositorContext.swapIndex, swapIndex, memory_order_release);
			VkImage swapImage = swap.images[compositorContext.swapIndex];

			VkImageMemoryBarrier2 beginBlitBarrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = swapImage,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT,
				VK_IMAGE_BARRIER_DST_BLIT,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			};
			CmdPipelineImageBarrier2(cmd, &beginBlitBarrier);

			CmdBlitImageFullScreen(cmd, graphicsFramebufferColorImage, swapImage);

			VkImageMemoryBarrier2 endBlitBarrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = swapImage,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_BLIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			};
			CmdPipelineImageBarrier2(cmd, &endBlitBarrier);
		}

		EndCommandBuffer(cmd);

		vkTimelineSignal(dvc, baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, timeline);

		baseCycleValue += MXC_CYCLE_COUNT;

		{ // wait for end and output query, probably don't need this wait if not querying?
			vkTimelineWait(dvc, baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, timeline);

			{  // update timequery
				uint64_t timestampsNS[2];
				GetQueryPoolResults(dvc, timeQueryPool, 0, 2, sizeof(uint64_t) * 2, timestampsNS, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
				double timestampsMS[2];
				for (uint32_t i = 0; i < 2; ++i) {
					timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
				}
				timeQueryMs = timestampsMS[1] - timestampsMS[0];
			}
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
	{   // Create

		{	// Graphics Pipes
			CreateNodeSetLayout(pInfo->mode, &pCompositor->nodeSetLayout);
			CreateGraphicsNodePipeLayout(pCompositor->nodeSetLayout, &pCompositor->nodePipeLayout);

			// we need all of these modes available, this should be flags
			switch (pInfo->mode) {
				case MXC_COMPOSITOR_MODE_BASIC:
					vkCreateBasicPipe("./shaders/basic_comp.vert.spv",
									  "./shaders/basic_comp.frag.spv",
									  vk.context.renderPass,
									  pCompositor->nodePipeLayout,
									  &pCompositor->nodePipe);
					vkCreateQuadMesh(0.5f, &pCompositor->quadMesh);
					break;
				case MXC_COMPOSITOR_MODE_TESSELATION:
					vkCreateBasicTessellationPipe("./shaders/tess_comp.vert.spv",
												  "./shaders/tess_comp.tesc.spv",
												  "./shaders/tess_comp.tese.spv",
												  "./shaders/tess_comp.frag.spv",
												  vk.context.renderPass,
												  pCompositor->nodePipeLayout,
												  &pCompositor->nodePipe);
					vkCreateQuadPatchMeshSharedMemory(&pCompositor->quadPatchMesh);
					break;
				case MXC_COMPOSITOR_MODE_TASK_MESH:
					vkCreateBasicTaskMeshPipe("./shaders/mesh_comp.task.spv",
											  "./shaders/mesh_comp.mesh.spv",
											  "./shaders/mesh_comp.frag.spv",
											  vk.context.renderPass,
											  pCompositor->nodePipeLayout,
											  &pCompositor->nodePipe);
					vkCreateQuadMesh(0.5f, &pCompositor->quadMesh);
					break;
				default: PANIC("Compositor mode not supported!");
			}
		}

		{ // Compute Pipe
			CreateNodeSetLayout(MXC_COMPOSITOR_MODE_COMPUTE, &pCompositor->computeNodeSetLayout);
			CreateComputeOutputSetLayout(&pCompositor->computeOutputSetLayout);
			CreateComputeNodePipeLayout(pCompositor->computeNodeSetLayout,
										pCompositor->computeOutputSetLayout,
										&pCompositor->computeNodePipeLayout);
			vkCreateComputePipe("./shaders/compute_compositor.comp.spv",
								pCompositor->computeNodePipeLayout,
								&pCompositor->computeNodePipe);
		}

		{ // Pools
			VkQueryPoolCreateInfo queryInfo = {
				VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
				.queryType = VK_QUERY_TYPE_TIMESTAMP,
				.queryCount = 2,
			};
			VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, VK_ALLOC, &pCompositor->timeQueryPool));

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

		{  // Global
			vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.globalSetLayout, &pCompositor->globalSet);
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(VkGlobalSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateSharedBuffer(&requestInfo, &pCompositor->globalBuffer);
		}

		{ // GBuffer Process
			CreateGBufferProcessSetLayout(&pCompositor->gbufferProcessSetLayout);
			CreateGBufferProcessPipeLayout(pCompositor->gbufferProcessSetLayout, &pCompositor->gbufferProcessPipeLayout);
			CreateGBufferProcessPipe("./shaders/compositor_gbuffer_blit_up.comp.spv", pCompositor->gbufferProcessPipeLayout, &pCompositor->gbufferProcessBlitUpPipe);

			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(GBufferProcessState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateSharedBuffer(&requestInfo, &pCompositor->gBufferProcessSetBuffer);
		}

		// Node Sets
		// Preallocate all Node Set buffers. MxcNodeCompositorSetState * 256 = 130 kb. Small price to pay to ensure contiguous memory on GPU
		// TODO this should be a descriptor array
		for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(MxcNodeCompositorSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateSharedBuffer(&requestInfo, &nodeCompositorData[i].nodeSetBuffer);
			VkDescriptorSetAllocateInfo setInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = threadContext.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &pCompositor->nodeSetLayout,
			};
			VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &nodeCompositorData[i].nodeSet));
			vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)nodeCompositorData[i].nodeSet, "NodeSet");
		}

		{ // Graphics Framebuffers
			VkBasicFramebufferTextureCreateInfo framebufferTextureInfo = {
				.debugName = "CompositeFramebufferTexture",
				.locality = VK_LOCALITY_CONTEXT,
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
			};
			vkCreateBasicFramebufferTextures(&framebufferTextureInfo, &pCompositor->graphicsFramebufferTexture);
			VkBasicFramebufferCreateInfo framebufferInfo = {
				.debugName = "CompositeFramebuffer",
				.renderPass = vk.context.renderPass,
			};
			vkCreateBasicFramebuffer(&framebufferInfo, &pCompositor->graphicsFramebuffer);
		}

		{ // Compute Output

			VkTextureCreateInfo colorCreateInfo = {
				.debugName = "ComputeColorFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_R8G8B8A8_UNORM,
					.extent =  {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_STORAGE_BIT,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT,
			};
			vkCreateTexture(&colorCreateInfo, &pCompositor->computeFramebufferColorTexture);

			VkDescriptorSetAllocateInfo setInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = threadContext.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &pCompositor->computeOutputSetLayout,
			};
			VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &pCompositor->computeOutputSet));
			vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pCompositor->computeOutputSet, "ComputeOutputSet");

			VkWriteDescriptorSet writeSets[] = {
				BIND_WRITE_COMPUTE_NODE_OUTPUT(pCompositor->computeOutputSet, pCompositor->computeFramebufferColorTexture.view),
			};
			vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);
		}
	}

	{  // Copy needed state
		pCompositor->device = vk.context.device;
		pCompositor->compRenderPass = vk.context.renderPass;
	}
}

void mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp)
{
	switch (pInfo->mode) {
		case MXC_COMPOSITOR_MODE_BASIC:
			break;
		case MXC_COMPOSITOR_MODE_TESSELATION:
			vkBindUpdateQuadPatchMesh(0.5f, &pComp->quadPatchMesh);
			break;
		case MXC_COMPOSITOR_MODE_TASK_MESH:
			break;
		default: PANIC("CompMode not supported!");
	}

	vkBindSharedBuffer(&pComp->globalBuffer);

	vkBindSharedBuffer(&pComp->gBufferProcessSetBuffer);
	pComp->pGBufferProcessMapped = vkSharedBufferPtr(pComp->gBufferProcessSetBuffer);
	*pComp->pGBufferProcessMapped = (GBufferProcessState){};

	vkUpdateDescriptorSets(vk.context.device, 1, &VK_BIND_WRITE_GLOBAL_BUFFER(pComp->globalSet, pComp->globalBuffer.buffer), 0, NULL);
	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// should I make them all share the same buffer? probably
		// should also share the same descriptor set with an array
		vkBindSharedBuffer(&nodeCompositorData[i].nodeSetBuffer);
		nodeCompositorData[i].pSetMapped = vkSharedBufferPtr(nodeCompositorData[i].nodeSetBuffer);
		*nodeCompositorData[i].pSetMapped = (MxcNodeCompositorSetState){};

		VkWriteDescriptorSet writeSets[] = {
			BIND_COMPOSITOR_WRITE_STATE(nodeCompositorData[i].nodeSet, nodeCompositorData[i].nodeSetBuffer.buffer),
		};
		vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);
	}
}

void* mxcCompNodeThread(MxcCompositorContext* pContext)
{
	MxcCompositor compositor;
	MxcCompositorCreateInfo info = {
//		.mode = MXC_COMPOSITOR_MODE_BASIC,
		.mode = MXC_COMPOSITOR_MODE_TESSELATION,
//		.mode = MXC_COMPOSITOR_MODE_COMPUTE,
	};

	vkBeginAllocationRequests();
	mxcCreateCompositor(&info, &compositor);
	vkEndAllocationRequests();

	mxcBindUpdateCompositor(&info, &compositor);

	mxcCompositorNodeRun(pContext, &info, &compositor);

	return NULL;
}