#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef struct MxcSwapTexture {
	VkExternalTexture externalTexture[XR_SWAPCHAIN_IMAGE_COUNT];
	XrSwapInfo        info;
} MxcSwapTexture;

// Context = Cold. Data = Hot
typedef struct MxcCompositorNodeData {

	MxcNodeInteractionState interactionState;
	MxcCompositorMode       activeCompositorMode;
	MxcNodeInterprocessMode activeInterprocessMode;

	u64 lastTimelineValue;

	// NodeSet which node is actively using to render
	MxcCompositorNodeSetState renderingNodeSetState;
	MxcCompositorNodeSetState compositingNodeSetState;

	MxcNodeSwap swaps[XR_SWAPCHAIN_CAPACITY][XR_SWAPCHAIN_IMAGE_COUNT];

	MxcNodeGBuffer gbuffer[XR_MAX_VIEW_COUNT];

	// this should go a UI thread node
	VkLineVert worldLineSegments[MXC_CUBE_SEGMENT_COUNT];
	vec3       worldCorners[CORNER_COUNT];
	vec2       uvCorners[CORNER_COUNT];

} MxcCompositorNodeData;

typedef struct MxcCompositor {

	MxcCompositorNodeData nodeData[MXC_NODE_CAPACITY];

	VkDescriptorSetLayout nodeSetLayout;

	VkPipelineLayout      gfxPipeLayout;
	VkPipeline            gfxQuadPipe;
	VkPipeline            gfxTessPipe;
	VkPipeline            nodeTaskMeshPipe;

	VkDescriptorSetLayout compOutputSetLayout;
	VkPipelineLayout      compPipeLayout;
	VkPipeline            compPipe;
	VkPipeline            postCompPipe;

	VkDescriptorSetLayout finalBlitSetLayout;
	VkPipelineLayout      finalBlitPipeLayout;
	VkPipeline            finalBlitPipe;

	VkSharedBuffer  globalBuffer;
	VkDescriptorSet globalSet;

	MxcCompositorNodeSetState* pNodeSetMapped;
	VkSharedBuffer             nodeSetBuffer;
	VkDescriptorSet            nodeSet;

	VkMesh       quadMesh;
	VkSharedMesh quadPatchMesh;

	VkDepthFramebufferTexture framebufferTexture;

	VkDedicatedTexture compFrameAtomicTex;
	VkDedicatedTexture compFrameColorTex;
	VkDescriptorSet    compOutputSet;

	VkQueryPool timeQryPool;

	VkSharedBuffer lineBuffer;
	int            lineCapacity;
	VkLineVert*    pLineMapped;

	struct {
		BLOCK_T_N(MxcSwapTexture, MXC_NODE_CAPACITY) swap;
	} block;

} MxcCompositor;

// Should CompositorContext and Compositor merge into one!? probably

typedef struct MxcCompositorContext {
	// read by multiple threads
	VkCommandBuffer gfxCmd;
	u64             baseCycleValue;
	VkSemaphore     timeline;
	VkSwapContext   swapCtx;

	// cold data
	VkCommandPool gfxPool;
	pthread_t     threadId;

	HANDLE timelineHandle;

	_Atomic bool isReady;

} MxcCompositorContext;

extern MxcCompositorContext compositorContext;
extern MxcCompositor cst;

typedef struct MxcCompositorCreateInfo {
	bool pEnabledCompositorModes[MXC_COMPOSITOR_MODE_COUNT];
} MxcCompositorCreateInfo;

void mxcCreateAndRunCompositorThread(VkSurfaceKHR surface);
void mxcClearNodeDescriptorSet(node_h hNode);
