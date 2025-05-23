#include "compositor.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <stdatomic.h>
#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>

#define VkDescriptorSetLayoutCreateInfo(...) (VkDescriptorSetLayoutCreateInfo) {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, __VA_ARGS__ }
#define VkDescriptorSetLayoutBindingElement(bind_index, ...) [bind_index] = { .binding = bind_index, __VA_ARGS__ }

//////////////
//// Constants
////

#define GRID_SUBGROUP_CAPACITY  16
// only divide this in if you are going to deal with quad samples and texture gather per thread
#define GRID_SUBGROUP_SQUARE_SIZE  4
#define GRID_WORKGROUP_SUBGROUP_COUNT 64

//////////
//// Pipes
////

constexpr VkShaderStageFlags MXC_COMPOSITOR_MODE_STAGE_FLAGS[] = {
	[MXC_COMPOSITOR_MODE_BASIC] =       VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TASK_MESH] =   VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
//	[MXC_COMPOSITOR_MODE_COMPUTE] =     VK_SHADER_STAGE_COMPUTE_BIT,
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
	auto info = VkDescriptorSetLayoutCreateInfo(
			.bindingCount = BIND_INDEX_NODE_COUNT,
			.pBindings = (VkDescriptorSetLayoutBinding[]){
				VkDescriptorSetLayoutBindingElement(
					BIND_INDEX_NODE_STATE,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode]),
				VkDescriptorSetLayoutBindingElement(
					BIND_INDEX_NODE_COLOR,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
					.pImmutableSamplers = &vk.context.linearSampler),
				VkDescriptorSetLayoutBindingElement(
					BIND_INDEX_NODE_GBUFFER,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = MXC_COMPOSITOR_MODE_STAGE_FLAGS[mode],
					.pImmutableSamplers = &vk.context.linearSampler),
			});
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, pLayout));
}
#define BIND_WRITE_NODE_STATE(set, buf)                      \
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
#define BIND_WRITE_NODE_COLOR(set, view, layout)                     \
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
#define BIND_WRITE_NODE_GBUFFER(set, view, layout)                   \
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
	BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
	BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
	BIND_INDEX_NODE_COMPUTE_COUNT,
};
static void CreateComputeOutputSetLayout(VkDescriptorSetLayout* pLayout)
{
	auto info = VkDescriptorSetLayoutCreateInfo(
			.bindingCount = BIND_INDEX_NODE_COMPUTE_COUNT,
			.pBindings = (VkDescriptorSetLayoutBinding[]){
				VkDescriptorSetLayoutBindingElement(
					BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT),
				VkDescriptorSetLayoutBindingElement(
					BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT),
			});
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, pLayout));
}
#define BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(set, view)         \
	(VkWriteDescriptorSet) {                                     \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
            .dstSet = set,                                       \
			.dstBinding = BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT, \
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
			.dstBinding = BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT, \
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
static void CreateNodeComputePipeLayout(
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

////////
//// Run
////
void mxcCompositorNodeRun(const MxcCompositorContext* pCtx, const MxcCompositorCreateInfo* pInfo, const MxcCompositor* pCompst)
{
	auto device = vk.context.device;

	auto graphCmd = pCtx->graphicsCmd;
	auto timeln = pCtx->compositorTimeline;
	u64  baseCycleValue = 0;

	auto mode = pInfo->mode;
	auto renderPass = pCompst->compRenderPass;
	auto framebuff = pCompst->graphicsFramebuffer;
	auto globalSet = pCompst->globalSet;
	auto comptOutputSet = pCompst->computeOutputSet;

	auto nodePipeLayout = pCompst->nodePipeLayout;
	auto nodePipe = pCompst->nodePipe;
	auto comptNodePipeLayout = pCompst->computeNodePipeLayout;
	auto comptNodePipe = pCompst->computeNodePipe;
	auto comptPostNodePipe = pCompst->computePostNodePipe;

	auto quadOffsets = pCompst->quadMesh.offsets;
	auto quadBuffer = pCompst->quadMesh.buffer;

	auto quadPatchOffsets = pCompst->quadPatchMesh.offsets;
	auto quadPatchBuffer = pCompst->quadPatchMesh.sharedBuffer.buffer;

	auto timeQueryPool = pCompst->timeQueryPool;

	auto swap = pCtx->swap;

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
	auto globSetState = (VkGlobalSetState){};
	auto pGlobSetMapped = vkSharedMemoryPtr(pCompst->globalBuffer.memory);
	vkUpdateGlobalSetViewProj(globCam, globCamPose, &globSetState, pGlobSetMapped);
	pCompst->pGBufferProcessMapped->cameraNearZ = globCam.zNear;
	pCompst->pGBufferProcessMapped->cameraFarZ = globCam.zFar;

	struct {
		VkImageView color;
		VkImageView normal;
		VkImageView depth;
	} graphFramebuffView;
	graphFramebuffView.color = pCompst->graphicsFramebufferTexture.color.view;
	graphFramebuffView.normal = pCompst->graphicsFramebufferTexture.normal.view;
	graphFramebuffView.depth = pCompst->graphicsFramebufferTexture.depth.view;

	auto grphFrmBufColorImg = pCompst->graphicsFramebufferTexture.color.image;
	auto cmptFrmBufColorImg = pCompst->computeFramebufferColorTexture.image;

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
	VkImageMemoryBarrier2 graphicAcquireBarriers[] = {
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
	VkImageMemoryBarrier2 graphicProcessFinishBarriers[] = {
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
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
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

	// Make sure atomics are only using barriers, not locks
	for (int i = 0; i < nodeCt; ++i) {
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

	vkTimelineWait(device, baseCycleValue + MXC_CYCLE_PROCESS_INPUT, timeln);
	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globCamPose);
	vkUpdateGlobalSetView(&globCamPose, &globSetState, pGlobSetMapped);

	{  // Update Nodes
		vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, timeln);
		vkCmdResetBegin(graphCmd);

		for (int iNode = 0; iNode < nodeCt; ++iNode) {
			auto pNodeShared = activeNodesShared[iNode];

			// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
			// shared: 569 - semaphore: 315416 ratio: 554.333919
			u64 nodeTimlnVal = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);

			if (nodeTimlnVal < 1)
				continue;

			auto pNodeCompositData = &nodeCompositData[iNode];

			// We need logic here. Some node you'd want to allow to move themselves. Other locked in specific place. Other move their offset.
			memcpy(&pNodeCompositData->rootPose, &pNodeShared->rootPose, sizeof(MidPose));
			//nodeCompositorData[i].rootPose.rotation = QuatFromEuler(nodeCompositorData[i].rootPose.euler);

			// update node model mat... this should happen every frame so user can move it in comp
			pNodeCompositData->nodeSetState.model = Mat4FromPosRot(pNodeCompositData->rootPose.position, pNodeCompositData->rootPose.rotation);

			if (nodeTimlnVal <= pNodeCompositData->lastTimelineValue)
				continue;

			{  // Acquire new framebuffers from node

				int  iNodeSwap = !(nodeTimlnVal % VK_SWAP_COUNT);
				auto pNodeSwap = &pNodeCompositData->swaps[iNodeSwap];

				VkImageMemoryBarrier2* pAcqBars;
				VkImageMemoryBarrier2* pFinBars;
				VkImageLayout finalBarrier;
				switch (pNodeShared->compositorMode){

					case MXC_COMPOSITOR_MODE_BASIC:
					case MXC_COMPOSITOR_MODE_TESSELATION:
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						pAcqBars = graphicAcquireBarriers;
						pFinBars = graphicProcessFinishBarriers;
						finalBarrier = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						break;
					case MXC_COMPOSITOR_MODE_COMPUTE:
						pAcqBars = computeAcquireBarriers;
						pFinBars = computeProcessFinishBarriers;
						finalBarrier = VK_IMAGE_LAYOUT_GENERAL;
						break;
					default:
						PANIC("Compositor mode not implemented!");
				}

				pAcqBars[0].image = pNodeSwap->color;
				pAcqBars[1].image = pNodeSwap->depth;
				pAcqBars[2].image = pNodeSwap->gBuffer;
				CmdPipelineImageBarriers2(graphCmd, 3, pAcqBars);

//				ivec2 extent = {pNodeShared->globalSetState.framebufferSize.x, pNodeShared->globalSetState.framebufferSize.y};
				ivec2 extent = IVEC2(DEFAULT_WIDTH, DEFAULT_HEIGHT);
				u32   pixelCt = extent.x * extent.y;
				u32  groupCt = pixelCt / GRID_SUBGROUP_CAPACITY  / GRID_WORKGROUP_SUBGROUP_COUNT / GRID_SUBGROUP_SQUARE_SIZE;

				memcpy(&pCompst->pGBufferProcessMapped->depth, (void*)&pNodeShared->depthState, sizeof(MxcDepthState));
				VkWriteDescriptorSet pshSets[] = {
					BIND_WRITE_GBUFFER_PROCESS_STATE(pCompst->gBufferProcessSetBuffer.buffer),
					BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pNodeSwap->depthView),
					BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwap->gBufferMipViews),
				};
				CmdBindPipeline(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pCompst->gbufferProcessBlitUpPipe);
				CmdPushDescriptorSetKHR(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pCompst->gbufferProcessPipeLayout, PIPE_INDEX_GBUFFER_PROCESS_INOUT, COUNT(pshSets), pshSets);
				CmdDispatch(graphCmd, 1, groupCt, 1);

				pFinBars[0].image = pNodeSwap->gBuffer;
				CmdPipelineImageBarriers2(graphCmd, 1, pFinBars);

				VkWriteDescriptorSet wrtSets[] = {
					BIND_WRITE_NODE_COLOR(pNodeCompositData->nodeSet, pNodeSwap->colorView, finalBarrier),
					BIND_WRITE_NODE_GBUFFER(pNodeCompositData->nodeSet, pNodeSwap->gBufferMipViews[0], finalBarrier),
				};
				vkUpdateDescriptorSets(device, COUNT(wrtSets), wrtSets, 0, NULL);
			}

			pNodeCompositData->lastTimelineValue = nodeTimlnVal;

			{  // Calc new node uniform and shared data

				// move the globalSetState that was previously used to render into the nodeSetState to use in comp
				memcpy(&pNodeCompositData->nodeSetState.view, (void*)&pNodeShared->globalSetState, sizeof(VkGlobalSetState));
				pNodeCompositData->nodeSetState.ulUV = pNodeShared->ulClipUV;
				pNodeCompositData->nodeSetState.lrUV = pNodeShared->lrClipUV;

				memcpy(pNodeCompositData->pSetMapped, &pNodeCompositData->nodeSetState, sizeof(MxcNodeCompositorSetState));

				float radius = pNodeShared->compositorRadius;

				vec4 ulbModel = Vec4Rot(globCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
				vec4 ulbWorld = Vec4MulMat4(pNodeCompositData->nodeSetState.model, ulbModel);
				vec4 ulbClip = Vec4MulMat4(globSetState.view, ulbWorld);
				vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globSetState.proj, ulbClip));
				vec2 ulbUV = UVFromNDC(ulbNDC);

				vec4 ulfModel = Vec4Rot(globCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
				vec4 ulfWorld = Vec4MulMat4(pNodeCompositData->nodeSetState.model, ulfModel);
				vec4 ulfClip = Vec4MulMat4(globSetState.view, ulfWorld);
				vec3 ulfNDC = Vec4WDivide(Vec4MulMat4(globSetState.proj, ulfClip));
				vec2 ulfUV = UVFromNDC(ulfNDC);

				vec2 ulUV = ulfNDC.z < 0 ? ulbUV : Vec2Min(ulfUV, ulbUV);
				vec2 ulUVClamp = Vec2Clamp(ulUV, 0.0f, 1.0f);

				vec4 lrbModel = Vec4Rot(globCamPose.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
				vec4 lrbWorld = Vec4MulMat4(pNodeCompositData->nodeSetState.model, lrbModel);
				vec4 lrbClip = Vec4MulMat4(globSetState.view, lrbWorld);
				vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globSetState.proj, lrbClip));
				vec2 lrbUV = UVFromNDC(lrbNDC);

				vec4 lrfModel = Vec4Rot(globCamPose.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
				vec4 lrfWorld = Vec4MulMat4(pNodeCompositData->nodeSetState.model, lrfModel);
				vec4 lrfClip = Vec4MulMat4(globSetState.view, lrfWorld);
				vec3 lrfNDC = Vec4WDivide(Vec4MulMat4(globSetState.proj, lrfClip));
				vec2 lrfUV = UVFromNDC(lrfNDC);

				vec2 lrUV = lrfNDC.z < 0 ? lrbUV : Vec2Max(lrbUV, lrfUV);
				vec2 lrUVClamp = Vec2Clamp(lrUV, 0.0f, 1.0f);

				vec2 diff = {.vec = lrUVClamp.vec - ulUVClamp.vec};

//				printf("%f  %f   %f  %f  \n", ulbNDC.z, ulfNDC.z,  lrbNDC.z, lrfNDC.z);

				// maybe I should only copy camera pose info and generate matrix on other thread? oxr only wants the pose
				pNodeShared->cameraPose = globCamPose;
				pNodeShared->camera = globCam;

				// write current global set state to node's global set state to use for next node render with new the framebuffer size
				memcpy(&pNodeShared->globalSetState, &globSetState, sizeof(VkGlobalSetState) - sizeof(ivec2));
				pNodeShared->globalSetState.framebufferSize = IVEC2(diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT);
				pNodeShared->ulClipUV = ulUV;
				pNodeShared->lrClipUV = lrUV;
				atomic_thread_fence(memory_order_release);
			}
		}

		// this needs far better solution
		bool graphicsBlit = false;
		bool computeBlit = false;

		{  // Graphics Recording Cycle
			vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, timeln);

			CmdBeginRenderPass(graphCmd, renderPass, framebuff, VK_PASS_CLEAR_COLOR,
							   graphFramebuffView.color,
							   graphFramebuffView.normal,
							   graphFramebuffView.depth);

			CmdBindPipeline(graphCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipe);
			CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_GLOBAL, 1, &globalSet, 0, NULL);

			for (int i = 0; i < nodeCt; ++i) {
				auto pNodShrd = activeNodesShared[i];

				// find a way to get rid of this load and check
				u64 nodTimlnVal = atomic_load_explicit(&pNodShrd->timelineValue, memory_order_acquire);

				// I don't like this but it waits until the node renders something. Prediction should be okay here.
				if (nodTimlnVal < 1)
					continue;

				// these should be different 'active' arrays so all of a similiar type can run at once and we dont have to switch
				switch (pNodShrd->compositorMode) {
					case MXC_COMPOSITOR_MODE_BASIC:
						CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositData[i].nodeSet, 0, NULL);
						CmdBindVertexBuffers(graphCmd, 0, 1, (VkBuffer[]){quadBuffer}, (VkDeviceSize[]){quadOffsets.vertexOffset});
						CmdBindIndexBuffer(graphCmd, quadBuffer, 0, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(graphCmd, quadOffsets.indexCount, 1, 0, 0, 0);
						graphicsBlit = true;
						break;
					case MXC_COMPOSITOR_MODE_TESSELATION:
						CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositData[i].nodeSet, 0, NULL);
						CmdBindVertexBuffers(graphCmd, 0, 1, (VkBuffer[]){quadPatchBuffer}, (VkDeviceSize[]){quadPatchOffsets.vertexOffset});
						CmdBindIndexBuffer(graphCmd, quadPatchBuffer, quadPatchOffsets.indexOffset, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(graphCmd, quadPatchOffsets.indexCount, 1, 0, 0, 0);
						graphicsBlit = true;
						break;
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_INDEX_NODE_GRAPHICS_NODE, 1, &nodeCompositData[i].nodeSet, 0, NULL);
						CmdDrawMeshTasksEXT(graphCmd, 1, 1, 1);
						graphicsBlit = true;
						break;
					default:
						break;
				}
			}

			CmdEndRenderPass(graphCmd);
		}

		ResetQueryPool(device, timeQueryPool, 0, 2);
		CmdWriteTimestamp2(graphCmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

		{  // Compute Recording Cycle. We really must separate into Compute and Graphics lists
			CmdBindPipeline(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptNodePipe);
			CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_GLOBAL, 1, &globalSet, 0, NULL);
			CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_OUTPUT, 1, &comptOutputSet, 0, NULL);

//			vkCmdClearColorImage(grphCmd, cmptFrmBufColorImg, VK_IMAGE_LAYOUT_GENERAL, &(VkClearColorValue){0.0f, 0.0f, 0.0f, 0.0f}, 1, &VK_COLOR_SUBRESOURCE_RANGE);

			for (int iNode = 0; iNode < nodeCt; ++iNode) {
				auto pNodShrd = activeNodesShared[iNode];

				// find a way to get rid of this load and check
				u64 nodeTimlnVal = atomic_load_explicit(&pNodShrd->timelineValue, memory_order_acquire);

				// I don't like this but it waits until the node renders something. Prediction should be okay here.
				if (nodeTimlnVal < 1)
					continue;

				auto pNodeCompstData = &nodeCompositData[iNode];
				int  iNodeSwap = !(nodeTimlnVal % VK_SWAP_COUNT);
				auto pNodeSwap = &pNodeCompstData->swaps[iNodeSwap];

				// these should be different 'active' arrays so all of a similiar type can run at once and we dont have to switch
				// really all the nodes need to be set in UBO array and the compute shader do this loop
				switch (pNodShrd->compositorMode) {
					case MXC_COMPOSITOR_MODE_COMPUTE:
						CmdBindDescriptorSets(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptNodePipeLayout, PIPE_INDEX_NODE_COMPUTE_NODE, 1, &nodeCompositData[iNode].nodeSet, 0, NULL);

						// can be saved
						ivec2 extent = IVEC2(DEFAULT_WIDTH, DEFAULT_HEIGHT);
						int   pixelCt = extent.x * extent.y;
						int   groupCt = pixelCt / GRID_SUBGROUP_CAPACITY / GRID_WORKGROUP_SUBGROUP_COUNT;
//						int   quadGroupCt = pixlCt / GRID_SUBGROUP_CAPACITY / GRID_WORKGROUP_SUBGROUP_COUNT / GRID_SUBGROUP_SQUARE_SIZE;

						CmdBindPipeline(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptNodePipe);
						CmdDispatch(graphCmd, 1, groupCt, 1);

						VkImageMemoryBarrier2 postComptBarrier[] = {
							{
								VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
								.image = pNodeSwap->color,
								VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
								VK_IMAGE_BARRIER_DST_COMPUTE_READ,
								VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
								VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
							},
							{
								VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
								.image = pNodeSwap->gBuffer,
								VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
								VK_IMAGE_BARRIER_DST_COMPUTE_READ,
								VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
								VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
							},
						};
						CmdPipelineImageBarriers2(graphCmd, COUNT(postComptBarrier), postComptBarrier);

						CmdBindPipeline(graphCmd, VK_PIPELINE_BIND_POINT_COMPUTE, comptPostNodePipe);
						CmdDispatch(graphCmd, 1, groupCt, 1);

						computeBlit = true;
						break;
					default:
						break;
				}
			}
		}

		// should have separate compute and graphics queries
		CmdWriteTimestamp2(graphCmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

		{  // Blit Framebuffer
			u32 swapIndex; AcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &swapIndex);
			atomic_store_explicit(&compositorContext.swapIndex, swapIndex, memory_order_release);
			VkImage swapImage = swap.images[compositorContext.swapIndex];

			VkImageMemoryBarrier2 beginBlitBarrier[] = {
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = swapImage,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT,
					VK_IMAGE_BARRIER_DST_BLIT_WRITE,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = cmptFrmBufColorImg,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_DST_BLIT_READ,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
			};
			CmdPipelineImageBarriers2(graphCmd, COUNT(beginBlitBarrier), beginBlitBarrier);

			if (graphicsBlit)
				CmdBlitImageFullScreen(graphCmd, grphFrmBufColorImg, swapImage);
			if (computeBlit)
				CmdBlitImageFullScreen(graphCmd, cmptFrmBufColorImg, swapImage);

			VkImageMemoryBarrier2 endBlitBarrier[] = {
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = swapImage,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_BLIT_WRITE,
					.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT  ,
					.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
					.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = cmptFrmBufColorImg,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
					VK_IMAGE_BARRIER_SRC_BLIT_READ,
					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				},
			};
			CmdPipelineImageBarriers2(graphCmd, COUNT(endBlitBarrier), endBlitBarrier);
		}

		EndCommandBuffer(graphCmd);

		vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, timeln);

		baseCycleValue += MXC_CYCLE_COUNT;

		{ // wait for end and output query, probably don't need this wait if not querying?
			vkTimelineWait(device, baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, timeln);

			{  // update timequery
				uint64_t timestampsNS[2];
				GetQueryPoolResults(device, timeQueryPool, 0, 2, sizeof(uint64_t) * 2, timestampsNS, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
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
//			CreateNodeSetLayout(MXC_COMPOSITOR_MODE_COMPUTE, &pCompositor->computeNodeSetLayout);
			CreateComputeOutputSetLayout(&pCompositor->computeOutputSetLayout);
			CreateNodeComputePipeLayout(pCompositor->nodeSetLayout,
										pCompositor->computeOutputSetLayout,
										&pCompositor->computeNodePipeLayout);
			vkCreateComputePipe("./shaders/compute_compositor.comp.spv",
								pCompositor->computeNodePipeLayout,
								&pCompositor->computeNodePipe);
			vkCreateComputePipe("./shaders/compute_post_compositor.comp.spv",
								pCompositor->computeNodePipeLayout,
								&pCompositor->computePostNodePipe);
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
			vkCreateComputePipe("./shaders/compositor_gbuffer_blit_up.comp.spv", pCompositor->gbufferProcessPipeLayout, &pCompositor->gbufferProcessBlitUpPipe);

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
			vkCreateSharedBuffer(&requestInfo, &nodeCompositData[i].nodeSetBuffer);
			VkDescriptorSetAllocateInfo setInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = threadContext.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &pCompositor->nodeSetLayout,
			};
			VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &nodeCompositData[i].nodeSet));
			vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)nodeCompositData[i].nodeSet, "NodeSet");
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
			{
				// I still don't entirely hate this....
#define VkImageCreateInfo(...) (VkImageCreateInfo) { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, __VA_ARGS__ }

				VkTextureCreateInfo atomicCreateInfo = {
					.debugName = "ComputeAtomicFramebuffer",
					.pImageCreateInfo = &VkImageCreateInfo(
							.imageType   = VK_IMAGE_TYPE_2D,
							.format      = VK_FORMAT_R32_UINT,
							.extent      = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
							.mipLevels   = 1,
							.arrayLayers = 1,
							.samples     = VK_SAMPLE_COUNT_1_BIT,
							.usage       = VK_IMAGE_USAGE_STORAGE_BIT),
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.locality   = VK_LOCALITY_CONTEXT,
				};
				vkCreateTexture(&atomicCreateInfo, &pCompositor->computeFramebufferAtomicTexture);

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
				vkCreateTexture(&colorCreateInfo, &pCompositor->computeFramebufferColorTexture);

				VkCommandBuffer cmd = vkBeginImmediateCommandBuffer(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS);

#define VkImageMemoryBarrier2(...) (VkImageMemoryBarrier2) { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, __VA_ARGS__ }
				VkImageMemoryBarrier2 barrier[] = {
					VkImageMemoryBarrier2(
							.image = pCompositor->computeFramebufferAtomicTexture.image,
							.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
							VK_IMAGE_BARRIER_SRC_UNDEFINED,
							VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
							VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED),
					VkImageMemoryBarrier2(
							.image = pCompositor->computeFramebufferColorTexture.image,
							.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
							VK_IMAGE_BARRIER_SRC_UNDEFINED,
							VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
							VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED),
				};
				vkCmdImageBarriers(cmd, COUNT(barrier), barrier);
				vkEndImmediateCommandBuffer(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, cmd);
			}


			{
				// I still don't entirely hate this....
#define VkDescriptorSetAllocateInfo(...) (VkDescriptorSetAllocateInfo) { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, __VA_ARGS__ }

				auto setInfo = VkDescriptorSetAllocateInfo(
						.descriptorPool = threadContext.descriptorPool,
						.descriptorSetCount = 1,
						.pSetLayouts = &pCompositor->computeOutputSetLayout);
				VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &pCompositor->computeOutputSet));
				vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pCompositor->computeOutputSet, "ComputeOutputSet");

				VkWriteDescriptorSet writeSets[] = {
					BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(pCompositor->computeOutputSet, pCompositor->computeFramebufferAtomicTexture.view),
					BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(pCompositor->computeOutputSet, pCompositor->computeFramebufferColorTexture.view),
				};
				vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);
			}
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
		vkBindSharedBuffer(&nodeCompositData[i].nodeSetBuffer);
		nodeCompositData[i].pSetMapped = vkSharedBufferPtr(nodeCompositData[i].nodeSetBuffer);
		*nodeCompositData[i].pSetMapped = (MxcNodeCompositorSetState){};

		VkWriteDescriptorSet writeSets[] = {
			BIND_WRITE_NODE_STATE(nodeCompositData[i].nodeSet, nodeCompositData[i].nodeSetBuffer.buffer),
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