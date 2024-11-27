#include "comp_node.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>

enum SetBindCompIndices {
	SET_BIND_COMP_BUFFER_INDEX,
	SET_BIND_COMP_COLOR_INDEX,
	SET_BIND_COMP_NORMAL_INDEX,
	SET_BIND_COMP_GBUFFER_INDEX,
	SET_BIND_COMP_COUNT
};
#define SET_WRITE_COMP_BUFFER(node_set, node_buffer)         \
	(VkWriteDescriptorSet)                                   \
	{                                                        \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
		.dstSet = node_set,                                  \
		.dstBinding = SET_BIND_COMP_BUFFER_INDEX,            \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = node_buffer,                           \
			.range = sizeof(MxcNodeCompositorSetState),      \
		},                                                   \
	}
#define SET_WRITE_COMP_COLOR(node_set, color_image_view)             \
	(VkWriteDescriptorSet)                                           \
	{                                                                \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstSet = node_set,                                          \
		.dstBinding = SET_BIND_COMP_COLOR_INDEX,                     \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = color_image_view,                           \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}
#define SET_WRITE_COMP_NORMAL(node_set, normal_image_view)           \
	(VkWriteDescriptorSet)                                           \
	{                                                                \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstSet = node_set,                                          \
		.dstBinding = SET_BIND_COMP_NORMAL_INDEX,                    \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = normal_image_view,                          \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}
#define SET_WRITE_COMP_GBUFFER(node_set, gbuffer_image_view)         \
	(VkWriteDescriptorSet)                                           \
	{                                                                \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstSet = node_set,                                          \
		.dstBinding = SET_BIND_COMP_GBUFFER_INDEX,                   \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = gbuffer_image_view,                         \
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
		},                                                           \
	}
#define SHADER_STAGE_VERT_TESC_TESE_FRAG VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
#define SHADER_STAGE_TASK_MESH_FRAG      VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT
static void CreateCompSetLayout(MxcCompMode compMode, VkDescriptorSetLayout* pLayout)
{
	VkShaderStageFlags stageFlags = {};
	switch (compMode) {
		case MXC_COMP_MODE_BASIC:     stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
		case MXC_COMP_MODE_TESS:      stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
		case MXC_COMP_MODE_TASK_MESH: stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
		default:                      PANIC("CompMode not supported!");
	}
	const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = SET_BIND_COMP_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_COMP_BUFFER_INDEX] = {
				.binding = SET_BIND_COMP_BUFFER_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
			},
			[SET_BIND_COMP_COLOR_INDEX] = {
				.binding = SET_BIND_COMP_COLOR_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[SET_BIND_COMP_NORMAL_INDEX] = {
				.binding = SET_BIND_COMP_NORMAL_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[SET_BIND_COMP_GBUFFER_INDEX] = {
				.binding = SET_BIND_COMP_GBUFFER_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = stageFlags,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &nodeSetLayoutCreateInfo, MIDVK_ALLOC, pLayout));
}

enum PipeSetCompIndices {
	PIPE_SET_COMP_GLOBAL_INDEX,
	PIPE_SET_COMP_NODE_INDEX,
	PIPE_SET_COMP_COUNT,
};
static void CreateCompPipeLayout(VkDescriptorSetLayout nodeSetLayout, VkPipelineLayout* pNodePipeLayout)
{
	const VkPipelineLayoutCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_COMP_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_COMP_GLOBAL_INDEX] = vk.context.basicPipeLayout.globalSetLayout,
			[PIPE_SET_COMP_NODE_INDEX] = nodeSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, MIDVK_ALLOC, pNodePipeLayout));
}

void mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode)
{
	{   // Create

		// layouts
		CreateCompSetLayout(pInfo->compMode, &pNode->compNodeSetLayout);
		CreateCompPipeLayout(pNode->compNodeSetLayout, &pNode->compNodePipeLayout);

		// meshes
		switch (pInfo->compMode) {
			case MXC_COMP_MODE_BASIC:
				vkCreateBasicPipe("./shaders/basic_comp.vert.spv",
								  "./shaders/basic_comp.frag.spv",
								  vk.context.renderPass,
								  pNode->compNodePipeLayout,
								  &pNode->compNodePipe);
				CreateQuadMesh(0.5f, &pNode->quadMesh);
				break;
			case MXC_COMP_MODE_TESS:
				vkCreateTessPipe("./shaders/tess_comp.vert.spv",
								 "./shaders/tess_comp.tesc.spv",
								 "./shaders/tess_comp.tese.spv",
								 "./shaders/tess_comp.frag.spv",
								 vk.context.renderPass,
								 pNode->compNodePipeLayout,
								 &pNode->compNodePipe);
				CreateQuadPatchMeshSharedMemory(&pNode->quadMesh);
				break;
			case MXC_COMP_MODE_TASK_MESH:
				vkCreateTaskMeshPipe("./shaders/mesh_comp.task.spv",
									 "./shaders/mesh_comp.mesh.spv",
									 "./shaders/mesh_comp.frag.spv",
									 vk.context.renderPass,
									 pNode->compNodePipeLayout,
									 &pNode->compNodePipe);
				CreateQuadMesh(0.5f, &pNode->quadMesh);
				break;
			default: PANIC("CompMode not supported!");
		}

		{
			// sets
			VkQueryPoolCreateInfo queryInfo = {
				.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
				.queryType = VK_QUERY_TYPE_TIMESTAMP,
				.queryCount = 2,
			};
			VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, MIDVK_ALLOC, &pNode->timeQueryPool));

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
			VK_CHECK(vkCreateDescriptorPool(vk.context.device, &poolInfo, MIDVK_ALLOC, &threadContext.descriptorPool));

			// global set
			midVkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.globalSetLayout, &pNode->globalSet.set);
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(VkmGlobalSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			vkCreateBufferSharedMemory(&requestInfo, &pNode->globalSet.buffer, &pNode->globalSet.sharedMemory);
		}

		// node set
		// should I preallocate all set memory? the MxcNodeCompositorSetState * 256 is only 130 kb. That may be a small price to pay to ensure contiguous memory on GPU
		for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
			VkDescriptorSetAllocateInfo setInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = threadContext.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &pNode->compNodeSetLayout,
			};
			VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &nodeCompositorData[i].set));
			vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)nodeCompositorData[i].set, CONCAT(NodeSet, i));
			VkRequestAllocationInfo requestInfo = {
				.memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
				.size = sizeof(MxcNodeCompositorSetState),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			// should I make them all share the same buffer? probably
			vkCreateBufferSharedMemory(&requestInfo, &nodeCompositorData[i].SetBuffer, &nodeCompositorData[i].SetSharedMemory);
		}

		VkFramebufferTextureCreateInfo framebufferInfo = {
			.locality = VK_LOCALITY_CONTEXT,
			.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1}
		};
		vkCreateFramebufferTexture(&framebufferInfo, VK_SWAP_COUNT, pNode->framebuffers);
		vkCreateBasicFramebuffer(vk.context.renderPass, &pNode->framebuffer);
		vkSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pNode->framebuffer, "CompFramebuffer");  // should be moved into method? probably
	}

	{  // Copy needed state
		// todo get rid lets go to context directly
		pNode->compMode = pInfo->compMode;
		pNode->device = vk.context.device;
		pNode->compRenderPass = vk.context.renderPass;
	}
}

void mxcBindUpdateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode)
{
	switch (pInfo->compMode) {
		case MXC_COMP_MODE_BASIC:
			break;
		case MXC_COMP_MODE_TESS:
			BindUpdateQuadPatchMesh(0.5f, &pNode->quadMesh);
			break;
		case MXC_COMP_MODE_TASK_MESH:
			break;
		default: PANIC("CompMode not supported!");
	}
	VK_CHECK(vkBindBufferMemory(vk.context.device, pNode->globalSet.buffer, deviceMemory[pNode->globalSet.sharedMemory.type], pNode->globalSet.sharedMemory.offset));
	vkUpdateDescriptorSets(vk.context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(pNode->globalSet.set, pNode->globalSet.buffer), 0, NULL);
	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		// should I make them all share the same buffer? probably
		midVkBindBufferSharedMemory(nodeCompositorData[i].SetBuffer, nodeCompositorData[i].SetSharedMemory);
		vkUpdateDescriptorSets(vk.context.device, 1, &SET_WRITE_COMP_BUFFER(nodeCompositorData[i].set, nodeCompositorData[i].SetBuffer), 0, NULL);
		nodeCompositorData[i].pNodeSetMapped = midVkSharedMemoryPointer(nodeCompositorData[i].SetSharedMemory);
	}
}

void mxcCompostorNodeRun(const MxcCompositorNodeContext* pNodeContext, const MxcCompNode* pNode)
{
	const MxcCompMode compMode = pNode->compMode;

	const VkDevice        device = pNode->device;
	const VkCommandBuffer cmd = pNodeContext->cmd;
	const VkRenderPass    renderPass = pNode->compRenderPass;
	const VkFramebuffer   framebuffer = pNode->framebuffer;
	const VkDescriptorSet globalSet = pNode->globalSet.set;

	MidCamera globalCamera = {
		.yFOV = 45.0f,
		.zNear = 0.1f,
		.zFar = 100.0f,
	};
	MidPose globalCameraPose = {
		.position = {0.0f, 0.0f, 2.0f},
		.euler = {0.0f, 0.0f, 0.0f},
	};
	globalCameraPose.rotation = QuatFromEuler(globalCameraPose.euler);
	VkmGlobalSetState  globalSetState = {};
	VkmGlobalSetState* pGlobalSetMapped = midVkSharedMemoryPointer(pNode->globalSet.sharedMemory);
	vkmUpdateGlobalSetViewProj(globalCamera, globalCameraPose, &globalSetState, pGlobalSetMapped);

	const VkPipelineLayout compNodePipeLayout = pNode->compNodePipeLayout;
	const VkPipeline       compNodePipe = pNode->compNodePipe;

	const uint32_t     quadIndexCount = pNode->quadMesh.indexCount;
	const VkBuffer     quadBuffer = pNode->quadMesh.buffer;
	const VkDeviceSize quadIndexOffset = pNode->quadMesh.indexOffset;
	const VkDeviceSize quadVertexOffset = pNode->quadMesh.vertexOffset;

	uint64_t          compositorBaseCycleValue = 0;
	const VkSemaphore compTimeline = pNodeContext->compTimeline;

	const VkSwapContext swap = pNodeContext->swap;

	const VkQueryPool timeQueryPool = pNode->timeQueryPool;

	struct {
		VkImageView color;
		VkImageView normal;
		VkImageView depth;
	} compositorFramebufferViews[VK_SWAP_COUNT];
	VkImage compositorFramebufferColorImages[VK_SWAP_COUNT];
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		compositorFramebufferViews[i].color = pNode->framebuffers[i].color.view;
		compositorFramebufferViews[i].normal = pNode->framebuffers[i].normal.view;
		compositorFramebufferViews[i].depth = pNode->framebuffers[i].depth.view;
		compositorFramebufferColorImages[i] = pNode->framebuffers[i].color.image;
	}
	int compositorFramebufferIndex = 0;

	// just making sure atomics are only using barriers, not locks
	for (int i = 0; i < nodeCount; ++i) {
		assert(__atomic_always_lock_free(sizeof(nodesShared[i].timelineValue), &nodesShared[i].timelineValue));
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

run_loop:

	midVkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compTimeline);
	vkmProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globalCameraPose);
	vkmProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globalCameraPose);
	vkmUpdateGlobalSetView(&globalCameraPose, &globalSetState, pGlobalSetMapped);

	// Update and Recording must be separate cycles because it's not ideal to acquire and transition images after being a renderpess
	{  // Update Nodes
		midVkTimelineSignal(device, compositorBaseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, compTimeline);
		vkmCmdResetBegin(cmd);

		for (int i = 0; i < nodeCount; ++i) {
			MxcNodeShared* pNodeShared = activeNodesShared[i];

			// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
			// shared: 569 - semaphore: 315416 ratio: 554.333919
			uint64_t nodeTimelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);

			// I don't like this but it waits until the node renders something. Prediction should be okay here.
			if (nodeTimelineValue < 1)
				continue;

			// We need logic here. Some node you'd want to allow to move themselves. Other locked in specific place. Other move their offset.
			memcpy(&nodeCompositorData[i].rootPose, &pNodeShared->rootPose, sizeof(MidPose));
			//nodeCompositorData[i].rootPose.rotation = QuatFromEuler(nodeCompositorData[i].rootPose.euler);

			// update node model mat... this should happen every frame so user can move it in comp
			nodeCompositorData[i].nodeSetState.model = Mat4FromPosRot(nodeCompositorData[i].rootPose.position, nodeCompositorData[i].rootPose.rotation);

			if (nodeTimelineValue <= nodeCompositorData[i].lastTimelineSwap)
				continue;

			nodeCompositorData[i].lastTimelineSwap = nodeTimelineValue;

			{  // Acquire new framebuffers from node

				// this needs better logic or 2 swap count enforced
				// the inverse of node timeline is the framebuffer index to display
				// while the non-inverse is the framebuffer to be rendered into
				// we want to keep this index a function of the timeline value to evade needing any other index list
				int nodeFramebufferIndex = !(nodeTimelineValue % VK_SWAP_COUNT);
				CmdPipelineImageBarriers2(cmd, 3, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].acquireBarriers);

				// These will end up being updated from a framebuffer pool so they need to be written each switch
				VkWriteDescriptorSet writeSets[] = {
					SET_WRITE_COMP_COLOR(nodeCompositorData[i].set, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].colorView),
					SET_WRITE_COMP_NORMAL(nodeCompositorData[i].set, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].normalView),
					SET_WRITE_COMP_GBUFFER(nodeCompositorData[i].set, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].gBufferView),
				};
				vkUpdateDescriptorSets(device, COUNT(writeSets), writeSets, 0, NULL);
			}

			{  // Calc new node uniform and shared data

				// move the globalSetState that was previously used to render into the nodeSetState to use in comp
				memcpy(&nodeCompositorData[i].nodeSetState.view, (void*)&pNodeShared->globalSetState, sizeof(VkmGlobalSetState));
				nodeCompositorData[i].nodeSetState.ulUV = pNodeShared->ulScreenUV;
				nodeCompositorData[i].nodeSetState.lrUV = pNodeShared->lrScreenUV;

				memcpy(nodeCompositorData[i].pNodeSetMapped, &nodeCompositorData[i].nodeSetState, sizeof(MxcNodeCompositorSetState));

				float radius = pNodeShared->compositorRadius;

				vec4 ulbModel = Vec4Rot(globalCameraPose.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
				vec4 ulbWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, ulbModel);
				vec4 ulbClip = Vec4MulMat4(globalSetState.view, ulbWorld);
				vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbClip));
				vec2 ulbUV = Vec2Clamp(UVFromNDC(ulbNDC), 0.0f, 1.0f);

				vec4 ulfModel = Vec4Rot(globalCameraPose.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
				vec4 ulfWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, ulfModel);
				vec4 ulfClip = Vec4MulMat4(globalSetState.view, ulfWorld);
				vec3 ulfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulfClip));
				vec2 ulfUV = Vec2Clamp(UVFromNDC(ulfNDC), 0.0f, 1.0f);

				vec2 ulUV = Vec2Min(ulfUV, ulbUV);

				vec4 lrbModel = Vec4Rot(globalCameraPose.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
				vec4 lrbWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, lrbModel);
				vec4 lrbClip = Vec4MulMat4(globalSetState.view, lrbWorld);
				vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrbClip));
				vec2 lrbUV = Vec2Clamp(UVFromNDC(lrbNDC), 0.0f, 1.0f);

				vec4 lrfModel = Vec4Rot(globalCameraPose.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
				vec4 lrfWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, lrfModel);
				vec4 lrfClip = Vec4MulMat4(globalSetState.view, lrfWorld);
				vec3 lrfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrfClip));
				vec2 lrfUV = Vec2Clamp(UVFromNDC(lrfNDC), 0.0f, 1.0f);

				vec2 lrUV = Vec2Max(lrbUV, lrfUV);

				vec2 diff = {.vec = lrUV.vec - ulUV.vec};

				// maybe I should only copy camera pos info and generate matrix on other thread? oxr only wants the pose
				pNodeShared->cameraPos = globalCameraPose;
				pNodeShared->camera = globalCamera;
				// write current global set state to node's global set state to use for next node render with new the framebuffer size
				memcpy(&pNodeShared->globalSetState, &globalSetState, sizeof(VkmGlobalSetState) - sizeof(ivec2));
				pNodeShared->globalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
				pNodeShared->ulScreenUV = ulUV;
				pNodeShared->lrScreenUV = lrUV;
				__atomic_thread_fence(__ATOMIC_RELEASE);
			}
		}

		{  // Recording Cycle
			midVkTimelineSignal(device, compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

			compositorFramebufferIndex = !compositorFramebufferIndex;

			ResetQueryPool(device, timeQueryPool, 0, 2);
			CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

			CmdBeginRenderPass(cmd, renderPass, framebuffer, MIDVK_PASS_CLEAR_COLOR,
							   compositorFramebufferViews[compositorFramebufferIndex].color,
							   compositorFramebufferViews[compositorFramebufferIndex].normal,
							   compositorFramebufferViews[compositorFramebufferIndex].depth);

			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipe);
			CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_GLOBAL_INDEX, 1, &globalSet, 0, NULL);

			for (int i = 0; i < nodeCount; ++i) {
				MxcNodeShared* pNodeShared = activeNodesShared[i];

				// find a way to get rid of this load and check
				uint64_t nodeCurrentTimelineSignal = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
				// I don't like this but it waits until the node renders something. Prediction should be okay here.
				if (nodeCurrentTimelineSignal < 1)
					continue;

//				int nodeFramebufferIndex = !(nodeCurrentTimelineSignal % VK_SWAP_COUNT);

				CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_NODE_INDEX, 1, &nodeCompositorData[i].set, 0, NULL);

				// move this into method delegate in node compositor data
				switch (compMode) {
					case MXC_COMP_MODE_BASIC:
					case MXC_COMP_MODE_TESS:
						CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){quadBuffer}, (const VkDeviceSize[]){quadVertexOffset});
						CmdBindIndexBuffer(cmd, quadBuffer, quadIndexOffset, VK_INDEX_TYPE_UINT16);
						CmdDrawIndexed(cmd, quadIndexCount, 1, 0, 0, 0);
						break;
					case MXC_COMP_MODE_TASK_MESH:
						CmdDrawMeshTasksEXT(cmd, 1, 1, 1);
						break;
					default: PANIC("CompMode not supported!");
				}
			}
		}

		CmdEndRenderPass(cmd);

		CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

		{  // Blit Framebuffer
			AcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &compositorNodeContext.swapIndex);
			__atomic_thread_fence(__ATOMIC_RELEASE);
			VkImage swapImage = swap.images[compositorNodeContext.swapIndex];
			CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_COLOR_ATTACHMENT_UNDEFINED, VKM_IMG_BARRIER_BLIT_DST, swapImage));
			CmdBlitImageFullScreen(cmd, compositorFramebufferColorImages[compositorFramebufferIndex], swapImage);
			CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMG_BARRIER_BLIT_DST, VKM_IMG_BARRIER_PRESENT_BLIT_RELEASE, swapImage));
		}

		EndCommandBuffer(cmd);

		midVkTimelineSignal(device, compositorBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compTimeline);

		compositorBaseCycleValue += MXC_CYCLE_COUNT;

		{ // wait for end and output query, probably don't need this wait if not querying?
			midVkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compTimeline);

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

void* mxcCompNodeThread(const MxcCompositorNodeContext* pNodeContext)
{
	MxcCompNode compNode;

	midVkBeginAllocationRequests();
	const MxcCompNodeCreateInfo compNodeInfo = {
//		.compMode = MXC_COMP_MODE_TESS,
		.compMode = MXC_COMP_MODE_BASIC,
	};
	mxcCreateCompNode(&compNodeInfo, &compNode);
	midVkEndAllocationRequests();

	mxcBindUpdateCompNode(&compNodeInfo, &compNode);
	mxcCompostorNodeRun(pNodeContext, &compNode);
	return NULL;
}