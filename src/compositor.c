#if defined(MOXAIC_COMPOSITOR)

#include "compositor.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

#include <stdatomic.h>
#include <assert.h>
#include <float.h>
#include <vulkan/vk_enum_string_helper.h>

MxcCompositor cst = {};

/*
 * Constants
 */

#define GRID_QUAD_SQUARE_SIZE         2
#define GRID_QUAD_COUNT               4
#define GRID_SUBGROUP_SQUARE_SIZE     4
#define GRID_SUBGROUP_COUNT           16
#define GRID_WORKGROUP_SQUARE_SIZE    8
#define GRID_WORKGROUP_SUBGROUP_COUNT 64

/*
 * Pipes
 */

/* Node Pipe */
static const VkShaderStageFlags COMPOSITOR_MODE_STAGE_FLAGS[] = {
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

#define COMPOSITOR_AGGREGATE_STAGE_FLAGS          \
	VK_SHADER_STAGE_VERTEX_BIT |                  \
	VK_SHADER_STAGE_FRAGMENT_BIT |                \
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |    \
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | \
	VK_SHADER_STAGE_TASK_BIT_EXT |                \
	VK_SHADER_STAGE_MESH_BIT_EXT |                \
	VK_SHADER_STAGE_COMPUTE_BIT

enum {
	SET_BIND_INDEX_NODE_STATE,
	SET_BIND_INDEX_NODE_COLOR,
	SET_BIND_INDEX_NODE_GBUFFER,
	SET_BIND_INDEX_NODE_COUNT,
};

typedef struct NodePush {
	u32 nodeHandle;
} NodePush;

static VkShaderStageFlags CompositeModeShaderFlags(const bool* pModes)
{
	VkShaderStageFlags stageFlags = 0;
	for (int i = MXC_COMPOSITOR_MODE_QUAD; i < MXC_COMPOSITOR_MODE_COUNT; ++i)
		if (pModes[i]) stageFlags |= COMPOSITOR_MODE_STAGE_FLAGS[i];
	return stageFlags;
}

static void CreateNodeSetLayout(VkShaderStageFlags stageFlags, VkDescriptorSetLayout* pLayout)
{
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &(VkDescriptorSetLayoutCreateInfo){
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		&(VkDescriptorSetLayoutBindingFlagsCreateInfo ){
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = 3,
			.pBindingFlags = (VkDescriptorBindingFlags[]) {
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
			},
		},
		.bindingCount = SET_BIND_INDEX_NODE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_INDEX_NODE_STATE] = {
				.binding         = SET_BIND_INDEX_NODE_STATE,
				.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = MXC_NODE_CAPACITY,
				.stageFlags      = stageFlags,
			},
			[SET_BIND_INDEX_NODE_COLOR] = {
				.binding         = SET_BIND_INDEX_NODE_COLOR,
				.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = MXC_NODE_CAPACITY,
				.stageFlags      = stageFlags,
			},
			[SET_BIND_INDEX_NODE_GBUFFER] = {
				.binding         = SET_BIND_INDEX_NODE_GBUFFER,
				.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = MXC_NODE_CAPACITY,
				.stageFlags      = stageFlags,
			},
		},
	}, VK_ALLOC, pLayout));
	VK_SET_DEBUG_NAME(*pLayout, "NodeSetLayout");
}

#define BIND_WRITE_NODE_STATE(_set, _index, _buffer)             \
	(VkWriteDescriptorSet) {                                     \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
		.dstSet = _set,                                          \
		.dstBinding = SET_BIND_INDEX_NODE_STATE,                 \
		.dstArrayElement = _index,                               \
		.descriptorCount = 1,                                    \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,     \
		.pBufferInfo = &(VkDescriptorBufferInfo){                \
			.buffer = _buffer,                                   \
			.offset = _index * sizeof(MxcCompositorNodeSetState) \
			.range = sizeof(MxcCompositorNodeSetState),          \
		},                                                       \
	}
#define BIND_WRITE_NODE_COLOR(_set, _index, _sampler, _view, _layout) \
	(VkWriteDescriptorSet) {                                          \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                       \
		.dstSet = _set,                                               \
		.dstBinding = SET_BIND_INDEX_NODE_COLOR,                      \
		.dstArrayElement = _index,                                    \
		.descriptorCount = 1,                                         \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  \
		.pImageInfo = &(VkDescriptorImageInfo){                       \
			.sampler = _sampler,                                      \
			.imageView = _view,                                       \
			.imageLayout = _layout,                                   \
		},                                                            \
	}
#define BIND_WRITE_NODE_GBUFFER(_set, _index, _sampler, _view, _layout) \
	(VkWriteDescriptorSet) {                                            \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                         \
		.dstSet = _set,                                                 \
		.dstBinding = SET_BIND_INDEX_NODE_GBUFFER,                      \
		.dstArrayElement = _index,                                      \
		.descriptorCount = 1,                                           \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    \
		.pImageInfo = &(VkDescriptorImageInfo){                         \
			.sampler = _sampler,                                        \
			.imageView = _view,                                         \
			.imageLayout = _layout,                                     \
		},                                                              \
	}

enum {
	PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL,
	PIPE_SET_INDEX_NODE_GRAPHICS_NODE,
	PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
};

static void CreateGraphicsNodePipeLayout(VkShaderStageFlags    stageFlags,
                                         VkDescriptorSetLayout nodeSetLayout,
                                         VkPipelineLayout*     pPipeLayout)
{
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &(VkPipelineLayoutCreateInfo){
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_GRAPHICS_COUNT,
		.pSetLayouts    = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_GRAPHICS_NODE]   = nodeSetLayout,
		},
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &(VkPushConstantRange){
			.stageFlags = stageFlags,
			.offset     = 0,
			.size       = sizeof(NodePush)
		},
	}, VK_ALLOC, pPipeLayout));
	VK_SET_DEBUG_NAME(*pPipeLayout, "GraphicsNodePipeLayout");
}

/* Compute Node Pipe */

enum {
	SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
	SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
	SET_BIND_INDEX_NODE_COMPUTE_COUNT,
};

static void CreateComputeOutputSetLayout(VkDescriptorSetLayout* pLayout)
{
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device,
		&(VkDescriptorSetLayoutCreateInfo){
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = SET_BIND_INDEX_NODE_COMPUTE_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT] = {
				.binding = SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			[SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT] = {
				.binding = SET_BIND_INDEX_NODE_COMPUTE_COLOR_OUTPUT,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	}, VK_ALLOC, pLayout));
	VK_SET_DEBUG_NAME(*pLayout, "ComputeOutputSetLayout");
}

#define BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(set, view)         \
	(VkWriteDescriptorSet) {                                     \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                  \
		.dstSet = set,                                           \
		.dstBinding = SET_BIND_INDEX_NODE_COMPUTE_ATOMIC_OUTPUT, \
		.descriptorCount = 1,                                    \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,      \
		.pImageInfo = &(VkDescriptorImageInfo){                  \
			.imageView = view,                                   \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,              \
		},                                                       \
	}
#define BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(set, view)         \
	(VkWriteDescriptorSet) {                                    \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 \
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

static void CreateNodeComputePipeLayout(VkShaderStageFlags    stageFlags,
                                        VkDescriptorSetLayout nodeSetLayout,
                                        VkDescriptorSetLayout computeOutputSetLayout,
                                        VkPipelineLayout*     pLayout)
{
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &(VkPipelineLayoutCreateInfo){
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_NODE_COMPUTE_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_NODE_COMPUTE_NODE]   = nodeSetLayout,
			[PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT] = computeOutputSetLayout,
		},
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &(VkPushConstantRange){
			.stageFlags = stageFlags,
			.offset     = 0,
			.size       = sizeof(NodePush)
		},
	}, VK_ALLOC, pLayout));
	VK_SET_DEBUG_NAME(*pLayout, "NodeComputePipeLayout");
}

/* Final Blit Pipe */

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
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &(VkDescriptorSetLayoutCreateInfo){
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_INDEX_FINAL_BLIT_COUNT,
		.pBindings    = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_INDEX_FINAL_BLIT_SRC] = {
				.binding = SET_BIND_INDEX_FINAL_BLIT_SRC,
				.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = BIND_ARRAY_INDEX_FINAL_BLIT_SRC_COUNT,
				.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			[SET_BIND_INDEX_FINAL_BLIT_DST] = {
				.binding = SET_BIND_INDEX_FINAL_BLIT_DST,
				.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	}, VK_ALLOC, pLayout));
	VK_SET_DEBUG_NAME(*pLayout, "FinalBlitSetLayout");
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

static void CreateFinalBlitPipeLayout(FinalBlitSetLayout inOutLayout,
	                                  FinalBlitPipeLayout* pLayout) {
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_FINAL_BLIT_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_FINAL_BLIT_GLOBAL] = vk.context.globalSetLayout,
			[PIPE_SET_INDEX_FINAL_BLIT_INOUT] = inOutLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
	VK_SET_DEBUG_NAME(*pLayout, "FinalBlitPipeLayout");
}

/* Time Queries */
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

/* Barriers */
#define COMPOSITOR_DST_GRAPHICS_READ                                          \
	.dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
	                 VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
	                 VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
	                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                             \
	.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

typedef struct ImageMemoryBarrierDst {
	VkPipelineStageFlags2 dstStageMask;
	VkAccessFlags2        dstAccessMask;
	VkImageLayout         newLayout;
} ImageMemoryBarrierDst;

static const ImageMemoryBarrierDst ProcessDstBarrier[] = {
	[MXC_COMPOSITOR_MODE_QUAD]        = (ImageMemoryBarrierDst){COMPOSITOR_DST_GRAPHICS_READ},
	[MXC_COMPOSITOR_MODE_TESSELATION] = (ImageMemoryBarrierDst){COMPOSITOR_DST_GRAPHICS_READ},
	[MXC_COMPOSITOR_MODE_TASK_MESH]   = (ImageMemoryBarrierDst){COMPOSITOR_DST_GRAPHICS_READ},
	[MXC_COMPOSITOR_MODE_COMPUTE]     = (ImageMemoryBarrierDst){VK_IMAGE_BARRIER_DST_COMPUTE_READ},
};

constexpr u32 ExternalQueueFamilyIndex[] = {
	[MXC_NODE_INTERPROCESS_MODE_THREAD]   = VK_QUEUE_FAMILY_IGNORED,
	[MXC_NODE_INTERPROCESS_MODE_EXPORTED] = VK_QUEUE_FAMILY_EXTERNAL,
};

static u32 CompositorQueueFamilyIndex[] = {
	[MXC_NODE_INTERPROCESS_MODE_THREAD]   = VK_QUEUE_FAMILY_IGNORED,
	[MXC_NODE_INTERPROCESS_MODE_EXPORTED] = 0,
};

/*
 * Main Run Loop
 */

static void CompositorRun(MxcCompositorContext* pCstCtx, MxcCompositor* pCst)
{
	/*
	 * Local Extract
	 */
	EXTRACT_FIELD(&vk.context, device);
	EXTRACT_FIELD(&vk.context, depthRenderPass);
	EXTRACT_FIELD(&vk.context, depthFramebuffer);

	EXTRACT_FIELD(&node, gbufferProcessDownPipe);
	EXTRACT_FIELD(&node, gbufferProcessUpPipe);
	EXTRACT_FIELD(&node, gbufferProcessPipeLayout);

	EXTRACT_FIELD(pCstCtx, gfxCmd);
	auto compTimeline = pCstCtx->timeline;
	auto pSwapCtx = &pCstCtx->swapCtx;

	EXTRACT_FIELD(pCst, globalSet);
	EXTRACT_FIELD(pCst, compOutputSet);

	EXTRACT_FIELD(pCst, gfxPipeLayout);
	EXTRACT_FIELD(pCst, gfxQuadPipe);
	EXTRACT_FIELD(pCst, gfxTessPipe);
	EXTRACT_FIELD(pCst, nodeTaskMeshPipe);

	EXTRACT_FIELD(pCst, compPipeLayout);
	EXTRACT_FIELD(pCst, compPipe);
	EXTRACT_FIELD(pCst, postCompPipe);

	EXTRACT_FIELD(pCst, finalBlitPipe);
	EXTRACT_FIELD(pCst, finalBlitPipeLayout);

	EXTRACT_FIELD(pCst, timeQryPool);

	EXTRACT_FIELD(pCst, pLineMapped);
	auto lineBuffer = pCst->lineBuffer.buffer;

	auto quadMeshOffsets = pCst->quadMesh.offsets;
	auto quadMeshBuf = pCst->quadMesh.buf;

	auto quadPatchOffsets = pCst->quadPatchMesh.offsets;
	auto quadPatchBuf = pCst->quadPatchMesh.sharedBuffer.buffer;

	auto gfxFrameColorView = pCst->framebufferTexture.color.view;
	auto gfxFrameDepthView = pCst->framebufferTexture.depth.view;
	auto compFrameColorView = pCst->compFrameColorTex.view;

	auto gfxFrameColorImg = pCst->framebufferTexture.color.image;

	auto compFrameAtomicImg = pCst->compFrameAtomicTex.image;
	auto compFrameColorImg = pCst->compFrameColorTex.image;

	auto pGlobSetMapped = vkSharedMemoryPtr(pCst->globalBuffer.memory);

	CompositorQueueFamilyIndex[MXC_NODE_INTERPROCESS_MODE_EXPORTED] = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

	atomic_store(&pCstCtx->isReady, true);

	// We copy everything locally. Set null to ensure not used!
	pCstCtx = NULL;
	pCst = NULL;

	/*
	 * Local State
	 */
	camera globCam = {
		.yFovRad = RAD_FROM_DEG(45.0f),
		.zNear = 0.1f,
		.zFar = 100.0f,
		.dimension.x = DEFAULT_WIDTH,
		.dimension.y = DEFAULT_HEIGHT,
	};
	pose globCamPose = {
		.pos = VEC3(0.0f, 0.0f, 2.0f),
		.euler = VEC3(0.0f, 0.0f, 0.0f),
	};
	globCamPose.rot = QuatFromEuler(globCamPose.euler);

	VkGlobalSetState globSetState = (VkGlobalSetState){};
	vkUpdateGlobalSetViewProj(globCam, globCamPose, &globSetState);
	memcpy(pGlobSetMapped, &globSetState, sizeof(VkGlobalSetState));

CompositeLoop:

	/*
	 * MXC_CYCLE_UPDATE_WINDOW_STATE
	 */

	/*
	 * MXC_CYCLE_PROCESS_INPUT
	 */
	atomic_thread_fence(memory_order_acquire);
	vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_PROCESS_INPUT, compTimeline);
	u64 baseCycleValue = compositorContext.baseCycleValue ;

	midProcessCameraMouseInput(midWindowInput.deltaTime, mxcWindowInput.mouseDelta, &globCamPose);
	midProcessCameraKeyInput(midWindowInput.deltaTime, mxcWindowInput.move, &globCamPose);
	vkUpdateGlobalSetView(globCamPose, &globSetState);
	memcpy(pGlobSetMapped, &globSetState, sizeof(VkGlobalSetState));

	/*
	 * MXC_CYCLE_UPDATE_NODE_STATES
	 */
	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, compTimeline);

	CmdResetBegin(gfxCmd);
	vk.ResetQueryPool(device, timeQryPool, 0, TIME_QUERY_COUNT);
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_GBUFFER_PROCESS_BEGIN);

	ivec2 windowExtent  = mxcWindowInput.iDimensions;
	int   windowPixelCt = windowExtent.x * windowExtent.y;
	int   windowGroupCt = windowPixelCt / GRID_SUBGROUP_COUNT / GRID_WORKGROUP_SUBGROUP_COUNT;

	/* Iterate Node State Updates */
	for (u32 iCstMode = MXC_COMPOSITOR_MODE_QUAD; iCstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCstMode) {
		atomic_thread_fence(memory_order_acquire);
		if (node.active[iCstMode].count == 0)
			continue;

		ImageMemoryBarrierDst dstBarrier = ProcessDstBarrier[iCstMode];

		auto pActiveNodes = &node.active[iCstMode];
		for (u32 iNode = 0; iNode < pActiveNodes->count; ++iNode) {
			atomic_thread_fence(memory_order_acquire);
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeShr = node.pShared[hNode];
			auto pNodeCst = &cst.nodeData[hNode];

			EXTRACT_FIELD(pNodeCst, activeInterprocessMode);

			/* Update Root Pose */
			ATOMIC_FENCE_SCOPE {
				// Update InteractionState and RootPose every cycle no matter what so that app stays responsive to moving.
				// This should probably be in a threaded node.
				vec3 worldDiff = VEC3_ZERO;
				switch (pNodeCst->interactionState) {
					case NODE_INTERACTION_STATE_SELECT:
						ray priorScreenRay = rayFromScreenUV(mxcWindowInput.priorMouseUV, globSetState.invProj, globSetState.invView, globSetState.invViewProj);
						ray screenRay = rayFromScreenUV(mxcWindowInput.mouseUV, globSetState.invProj, globSetState.invView, globSetState.invViewProj);

						vec4  nodeOrigin = vec4MulMat4(pNodeCst->compositingNodeSetState.model, VEC4_IDENT);
						Plane plane = {.origin = VEC3(nodeOrigin.x, nodeOrigin.y, nodeOrigin.z), .normal = VEC3(0, 0, 1)};

						vec3 hitPoints[2];
						if (rayIntersetPlane(priorScreenRay, plane, &hitPoints[0]) && rayIntersetPlane(screenRay, plane, &hitPoints[1])) {
							worldDiff = vec3Sub(hitPoints[0], hitPoints[1]);
						}

						break;
					default: break;
				}

				pNodeShr->rootPose.pos.vec -= worldDiff.vec;
				pNodeCst->compositingNodeSetState.model = mat4FromPosRot(pNodeShr->rootPose.pos, pNodeShr->rootPose.rot);
			}

			/* Poll New Node Swap */
			u64 nodeTimelineVal = pNodeShr->timelineValue;
			if (nodeTimelineVal <= pNodeCst->lastTimelineValue)
				continue;

			pNodeCst->lastTimelineValue = nodeTimelineVal;
			atomic_thread_fence(memory_order_release);

			/* Acquire New Node Swap */
			ATOMIC_FENCE_SCOPE {
				// int iPriorSwapImg = (nodeTimelineVal % VK_SWAP_COUNT);
				int iSwapImg = !(nodeTimelineVal % VK_SWAP_COUNT);

				int iLeftColorImgId = pNodeShr->viewSwaps[XR_VIEW_ID_LEFT_STEREO].colorId;
				int iLeftDepthImgId = pNodeShr->viewSwaps[XR_VIEW_ID_LEFT_STEREO].depthId;
				int iRightColorImgId = pNodeShr->viewSwaps[XR_VIEW_ID_RIGHT_STEREO].colorId;
				int iRightDepthImgId = pNodeShr->viewSwaps[XR_VIEW_ID_RIGHT_STEREO].depthId;

				// need better way to determine these invalid
				// and maybe better way to signify frame has been set
				if (iLeftColorImgId == CHAR_MAX) continue;
				if (iLeftDepthImgId == CHAR_MAX) continue;
				// if (iRightColorImgId == CHAR_MAX) continue;
				// if (iRightColorImgId == CHAR_MAX) continue;

				auto pNodeCtxt = &node.context[hNode];
				MxcNodeSwap* pLeftColorSwap  = &pNodeCst->swaps[iLeftColorImgId][iSwapImg];
				MxcNodeSwap* pLeftDepthSwap  = &pNodeCst->swaps[iLeftDepthImgId][iSwapImg];
				MxcNodeSwap* pRightColorSwap = &pNodeCst->swaps[iRightColorImgId][iSwapImg];
				MxcNodeSwap* pRightDepthSwap = &pNodeCst->swaps[iRightDepthImgId][iSwapImg];

				MxcNodeGBuffer* pLeftGBuffer = &pNodeCst->gbuffer[XR_VIEW_ID_LEFT_STEREO];
				MxcNodeGBuffer* pRightGBuffer = &pNodeCst->gbuffer[XR_VIEW_ID_RIGHT_STEREO];

				// TODO each node needs its own process state
//				memcpy(pProcessStateMapped, pProcState, sizeof(MxcProcessState));

				// Release Prior Swaps. Release barrier not needed since we don't retain the image
				// CMD_IMAGE_BARRIERS2(gfxCmd, {
				// 	{
				// 		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				// 		.image = pNodeCst->swaps[iLeftColorImgId][iPriorSwapImg].image,
				// 		VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				// 		VK_IMAGE_BARRIER_DST_COMPUTE_RELEASE,
				// 		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				// 		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
				// 	},
				// 	{
				// 		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				// 		.image = pNodeCst->swaps[iLeftDepthImgId][iPriorSwapImg].image,
				// 		VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				// 		VK_IMAGE_BARRIER_DST_COMPUTE_RELEASE,
				// 		VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				// 		VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
				// 	},
				// });

				u32 srcQueueFamilyIndex = ExternalQueueFamilyIndex[activeInterprocessMode];
				u32 dstQueueFamilyIndex = CompositorQueueFamilyIndex[activeInterprocessMode];

				// Acquire Swaps
				CMD_IMAGE_BARRIERS2(gfxCmd, {
					{	// Color
						VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.image               = pLeftColorSwap->image,
						.srcStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
						.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = srcQueueFamilyIndex,
						.dstQueueFamilyIndex = dstQueueFamilyIndex,
						.newLayout           = dstBarrier.newLayout,
						.dstStageMask        = dstBarrier.dstStageMask,
						.dstAccessMask       = dstBarrier.dstAccessMask,
						VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
					},
					{	// Depth
						VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.image               = pLeftDepthSwap->image,
						.srcStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
						.oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
						.srcQueueFamilyIndex = srcQueueFamilyIndex,
						.dstQueueFamilyIndex = dstQueueFamilyIndex,
						VK_IMAGE_BARRIER_DST_COMPUTE_READ,
						VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE
					},
					{	// Gbuffer
						VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.image               = pLeftGBuffer->image,
						.srcStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.srcAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
						.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED, // Acquire discard
						.srcQueueFamilyIndex = srcQueueFamilyIndex,
						.dstQueueFamilyIndex = dstQueueFamilyIndex,
						VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
						VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
					},
				});

				auto pProcessState = &pNodeShr->processState;
				// Should be way to only set this once
				pProcessState->cameraNearZ = globCam.zNear;
				pProcessState->cameraFarZ = globCam.zFar;

				// TODO this needs to be specifically only the rect which was rendered into
				ivec2 nodeSwapExtent = IVEC2(pNodeShr->swapMaxWidth, pNodeShr->swapMaxHeight);
				mxcNodeGBufferProcessDepth(gfxCmd, pProcessState, pLeftDepthSwap, pLeftGBuffer, nodeSwapExtent);

				CMD_IMAGE_BARRIERS2(gfxCmd, {
					{	// Gbuffer
						VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.image         = pLeftGBuffer->image,
						VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
						.newLayout     = dstBarrier.newLayout,
						.dstStageMask  = dstBarrier.dstStageMask,
						.dstAccessMask = dstBarrier.dstAccessMask,
						VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
						VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
					},
				});

				// Update Node Descriptor Sets
				CMD_WRITE_SETS(device, {
					BIND_WRITE_NODE_COLOR(cst.nodeSet, hNode, vk.context.nearestSampler, pLeftColorSwap->view, dstBarrier.newLayout),
					BIND_WRITE_NODE_GBUFFER(cst.nodeSet, hNode, vk.context.nearestSampler, pLeftGBuffer->mipViews[0], dstBarrier.newLayout),
				});
			}

			/* Calc new node uniform and shared data */
			ATOMIC_FENCE_SCOPE {
				float radius = pNodeShr->compositorRadius;
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

				vec2 uvMin = VEC2(FLT_MAX, FLT_MAX);
				vec2 uvMax = VEC2(FLT_MIN, FLT_MIN);
				for (int i = 0; i < CORNER_COUNT; ++i) {
					vec4 model = VEC4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
					vec4 world = vec4MulMat4(pNodeCst->compositingNodeSetState.model, model);
					vec4 clip  = vec4MulMat4(globSetState.view, world);
					vec3 ndc   = Vec4WDivide(vec4MulMat4(globSetState.proj, clip));
					vec2 uv    = UVFromNDC(ndc);
					uvMin.x = MIN(uvMin.x, uv.x);
					uvMin.y = MIN(uvMin.y, uv.y);
					uvMax.x = MAX(uvMax.x, uv.x);
					uvMax.y = MAX(uvMax.y, uv.y);
					pNodeCst->worldCorners[i] = VEC3(world.x, world.y, world.z);
					pNodeCst->uvCorners[i] = uv;
				}
				vec2 uvMinClamp = Vec2Clamp(uvMin, 0.0f, 1.0f);
				vec2 uvMaxClamp = Vec2Clamp(uvMax, 0.0f, 1.0f);
				vec2 uvDiff = (vec2){.vec = uvMaxClamp.vec - uvMinClamp.vec};  // TODO fill in macros for this

				/* Update Interaction Line Segments */
				{
					static const color stateColors[] = {
						[NODE_INTERACTION_STATE_NONE] = {{128, 128, 128, 255}},
						[NODE_INTERACTION_STATE_HOVER] = {{128, 128, 255, 255}},
						[NODE_INTERACTION_STATE_SELECT] = {{255, 255, 255, 255}},
					};

					bool moveButtonDown = mxcWindowInput.leftMouseButton;
					bool isHovering = false;
					for (u32 i = 0; i < MXC_CUBE_SEGMENT_COUNT; i += 2) {
						vec2 uvStart = pNodeCst->uvCorners[MXC_CUBE_SEGMENTS[i]];
						vec2 uvEnd = pNodeCst->uvCorners[MXC_CUBE_SEGMENTS[i + 1]];
						bool segmentHovering = Vec2PointOnLineSegment(mxcWindowInput.mouseUV, uvStart, uvEnd, 0.0005f);
						isHovering |= segmentHovering;

						vec3 start = pNodeCst->worldCorners[MXC_CUBE_SEGMENTS[i]];
						vec3 end = pNodeCst->worldCorners[MXC_CUBE_SEGMENTS[i + 1]];
						pNodeCst->worldLineSegments[i].pos = start;
						pNodeCst->worldLineSegments[i + 1].pos = end;
						pNodeCst->worldLineSegments[i].color = stateColors[segmentHovering ? NODE_INTERACTION_STATE_HOVER : NODE_INTERACTION_STATE_NONE];
						pNodeCst->worldLineSegments[i + 1].color = stateColors[segmentHovering ? NODE_INTERACTION_STATE_HOVER : NODE_INTERACTION_STATE_NONE];
					}

					switch (pNodeCst->interactionState) {
						case NODE_INTERACTION_STATE_NONE:
							pNodeCst->interactionState = isHovering ? NODE_INTERACTION_STATE_HOVER
							                              : NODE_INTERACTION_STATE_NONE;
							break;
						case NODE_INTERACTION_STATE_HOVER:
							pNodeCst->interactionState = isHovering ? moveButtonDown
							                                    ? NODE_INTERACTION_STATE_SELECT
							                                    : NODE_INTERACTION_STATE_HOVER
							                              : NODE_INTERACTION_STATE_NONE;
							break;
						case NODE_INTERACTION_STATE_SELECT:
							pNodeCst->interactionState = moveButtonDown ? NODE_INTERACTION_STATE_SELECT
							                                  : NODE_INTERACTION_STATE_NONE;

							for (u32 i = 0; i < MXC_CUBE_SEGMENT_COUNT; i += 2) {
								pNodeCst->worldLineSegments[i].color     = stateColors[NODE_INTERACTION_STATE_SELECT];
								pNodeCst->worldLineSegments[i + 1].color = stateColors[NODE_INTERACTION_STATE_SELECT];
							}
							break;
						default: break;
					}
				}

				// Update compositor to use node state which rendered the frame
				memcpy(&pNodeCst->compositingNodeSetState, &pNodeCst->renderingNodeSetState, sizeof(MxcCompositorNodeSetState));
				memcpy(cst.pNodeSetMapped + hNode, &pNodeCst->compositingNodeSetState, sizeof(MxcCompositorNodeSetState));

				// Update node state to use in next node frame
				pNodeShr->cameraPose = globCamPose;
				pNodeShr->camera = globCam;

				pNodeShr->left.active = false;
				pNodeShr->left.gripPose = globCamPose;
				pNodeShr->left.aimPose = globCamPose;
				pNodeShr->left.selectClick = mxcWindowInput.leftMouseButton;

				pNodeShr->right.active = true;
				pNodeShr->right.gripPose = globCamPose;
				pNodeShr->right.aimPose = globCamPose;
				pNodeShr->right.selectClick = mxcWindowInput.leftMouseButton;

				pNodeShr->clip.ulUV = uvMinClamp;
				pNodeShr->clip.lrUV = uvMaxClamp;

				vkUpdateGlobalSetViewProj(pNodeShr->camera, pNodeShr->cameraPose, (VkGlobalSetState*)&pNodeCst->renderingNodeSetState.view); // don't have to call SetViewProj every frame?
				memcpy(&pNodeCst->renderingNodeSetState.ulUV, &pNodeShr->clip, sizeof(MxcClip));
			}
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_GBUFFER_PROCESS_END);

	/*
	 * MXC_CYCLE_COMPOSITOR_RECORD
	 */

	/* Graphics Pipe */
	vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

	vk.CmdSetViewport(gfxCmd, 0, 1, &(VkViewport){.width = windowExtent.x, .height = windowExtent.y, .maxDepth = 1.0f});
	vk.CmdSetScissor(gfxCmd,  0, 1, &(VkRect2D){.extent = {.width = windowExtent.x, .height = windowExtent.y}});
	CmdBeginDepthRenderPass(gfxCmd, depthRenderPass, depthFramebuffer, VK_RENDER_PASS_CLEAR_COLOR, gfxFrameColorView, gfxFrameDepthView);

	vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_GLOBAL, 1, &globalSet,        0, NULL);
	vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeLayout, PIPE_SET_INDEX_NODE_GRAPHICS_NODE,   1, &cst.nodeSet, 0, NULL);

	bool hasGfx = false;
	bool hasComp = false;

	/* Graphics Quad Node Commands */
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_QUAD_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (node.active[MXC_COMPOSITOR_MODE_QUAD].count > 0) {
		hasGfx = true;

		auto pActiveNodes = &node.active[MXC_COMPOSITOR_MODE_QUAD];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxQuadPipe);
		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadMeshBuf}, (VkDeviceSize[]){quadMeshOffsets.vertexOffset});
		vk.CmdBindIndexBuffer(gfxCmd, quadMeshBuf, quadMeshOffsets.indexOffset, VK_INDEX_TYPE_UINT16);

		for (int iNode = 0; iNode < pActiveNodes->count; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			vk.CmdPushConstants(gfxCmd, gfxPipeLayout, COMPOSITOR_AGGREGATE_STAGE_FLAGS, 0, sizeof(NodePush), &(NodePush){.nodeHandle = hNode});
			vk.CmdDrawIndexed(gfxCmd, quadMeshOffsets.indexCount, 1, 0, 0, 0);
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_QUAD_RENDER_END);

	/* Graphics Tesselation Node Commands */
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TESS_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (node.active[MXC_COMPOSITOR_MODE_TESSELATION].count > 0) {
		hasGfx = true;

		auto pActiveNodes = &node.active[MXC_COMPOSITOR_MODE_TESSELATION];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxTessPipe);
		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){quadPatchBuf}, (VkDeviceSize[]){quadPatchOffsets.vertexOffset});
		vk.CmdBindIndexBuffer(gfxCmd, quadPatchBuf, quadPatchOffsets.indexOffset, VK_INDEX_TYPE_UINT16);

		for (int iNode = 0; iNode < pActiveNodes->count; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			vk.CmdPushConstants(gfxCmd, gfxPipeLayout, COMPOSITOR_AGGREGATE_STAGE_FLAGS, 0, sizeof(NodePush), &(NodePush){.nodeHandle = hNode});
			vk.CmdDrawIndexed(gfxCmd, quadPatchOffsets.indexCount, 1, 0, 0, 0); // this should use instancing instead of push on the node handle
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TESS_RENDER_END);

	/* Graphics Task Mesh Node Commands */
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TASKMESH_RENDER_BEGIN);
	atomic_thread_fence(memory_order_acquire);
	if (node.active[MXC_COMPOSITOR_MODE_TASK_MESH].count > 0) {
		hasGfx = true;

		auto pActiveNodes = &node.active[MXC_COMPOSITOR_MODE_TASK_MESH];
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodeTaskMeshPipe);

		for (int iNode = 0; iNode < pActiveNodes->count; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &cst.nodeData[hNode];
			vk.CmdPushConstants(gfxCmd, gfxPipeLayout, COMPOSITOR_AGGREGATE_STAGE_FLAGS, 0, sizeof(NodePush), &(NodePush){.nodeHandle = hNode});
			vk.CmdDrawMeshTasksEXT(gfxCmd, 1, 1, 1);
		}
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_TASKMESH_RENDER_END);

	/* Graphic Line Commands */
	{
		// TODO this could be another thread and run at a lower rate
		hasGfx = true;

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.context.linePipeLayout, VK_PIPE_SET_INDEX_LINE_GLOBAL, 1, &globalSet, 0, NULL);

		vkCmdSetLineWidth(gfxCmd, 1.0f);

		int cubeCount = 0;
		for (u32 iCstMode = MXC_COMPOSITOR_MODE_QUAD; iCstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCstMode) {
			atomic_thread_fence(memory_order_acquire);
			int activeNodeCt = node.active[iCstMode].count;
			if (activeNodeCt == 0)
				continue;

			auto pActiveNodes = &node.active[iCstMode];
			for (int iNode = 0; iNode < activeNodeCt; ++iNode) {
				auto pNodeCst = &cst.nodeData[iNode];
				memcpy(pLineMapped + (cubeCount * MXC_CUBE_SEGMENT_COUNT), pNodeCst->worldLineSegments, sizeof(VkLineVert) * MXC_CUBE_SEGMENT_COUNT);
				cubeCount++;
			}
		}

		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){lineBuffer}, (VkDeviceSize[]){0});
		vkCmdDraw(gfxCmd, cubeCount * MXC_CUBE_SEGMENT_COUNT, 1, 0, 0);
	}

	vk.CmdEndRenderPass(gfxCmd);

	/* Compute Recording Cycle */
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_COMPUTE_RENDER_BEGIN);
	if (node.active[MXC_COMPOSITOR_MODE_COMPUTE].count > 0) {
		hasComp = true;

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_GLOBAL, 1, &globalSet,     0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_NODE,   1, &cst.nodeSet,   0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipeLayout, PIPE_SET_INDEX_NODE_COMPUTE_OUTPUT, 1, &compOutputSet, 0, NULL);

		auto pActiveNodes = &node.active[MXC_COMPOSITOR_MODE_COMPUTE];
		for (int iNode = pActiveNodes->count - 1; iNode >= 0; --iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			auto pNodeCstData = &cst.nodeData[hNode];
			vk.CmdPushConstants(gfxCmd, compPipeLayout, COMPOSITOR_AGGREGATE_STAGE_FLAGS, 0, sizeof(NodePush), &(NodePush){.nodeHandle = hNode});
			vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);
		}

		CMD_IMAGE_BARRIERS2(gfxCmd, {
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
			},
		});

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, postCompPipe);
		vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);
	}
	vk.CmdWriteTimestamp2(gfxCmd, VK_PIPELINE_STAGE_2_NONE, timeQryPool, TIME_QUERY_COMPUTE_RENDER_END);

	/* Blit Framebuffer */
	{
		u32 frameIdx;
		CmdSwapAcquire(device, pSwapCtx, &frameIdx);
		auto pSwap = &pSwapCtx->frames[frameIdx];

		CMD_IMAGE_BARRIERS2(gfxCmd, {
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
			},
		});

		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipe);
		CMD_BIND_DESCRIPTOR_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipeLayout, PIPE_SET_INDEX_FINAL_BLIT_GLOBAL, globalSet);
		CMD_PUSH_DESCRIPTOR_SETS2(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, finalBlitPipeLayout, PIPE_SET_INDEX_FINAL_BLIT_INOUT, {
			BIND_WRITE_FINAL_BLIT_SRC_GRAPHICS_FRAMEBUFFER(hasGfx ? gfxFrameColorView : VK_NULL_HANDLE, hasComp ? compFrameColorView : VK_NULL_HANDLE),
			BIND_WRITE_FINAL_BLIT_DST(pSwap->view),
		});
		vk.CmdDispatch(gfxCmd, 1, windowGroupCt, 1);

		CMD_IMAGE_BARRIERS2(gfxCmd,{
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
			},
		});
	}

	vk.EndCommandBuffer(gfxCmd);

	/*
	 * MXC_CYCLE_RENDER_COMPOSITE
	 */
	{
		// Signal will submit gfxCmd on main
		vkTimelineSignal(device, baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compTimeline);
	}

	/*
	 * MXC_CYCLE_UPDATE_WINDOW_STATE
	 */
	{
		u64 nextUpdateWindowStateCycle = baseCycleValue + MXC_CYCLE_COUNT + MXC_CYCLE_UPDATE_WINDOW_STATE;
		vkTimelineWait(device, nextUpdateWindowStateCycle, compTimeline);
		u64 timestampsNS[TIME_QUERY_COUNT];
		VK_CHECK(vk.GetQueryPoolResults(device, timeQryPool, 0, TIME_QUERY_COUNT, sizeof(u64) * TIME_QUERY_COUNT, timestampsNS, sizeof(u64), VK_QUERY_RESULT_64_BIT));
		double timestampsMS[TIME_QUERY_COUNT];
		for (u32 i = 0; i < TIME_QUERY_COUNT; ++i) timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
		timeQueryMs = timestampsMS[TIME_QUERY_COMPUTE_RENDER_END] - timestampsMS[TIME_QUERY_COMPUTE_RENDER_BEGIN];
	}

	CHECK_RUNNING;
	goto CompositeLoop;
}

////
//// Allocate
////
static void Allocate(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst)
{
//	VkShaderStageFlags stageFlags = CompositeModeShaderFlags(pInfo->pEnabledCompositorModes);
	CreateNodeSetLayout(COMPOSITOR_AGGREGATE_STAGE_FLAGS, &pCst->nodeSetLayout);
	CreateGraphicsNodePipeLayout(COMPOSITOR_AGGREGATE_STAGE_FLAGS, pCst->nodeSetLayout, &pCst->gfxPipeLayout);

	///
	/// Graphics Pipes
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_QUAD]) {
		vkCreateQuadMesh(0.5f, &pCst->quadMesh);
		vkCreateTrianglePipe("./shaders/basic_comp.vert.spv",
		                     "./shaders/basic_comp.frag.spv",
		                     vk.context.depthRenderPass,
		                     pCst->gfxPipeLayout,
		                     &pCst->gfxQuadPipe);
	}

	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_TESSELATION]) {
		vkCreateQuadPatchMeshSharedMemory(&pCst->quadPatchMesh);
		vkCreateTessellationPipe("./shaders/tess_comp.vert.spv",
			                     "./shaders/tess_comp.tesc.spv",
			                     "./shaders/tess_comp.tese.spv",
			                     "./shaders/tess_comp.frag.spv",
			                     vk.context.depthRenderPass,
			                     pCst->gfxPipeLayout,
			                     &pCst->gfxTessPipe);
	}

	//	if (pInfo->enabledCompositorModes[MXC_COMPOSITOR_MODE_TASK_MESH]) {
	//		vkCreateBasicTaskMeshPipe("./shaders/mesh_comp.task.spv",
	//								  "./shaders/mesh_comp.mesh.spv",
	//								  "./shaders/mesh_comp.frag.spv",
	//								  vk.context.basicPass,
	//								  pCst->nodePipeLayout,
	//								  &pCst->nodeTaskMeshPipe);
	//	}

	///
	/// Compute Pipe
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_COMPUTE]) {
		CreateComputeOutputSetLayout(&pCst->compOutputSetLayout);
		CreateNodeComputePipeLayout(COMPOSITOR_AGGREGATE_STAGE_FLAGS, pCst->nodeSetLayout, pCst->compOutputSetLayout, &pCst->compPipeLayout);
		vkCreateComputePipe("./shaders/compute_compositor.comp.spv", pCst->compPipeLayout, &pCst->compPipe);
		vkCreateComputePipe("./shaders/compute_post_compositor_basic.comp.spv", pCst->compPipeLayout, &pCst->postCompPipe);
		VK_SET_DEBUG(pCst->compPipe);
		VK_SET_DEBUG(pCst->postCompPipe);
	}

	///
	/// Line Pipe
	{
		pCst->lineCapacity = MXC_CUBE_SEGMENT_COUNT * MXC_NODE_CAPACITY;
		VkRequestAllocationInfo lineRequest = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size      = sizeof(VkLineVert) * pCst->lineCapacity,
			.usage     = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.locality  = VK_LOCALITY_CONTEXT,
			.dedicated = VK_DEDICATED_MEMORY_FALSE,
		};
		vkCreateSharedBuffer(&lineRequest, &pCst->lineBuffer);
	}

	///
	/// Pools
	{
		VkQueryPoolCreateInfo queryInfo = {
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType  = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = TIME_QUERY_COUNT,
		};
		VK_CHECK(vkCreateQueryPool(vk.context.device, &queryInfo, VK_ALLOC, &pCst->timeQryPool));

		VkDescriptorPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets = MXC_NODE_CAPACITY * 5,
			.poolSizeCount = 3,
			.pPoolSizes = (VkDescriptorPoolSize[]){
				{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = MXC_NODE_CAPACITY},
				{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MXC_NODE_CAPACITY * 2},
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = MXC_NODE_CAPACITY * 2},
			},
		};
		VK_CHECK(vkCreateDescriptorPool(vk.context.device, &poolInfo, VK_ALLOC, &threadContext.descriptorPool));
	}

	///
	/// Global
	{
		vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.globalSetLayout, &pCst->globalSet);
		VkRequestAllocationInfo requestInfo = {
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size                = sizeof(VkGlobalSetState),
			.usage               = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		};
		vkCreateSharedBuffer(&requestInfo, &pCst->globalBuffer);
	}

	///
	/// Final Blit
	{
		CreateFinalBlitSetLayout(&pCst->finalBlitSetLayout);
		CreateFinalBlitPipeLayout(pCst->finalBlitSetLayout, &pCst->finalBlitPipeLayout);
		vkCreateComputePipe("./shaders/compositor_final_blit.comp.spv", pCst->finalBlitPipeLayout, &pCst->finalBlitPipe);
		VK_SET_DEBUG(pCst->finalBlitPipeLayout);
		VK_SET_DEBUG(pCst->finalBlitPipe);
	}

	///
	/// Node Sets
	vkCreateSharedBuffer(&(VkRequestAllocationInfo){
			.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			.size = sizeof(MxcCompositorNodeSetState) * MXC_NODE_CAPACITY,
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		}, &cst.nodeSetBuffer);
	VK_SET_DEBUG(cst.nodeSetBuffer.buffer);

	vkAllocateDescriptorSets(vk.context.device, &(VkDescriptorSetAllocateInfo){
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &pCst->nodeSetLayout,
		}, &cst.nodeSet);
	VK_SET_DEBUG(cst.nodeSet);

	// for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
	// 	// Preallocate all Node Set buffers. MxcNodeCompositorSetState * 256 = 130 kb. Small price to pay to ensure contiguous memory on GPU
	// 	// TODO this should be a descriptor array
	// 	VkRequestAllocationInfo requestInfo = {
	// 		.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
	// 		.size                = sizeof(MxcCompositorNodeSetState),
	// 		.usage               = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	// 	};
		// vkCreateSharedBuffer(&requestInfo, &node.cst.data[i].compositingNodeSet.buffer);
	//
	// 	VkDescriptorSetAllocateInfo setCreateInfo = {
	// 		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	// 		.descriptorPool     = threadContext.descriptorPool,
	// 		.descriptorSetCount = 1,
	// 		.pSetLayouts        = &pCst->nodeSetLayout,
	// 	};
	// 	vkAllocateDescriptorSets(vk.context.device, &setCreateInfo, &node.cst.data[i].compositingNodeSet.set);
	// 	vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)node.cst.data[i].compositingNodeSet.set, "NodeSet");
	// }

	///
	/// Graphics Framebuffers
	{
		VkDepthFramebufferTextureCreateInfo framebufferTextureInfo = {
			.locality = VK_LOCALITY_CONTEXT,
			.extent   = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
		};
		vkCreateDepthFramebufferTextures(&framebufferTextureInfo, &pCst->framebufferTexture);
		VK_SET_DEBUG(pCst->framebufferTexture.color.view);
		VK_SET_DEBUG(pCst->framebufferTexture.color.image);
		VK_SET_DEBUG(pCst->framebufferTexture.color.memory);
		VK_SET_DEBUG(pCst->framebufferTexture.depth.view);
		VK_SET_DEBUG(pCst->framebufferTexture.depth.image);
		VK_SET_DEBUG(pCst->framebufferTexture.depth.memory);
	}

	int arr[] = { 1, 2, 3};

	///
	/// Compute Output
	{
		VkDedicatedTextureCreateInfo atomicCreateInfo = {
			.pImageCreateInfo =	&(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType   = VK_IMAGE_TYPE_2D,
					.format      = VK_FORMAT_R32_UINT,
					.extent      = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.mipLevels   = 1,
					.arrayLayers = 1,
					.samples     = VK_SAMPLE_COUNT_1_BIT,
					.usage       = VK_IMAGE_USAGE_STORAGE_BIT,
				},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality   = VK_LOCALITY_CONTEXT,
		};
		vkCreateDedicatedTexture(&atomicCreateInfo, &pCst->compFrameAtomicTex);
		VK_SET_DEBUG(pCst->compFrameAtomicTex.image);
		VK_SET_DEBUG(pCst->compFrameAtomicTex.view);
		VK_SET_DEBUG(pCst->compFrameAtomicTex.memory);

		VkDedicatedTextureCreateInfo colorCreateInfo = {
			.pImageCreateInfo =	&(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType   = VK_IMAGE_TYPE_2D,
					.format      = VK_FORMAT_R8G8B8A8_UNORM,
					.extent      = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.mipLevels   = 1,
					.arrayLayers = 1,
					.samples     = VK_SAMPLE_COUNT_1_BIT,
					.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = VK_LOCALITY_CONTEXT,
		};
		vkCreateDedicatedTexture(&colorCreateInfo, &pCst->compFrameColorTex);
		VK_SET_DEBUG(pCst->compFrameColorTex.image);
		VK_SET_DEBUG(pCst->compFrameColorTex.view);
		VK_SET_DEBUG(pCst->compFrameColorTex.memory);

		VkDescriptorSetAllocateInfo setInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = threadContext.descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &pCst->compOutputSetLayout,
		};
		VK_CHECK(vkAllocateDescriptorSets(vk.context.device, &setInfo, &pCst->compOutputSet));
		vkSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pCst->compOutputSet, "ComputeOutputSet");

		VK_UPDATE_DESCRIPTOR_SETS(
			BIND_WRITE_NODE_COMPUTE_ATOMIC_OUTPUT(pCst->compOutputSet, pCst->compFrameAtomicTex.view),
			BIND_WRITE_NODE_COMPUTE_COLOR_OUTPUT(pCst->compOutputSet, pCst->compFrameColorTex.view));

		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS) {
			VkImageMemoryBarrier2 barriers[] = {
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
			vkCmdImageBarriers(cmd, COUNT(barriers), barriers);
		}
	}
}

static void Bind(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst) {
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_QUAD]) {}
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_TESSELATION]) {
		vkBindUpdateQuadPatchMesh(0.5f, &pCst->quadPatchMesh);
	}
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_TASK_MESH]) {}
	if (pInfo->pEnabledCompositorModes[MXC_COMPOSITOR_MODE_COMPUTE]) {}

	vkBindSharedBuffer(&pCst->lineBuffer);
	pCst->pLineMapped = vkSharedBufferPtr(pCst->lineBuffer);

	vkBindSharedBuffer(&pCst->globalBuffer);
	VK_UPDATE_DESCRIPTOR_SETS(VK_BIND_WRITE_GLOBAL_BUFFER(pCst->globalSet, pCst->globalBuffer.buffer));

	vkBindSharedBuffer(&cst.nodeSetBuffer);
	cst.pNodeSetMapped = vkSharedBufferPtr(cst.nodeSetBuffer);
	memset(cst.pNodeSetMapped, 0, sizeof(MxcCompositorNodeSetState) * MXC_NODE_CAPACITY);

	VkDescriptorBufferInfo bufferInfos[MXC_NODE_CAPACITY];
	VkDescriptorImageInfo  colorInfos[MXC_NODE_CAPACITY];
	VkDescriptorImageInfo  gbufferInfos[MXC_NODE_CAPACITY];

	for (int i = 0; i < MXC_NODE_CAPACITY; ++i) {
		bufferInfos[i] = (VkDescriptorBufferInfo){
			.buffer      = cst.nodeSetBuffer.buffer,
			.offset      = i * sizeof(MxcCompositorNodeSetState),
			.range       = sizeof(MxcCompositorNodeSetState),
		};
		colorInfos[i] = (VkDescriptorImageInfo){
			.sampler     = vk.context.nearestSampler,
			.imageView   = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		gbufferInfos[i] = (VkDescriptorImageInfo){
			.sampler     = vk.context.nearestSampler,
			.imageView   = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
	}

	VK_UPDATE_DESCRIPTOR_SETS2({
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = cst.nodeSet,
			.dstBinding = SET_BIND_INDEX_NODE_STATE,
			.dstArrayElement = 0,
			.descriptorCount = MXC_NODE_CAPACITY,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = bufferInfos,
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = cst.nodeSet,
			.dstBinding = SET_BIND_INDEX_NODE_COLOR,
			.dstArrayElement = 0,
			.descriptorCount = MXC_NODE_CAPACITY,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = colorInfos,
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = cst.nodeSet,
			.dstBinding = SET_BIND_INDEX_NODE_GBUFFER,
			.dstArrayElement = 0,
			.descriptorCount = MXC_NODE_CAPACITY,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = gbufferInfos,
		},
	});
}

static void* CompositorThread(void*)
{
	MxcCompositorCreateInfo info = {
		.pEnabledCompositorModes = {
			[MXC_COMPOSITOR_MODE_QUAD] = true,
			[MXC_COMPOSITOR_MODE_TESSELATION] = true,
			[MXC_COMPOSITOR_MODE_TASK_MESH] = false,
			[MXC_COMPOSITOR_MODE_COMPUTE] = true,
		},
	};

	vkBeginAllocationRequests();
	Allocate(&info, &cst);
	vkEndAllocationRequests();
	Bind(&info, &cst);

	CompositorRun(&compositorContext, &cst);
	return NULL;
}

void mxcCreateAndRunCompositorThread(VkSurfaceKHR surface)
{
	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &compositorContext.timeline);
	compositorContext.timelineHandle = vkGetSemaphoreExternalHandle(compositorContext.timeline);
	VK_SET_DEBUG(compositorContext.timeline);

	vkCreateSwapContext(surface, VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorContext.swapCtx);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &compositorContext.gfxPool));
	VK_SET_DEBUG(compositorContext.gfxPool);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = compositorContext.gfxPool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &compositorContext.gfxCmd));
	VK_SET_DEBUG(compositorContext.gfxCmd);

	CHECK(pthread_create(&compositorContext.threadId, NULL, (void* (*)(void*))CompositorThread, NULL), "Compositor Node thread creation failed!");

	LOG("Waiting for Compositor init.\n");
	// TODO need to come up with some better way for other threads to the CmdQueue when the main thread isn't running vkSubmitQueuedCommandBuffers yet
	while (!atomic_load(&compositorContext.isReady)) {
		vkSubmitQueuedCommandBuffers();
	}

	LOG("Compositor Create and Run Thread Success.\n");
}

#endif