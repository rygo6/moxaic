#include "compositor.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>

///////////
//// Pipes
////

//// Compositor Node Render Pipe
enum {
	BIND_COMPOSITOR_INDEX_STATE,
	BIND_COMPOSITOR_INDEX_COLOR,
	BIND_COMPOSITOR_INDEX_GBUFFER,
	BIND_COMPOSITOR_INDEX_COUNT
};
constexpr VkShaderStageFlags MXC_COMPOSITOR_MODE_FLAGS[] = {
	[MXC_COMPOSITOR_MODE_BASIC] = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TESSELATION] = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	[MXC_COMPOSITOR_MODE_TASK_MESH] = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
};
static void CreateCompositorSetLayout(MxcCompositorMode mode, VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = BIND_COMPOSITOR_INDEX_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[BIND_COMPOSITOR_INDEX_STATE] = {
				.binding = BIND_COMPOSITOR_INDEX_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_FLAGS[mode],
			},
			[BIND_COMPOSITOR_INDEX_COLOR] = {
				.binding = BIND_COMPOSITOR_INDEX_COLOR,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[BIND_COMPOSITOR_INDEX_GBUFFER] = {
				.binding = BIND_COMPOSITOR_INDEX_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = MXC_COMPOSITOR_MODE_FLAGS[mode],
				.pImmutableSamplers = &vk.context.linearSampler,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &info, VK_ALLOC, pLayout));
}
#define BIND_COMPOSITOR_WRITE_STATE(set, state)              \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstSet = set,                                       \
		.dstBinding = BIND_COMPOSITOR_INDEX_STATE,           \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = state,                                 \
			.range = sizeof(MxcNodeCompositorSetState),      \
		},                                                   \
	}
#define BIND_COMPOSITOR_WRITE_COLOR(set, view)                       \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = set,                                               \
		.dstBinding = BIND_COMPOSITOR_INDEX_COLOR,                   \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = view,                                       \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}
#define BIND_COMPOSITOR_WRITE_GBUFFER(set, view)                     \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstSet = set,                                               \
		.dstBinding = BIND_COMPOSITOR_INDEX_GBUFFER,                 \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = view,                                       \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}

enum SetCompositorIndices {
	SET_COMPOSITOR_INDEX_GLOBAL,
	SET_COMPOSITOR_INDEX_NODE,
	SET_COMPOSITOR_INDEX_COUNT,
};
static void CreateCompositorPipeLayout(VkDescriptorSetLayout nodeSetLayout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = SET_COMPOSITOR_INDEX_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[SET_COMPOSITOR_INDEX_GLOBAL] = vk.context.basicPipeLayout.globalSetLayout,
			[SET_COMPOSITOR_INDEX_NODE] = nodeSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}

//// Compositor GBuffer Process Pipe
enum BindGBufferProcessIndices {
	BIND_GBUFFER_PROCESS_INDEX_STATE,
	BIND_GBUFFER_PROCESS_INDEX_SRC_DEPTH,
	BIND_GBUFFER_PROCESS_INDEX_DST,
	BIND_GBUFFER_PROCESS_INDEX_COUNT
};
static void CreateGBufferProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = BIND_GBUFFER_PROCESS_INDEX_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[BIND_GBUFFER_PROCESS_INDEX_STATE] = {
				.binding = BIND_GBUFFER_PROCESS_INDEX_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			[BIND_GBUFFER_PROCESS_INDEX_SRC_DEPTH] = {
				.binding = BIND_GBUFFER_PROCESS_INDEX_SRC_DEPTH,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[BIND_GBUFFER_PROCESS_INDEX_DST] = {
				.binding = BIND_GBUFFER_PROCESS_INDEX_DST,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 4,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &nodeSetLayoutCreateInfo, VK_ALLOC, pLayout));
}
#define BIND_WRITE_GBUFFER_PROCESS_STATE(view)               \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstBinding = BIND_GBUFFER_PROCESS_INDEX_STATE,      \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = view,                                  \
			.range = sizeof(GBufferProcessState),            \
		},                                                   \
	}
#define BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(view)          \
	(VkWriteDescriptorSet) {                                \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstBinding = BIND_GBUFFER_PROCESS_INDEX_SRC_DEPTH, \
		.descriptorCount = 1,                               \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){             \
			.imageView = view,                              \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
		},                                                  \
	}
#define BIND_WRITE_GBUFFER_PROCESS_DST(mip_views)               \
	(VkWriteDescriptorSet) {                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
			.dstBinding = BIND_GBUFFER_PROCESS_INDEX_DST,       \
			.descriptorCount = 4,                               \
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
			.pImageInfo = (VkDescriptorImageInfo[]) {           \
			{                                                   \
				.imageView = mip_views[0],                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
			{                                                   \
				.imageView = mip_views[1],                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
			{                                                   \
				.imageView = mip_views[2],                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
			{                                                   \
				.imageView = mip_views[3],                      \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
			},                                                  \
		}                                                       \
	}

enum {
	SET_GBUFFER_PROCESS_INDEX,
	SET_GBUFFER_PROCESS_COUNT,
};
static void CreateGBufferProcessPipeLayout(VkDescriptorSetLayout layout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[SET_GBUFFER_PROCESS_INDEX] = layout,
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
void mxcCompositorNodeRun(const MxcCompositorContext* pCtxt, const MxcCompositorCreateInfo* pInfo, const MxcCompositor* pComp)
{
	auto dvc = vk.context.device;

	auto cmd = pCtxt->cmd;
	auto compTimeline = pCtxt->compositorTimeline;
	u64  compBaseCycleValue = 0;

	auto compMode = pInfo->mode;
	auto renderPass = pComp->compRenderPass;
	auto framebuffer = pComp->framebuffer;
	auto globalSet = pComp->globalSet.set;

	auto compNodePipeLayout = pComp->compositorPipeLayout;
	auto compNodePipe = pComp->compNodePipe;

	auto quadIndexCount = pComp->quadMesh.indexCount;
	auto quadBuffer = pComp->quadMesh.buffer;
	auto quadIndexOffset = pComp->quadMesh.indexOffset;
	auto quadVertexOffset = pComp->quadMesh.vertexOffset;

	auto timeQueryPool = pComp->timeQueryPool;

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
	auto pGlobalSetMapped = vkSharedMemoryPtr(pComp->globalSet.sharedMemory);
	vkmUpdateGlobalSetViewProj(globalCam, globalCamPose, &globalSetState, pGlobalSetMapped);
	pComp->pGBufferProcessMapped->cameraNearZ = globalCam.zNear;
	pComp->pGBufferProcessMapped->cameraFarZ = globalCam.zFar;

	struct {
		VkImageView color;
		VkImageView normal;
		VkImageView depth;
	} compositorFramebufferViews[VK_SWAP_COUNT];
	VkImage compositorFramebufferColorImages[VK_SWAP_COUNT];
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		compositorFramebufferViews[i].color = pComp->framebuffers[i].color.view;
		compositorFramebufferViews[i].normal = pComp->framebuffers[i].normal.view;
		compositorFramebufferViews[i].depth = pComp->framebuffers[i].depth.view;
		compositorFramebufferColorImages[i] = pComp->framebuffers[i].color.image;
	}

	#define COMPOSITOR_DST_READ                                                  \
		.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
						VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
						VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
						VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                            \
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

//	VkImageMemoryBarrier2 localAcquireBarriers[] = {
//		{
//			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//			.image = VK_NULL_HANDLE,
//			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//			SWAP_ACQUIRE_BARRIER,
//		},
//		{
//			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//			.image = VK_NULL_HANDLE,
//			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//			SWAP_ACQUIRE_BARRIER,
//		},
//	};
	VkImageMemoryBarrier2 extAcquireBars[] = {
		{ // Color
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			COMPOSITOR_DST_READ,
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
	VkImageMemoryBarrier2 processFinishBarriers[] = {
		{	// Gbuffer
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
			COMPOSITOR_DST_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = VK_NULL_HANDLE,
			.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
		},
	};
	#undef SWAP_ACQUIRE_BARRIER

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

	vkTimelineWait(dvc, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compTimeline);
	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globalCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globalCamPose);
	vkUpdateGlobalSetView(&globalCamPose, &globalSetState, pGlobalSetMapped);

	// Update and Recording must be separate cycles because it's not ideal to acquire and transition images after being a renderpess
	{  // Update Nodes
		vkTimelineSignal(dvc, compBaseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, compTimeline);
		vkCmdResetBegin(cmd);

		for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
			auto pNodeShrd = activeNodesShared[nodeIndex];

			// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
			// shared: 569 - semaphore: 315416 ratio: 554.333919
			u64 nodeTimelineVal = __atomic_load_n(&pNodeShrd->timelineValue, __ATOMIC_ACQUIRE);

			if (nodeTimelineVal < 1)
				continue;

			auto pNodeCompData = &nodeCompositorData[nodeIndex];

			// We need logic here. Some node you'd want to allow to move themselves. Other locked in specific place. Other move their offset.
			memcpy(&pNodeCompData->rootPose, &pNodeShrd->rootPose, sizeof(MidPose));
			//nodeCompositorData[i].rootPose.rotation = QuatFromEuler(nodeCompositorData[i].rootPose.euler);

			// update node model mat... this should happen every frame so user can move it in comp
			pNodeCompData->setState.model = Mat4FromPosRot(pNodeCompData->rootPose.position, pNodeCompData->rootPose.rotation);

			if (nodeTimelineVal <= pNodeCompData->lastTimelineValue)
				continue;

			{  // Acquire new framebuffers from node

				// this needs better logic or 2 swap count enforced
				// the inverse of node timeline is the framebuffer index to display
				// while the non-inverse is the framebuffer to be rendered into
				// we want to keep this index a function of the timeline value to evade needing any other index list
//				if (pNodeCompositorData->lastTimelineValue > 0) {
//					uint8_t priorNodeFramebufferIndex = !(pNodeCompositorData->lastTimelineValue % VK_SWAP_COUNT);
//					auto pPriorFramebuffers = &pNodeCompositorData->framebuffers[priorNodeFramebufferIndex];
//					CmdPipelineImageBarriers2(cmd, COUNT(pPriorFramebuffers->releaseBarriers), pPriorFramebuffers->releaseBarriers);
////					uint8_t priorExchangedSwap = __atomic_exchange_n(&pNodeShared->swapClaimed[priorNodeFramebufferIndex], false, __ATOMIC_SEQ_CST);
////					assert(priorExchangedSwap);
//				}

				u8   nodeSwapIndex = !(nodeTimelineVal % VK_SWAP_COUNT);
				auto pNodeSwaps = &pNodeCompData->swaps[nodeSwapIndex];

				extAcquireBars[0].image = pNodeSwaps->color;
				extAcquireBars[1].image = pNodeSwaps->depth;
				extAcquireBars[2].image = pNodeSwaps->gBuffer;
				CmdPipelineImageBarriers2(cmd, COUNT(extAcquireBars), extAcquireBars);
//				uint8_t exchangedSwap = __atomic_exchange_n(&pNodeShared->swapClaimed[nodeFramebufferIndex], true, __ATOMIC_SEQ_CST);
//				assert(!exchangedSwap);

//				ivec2 extent = {pNodeShared->globalSetState.framebufferSize.x, pNodeShared->globalSetState.framebufferSize.y};
				ivec2 extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT};
				constexpr int localSize = 4;
				constexpr int gatherSize = 2;
				ivec2 groupCount = iVec2Min(iVec2CeiDivide(extent, localSize * gatherSize), 1);

//				printf("%f %f %f %f\n", pNodeShrd->depthState.minDepth, pNodeShrd->depthState.maxDepth, pNodeShrd->depthState.nearZ, pNodeShrd->depthState.farZ);
				memcpy(&pComp->pGBufferProcessMapped->depth, (void*)&pNodeShrd->depthState, sizeof(MxcDepthState));
				VkWriteDescriptorSet initialPushSet[] = {
					BIND_WRITE_GBUFFER_PROCESS_STATE(pComp->gBufferProcessSetBuffer),
					BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pNodeSwaps->depthView),
					BIND_WRITE_GBUFFER_PROCESS_DST(pNodeSwaps->gBufferMipViews),
				};
				CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pComp->gbufferProcessBlitUpPipe);
				CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pComp->gbufferProcessPipeLayout, SET_GBUFFER_PROCESS_INDEX, COUNT(initialPushSet), initialPushSet);
				CmdDispatch(cmd, groupCount.x, groupCount.y, 1);

				processFinishBarriers[0].image = pNodeSwaps->gBuffer;
				CmdPipelineImageBarriers2(cmd, COUNT(processFinishBarriers), processFinishBarriers);

				VkWriteDescriptorSet writeSets[] = {
					BIND_COMPOSITOR_WRITE_COLOR(pNodeCompData->set, pNodeSwaps->colorView),
					BIND_COMPOSITOR_WRITE_GBUFFER(pNodeCompData->set, pNodeSwaps->gBufferMipViews[0]),
				};
				vkUpdateDescriptorSets(dvc, COUNT(writeSets), writeSets, 0, NULL);
			}

			pNodeCompData->lastTimelineValue = nodeTimelineVal;

			{  // Calc new node uniform and shared data

				// move the globalSetState that was previously used to render into the nodeSetState to use in comp
				memcpy(&pNodeCompData->setState.view, (void*)&pNodeShrd->globalSetState, sizeof(VkGlobalSetState));
				pNodeCompData->setState.ulUV = pNodeShrd->ulScreenUV;
				pNodeCompData->setState.lrUV = pNodeShrd->lrScreenUV;

				memcpy(pNodeCompData->pSetMapped, &pNodeCompData->setState, sizeof(MxcNodeCompositorSetState));

				float radius = pNodeShrd->compositorRadius;

				vec4 ulbModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
				vec4 ulbWorld = Vec4MulMat4(pNodeCompData->setState.model, ulbModel);
				vec4 ulbClip = Vec4MulMat4(globalSetState.view, ulbWorld);
				vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbClip));
				vec2 ulbUV = Vec2Clamp(UVFromNDC(ulbNDC), 0.0f, 1.0f);

				vec4 ulfModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
				vec4 ulfWorld = Vec4MulMat4(pNodeCompData->setState.model, ulfModel);
				vec4 ulfClip = Vec4MulMat4(globalSetState.view, ulfWorld);
				vec3 ulfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulfClip));
				vec2 ulfUV = Vec2Clamp(UVFromNDC(ulfNDC), 0.0f, 1.0f);

				vec2 ulUV = Vec2Min(ulfUV, ulbUV);

				vec4 lrbModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
				vec4 lrbWorld = Vec4MulMat4(pNodeCompData->setState.model, lrbModel);
				vec4 lrbClip = Vec4MulMat4(globalSetState.view, lrbWorld);
				vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrbClip));
				vec2 lrbUV = Vec2Clamp(UVFromNDC(lrbNDC), 0.0f, 1.0f);

				vec4 lrfModel = Vec4Rot(globalCamPose.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
				vec4 lrfWorld = Vec4MulMat4(pNodeCompData->setState.model, lrfModel);
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
				pNodeShrd->ulScreenUV = ulUV;
				pNodeShrd->lrScreenUV = lrUV;
				__atomic_thread_fence(__ATOMIC_RELEASE);
			}
		}

		{  // Recording Cycle
			vkTimelineSignal(dvc, compBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

			compositorFramebufferIndex = !compositorFramebufferIndex;

			ResetQueryPool(dvc, timeQueryPool, 0, 2);
			CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

			CmdBeginRenderPass(cmd, renderPass, framebuffer, VK_PASS_CLEAR_COLOR,
							   compositorFramebufferViews[compositorFramebufferIndex].color,
							   compositorFramebufferViews[compositorFramebufferIndex].normal,
							   compositorFramebufferViews[compositorFramebufferIndex].depth);

			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipe);
			CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, SET_COMPOSITOR_INDEX_GLOBAL, 1, &globalSet, 0, NULL);

			for (int i = 0; i < nodeCount; ++i) {
				MxcNodeShared* pNodeShared = activeNodesShared[i];

				// find a way to get rid of this load and check
				uint64_t nodeCurrentTimelineSignal = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
//				uint64_t nodeCurrentTimelineSignal;
//				vkGetSemaphoreCounterValue(device, nodeContexts[i].vkNodeTimeline, &nodeCurrentTimelineSignal);

				// I don't like this but it waits until the node renders something. Prediction should be okay here.
				if (nodeCurrentTimelineSignal < 1)
					continue;

//				int nodeFramebufferIndex = !(nodeCurrentTimelineSignal % VK_SWAP_COUNT);

				CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, SET_COMPOSITOR_INDEX_NODE, 1, &nodeCompositorData[i].set, 0, NULL);

				// move this into method delegate in node compositor data
				switch (compMode) {
					case MXC_COMPOSITOR_MODE_BASIC:
					case MXC_COMPOSITOR_MODE_TESSELATION:
						CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){quadBuffer}, (const VkDeviceSize[]){quadVertexOffset});
						CmdBindIndexBuffer(cmd, quadBuffer, quadIndexOffset, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(cmd, quadIndexCount, 1, 0, 0, 0);
						break;
					case MXC_COMPOSITOR_MODE_TASK_MESH:
						CmdDrawMeshTasksEXT(cmd, 1, 1, 1);
						break;
					default: PANIC("CompMode not supported!");
				}
			}
		}

		CmdEndRenderPass(cmd);

		CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

		{  // Blit Framebuffer
			AcquireNextImageKHR(dvc, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &compositorContext.swapIndex);
			__atomic_thread_fence(__ATOMIC_RELEASE);
			VkImage swapImage = swap.images[compositorContext.swapIndex];

			VkImageMemoryBarrier2 beginBlitBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = swapImage,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT,
				VK_IMAGE_BARRIER_DST_BLIT,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			};
			CmdPipelineImageBarrier2(cmd, &beginBlitBarrier);

			CmdBlitImageFullScreen(cmd, compositorFramebufferColorImages[compositorFramebufferIndex], swapImage);

			VkImageMemoryBarrier2 endBlitBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
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

		vkTimelineSignal(dvc, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compTimeline);

		compBaseCycleValue += MXC_CYCLE_COUNT;

		{ // wait for end and output query, probably don't need this wait if not querying?
			vkTimelineWait(dvc, compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compTimeline);

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
void mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp)
{
	{   // Create

		// layouts
		CreateCompositorSetLayout(pInfo->mode, &pComp->compositorNodeSetLayout);
		CreateCompositorPipeLayout(pComp->compositorNodeSetLayout, &pComp->compositorPipeLayout);

		// meshes
		switch (pInfo->mode) {
			case MXC_COMPOSITOR_MODE_BASIC:
				vkCreateBasicPipe("./shaders/basic_comp.vert.spv",
								  "./shaders/basic_comp.frag.spv",
								  vk.context.renderPass,
								  pComp->compositorPipeLayout,
								  &pComp->compNodePipe);
				CreateQuadMesh(0.5f, &pComp->quadMesh);
				break;
			case MXC_COMPOSITOR_MODE_TESSELATION:
				vkCreateBasicTessellationPipe("./shaders/tess_comp.vert.spv",
											  "./shaders/tess_comp.tesc.spv",
											  "./shaders/tess_comp.tese.spv",
											  "./shaders/tess_comp.frag.spv",
											  vk.context.renderPass,
											  pComp->compositorPipeLayout,
											  &pComp->compNodePipe);
				CreateQuadPatchMeshSharedMemory(&pComp->quadMesh);
				break;
			case MXC_COMPOSITOR_MODE_TASK_MESH:
				vkCreateBasicTaskMeshPipe("./shaders/mesh_comp.task.spv",
										  "./shaders/mesh_comp.mesh.spv",
										  "./shaders/mesh_comp.frag.spv",
										  vk.context.renderPass,
										  pComp->compositorPipeLayout,
										  &pComp->compNodePipe);
				CreateQuadMesh(0.5f, &pComp->quadMesh);
				break;
			default: PANIC("CompMode not supported!");
		}

		{ // Pools
			VkQueryPoolCreateInfo queryInfo = {
				.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
				.queryType = VK_QUERY_TYPE_TIMESTAMP,
				.queryCount = 2,
			};
			VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, VK_ALLOC, &pComp->timeQueryPool));

			VkDescriptorPoolCreateInfo poolInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
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

		{  // Global
			vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.globalSetLayout, &pComp->globalSet.set);
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(VkGlobalSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateBufferSharedMemory(&requestInfo, &pComp->globalSet.buffer, &pComp->globalSet.sharedMemory);
		}

		{ // GBuffer Process
			CreateGBufferProcessSetLayout(&pComp->gbufferProcessSetLayout);
			CreateGBufferProcessPipeLayout(pComp->gbufferProcessSetLayout, &pComp->gbufferProcessPipeLayout);
			CreateGBufferProcessPipe("./shaders/compositor_gbuffer_blit_up.comp.spv", pComp->gbufferProcessPipeLayout, &pComp->gbufferProcessBlitUpPipe);

			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(GBufferProcessState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateBufferSharedMemory(&requestInfo, &pComp->gBufferProcessSetBuffer, &pComp->gBufferProcessSetSharedMemory);
		}

		// node set
		// should I preallocate all set memory? the MxcNodeCompositorSetState * 256 is only 130 kb. That may be a small price to pay to ensure contiguous memory on GPU
		for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
			VkDescriptorSetAllocateInfo setInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = threadContext.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &pComp->compositorNodeSetLayout,
			};
			VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &nodeCompositorData[i].set));
			vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)nodeCompositorData[i].set, CONCAT(NodeSet, i));
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(MxcNodeCompositorSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			// should I make them all share the same buffer? probably
			vkCreateBufferSharedMemory(&requestInfo, &nodeCompositorData[i].setBuffer, &nodeCompositorData[i].setSharedMemory);
		}

		VkBasicFramebufferTextureCreateInfo framebufferInfo = {
			.locality = VK_LOCALITY_CONTEXT,
			.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1}
		};
		vkCreateBasicFramebufferTextures(&framebufferInfo, VK_SWAP_COUNT, pComp->framebuffers);
		vkCreateBasicFramebuffer(vk.context.renderPass, &pComp->framebuffer);
		vkSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pComp->framebuffer, "CompositeFramebuffer");  // should be moved into method? probably
	}

	{  // Copy needed state
		pComp->device = vk.context.device;
		pComp->compRenderPass = vk.context.renderPass;
	}
}

void mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp)
{
	switch (pInfo->mode) {
		case MXC_COMPOSITOR_MODE_BASIC:
			break;
		case MXC_COMPOSITOR_MODE_TESSELATION:
			BindUpdateQuadPatchMesh(0.5f, &pComp->quadMesh);
			break;
		case MXC_COMPOSITOR_MODE_TASK_MESH:
			break;
		default: PANIC("CompMode not supported!");
	}

	vkBindBufferSharedMemory(pComp->globalSet.buffer, pComp->globalSet.sharedMemory);
	vkBindBufferSharedMemory(pComp->gBufferProcessSetBuffer, pComp->gBufferProcessSetSharedMemory);
	pComp->pGBufferProcessMapped = vkSharedMemoryPtr(pComp->gBufferProcessSetSharedMemory);
	*pComp->pGBufferProcessMapped = (GBufferProcessState){};

	vkUpdateDescriptorSets(vk.context.device, 1, &VK_SET_WRITE_GLOBAL_BUFFER(pComp->globalSet.set, pComp->globalSet.buffer), 0, NULL);
	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// should I make them all share the same buffer? probably
		vkBindBufferSharedMemory(nodeCompositorData[i].setBuffer, nodeCompositorData[i].setSharedMemory);
		vkUpdateDescriptorSets(vk.context.device, 1, &BIND_COMPOSITOR_WRITE_STATE(nodeCompositorData[i].set, nodeCompositorData[i].setBuffer), 0, NULL);
		nodeCompositorData[i].pSetMapped = vkSharedMemoryPtr(nodeCompositorData[i].setSharedMemory);
		*nodeCompositorData[i].pSetMapped = (MxcNodeCompositorSetState){};
	}
}

void* mxcCompNodeThread(MxcCompositorContext* pContext)
{
	MxcCompositor compositor;
	MxcCompositorCreateInfo info = {
//		.mode = MXC_COMPOSITOR_MODE_BASIC,
		.mode = MXC_COMPOSITOR_MODE_TESSELATION,
	};

	midVkBeginAllocationRequests();
	mxcCreateCompositor(&info, &compositor);
	midVkEndAllocationRequests();
	mxcBindUpdateCompositor(&info, &compositor);

	mxcCompositorNodeRun(pContext, &info, &compositor);

	return NULL;
}