#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef struct MxcSwapTexture {
	VkExternalTexture externalTexture[XR_SWAPCHAIN_IMAGE_COUNT];
	XrSwapState       state;
	XrSwapInfo        info;
} MxcSwapTexture;

// Hot data used by the compositor for each node
// should I call it Hot or Data?
// Context = Cold. Data = Hot?
typedef struct MxcCompositorNodeData {

	MxcNodeInteractionState interactionState;
	MxcCompositorMode       activeCompositorMode;
	MxcNodeInterprocessMode activeInterprocessMode;

	//	pose rootPose;
	u64 lastTimelineValue;

	// NodeSet which node is actively using to render
	MxcCompositorNodeSetState renderingNodeSetState;
	MxcCompositorNodeSetState compositingNodeSetState;
	// This should go in one big shared buffer for all nodes
	// VkSharedDescriptor         compositingNodeSet;
	// MxcCompositorNodeSetState* pCompositingNodeSetMapped;

	MxcNodeSwap swaps[XR_SWAPCHAIN_CAPACITY][XR_SWAPCHAIN_IMAGE_COUNT];

	MxcNodeGBuffer gbuffer[XR_MAX_VIEW_COUNT];
	//	} gbuffer[XR_MAX_VIEW_COUNT][XR_SWAPCHAIN_IMAGE_COUNT];
	// If I ever need to retain gbuffers, I will need more gbuffers.
	// However, as long as gbuffer process is on the compositor thread
	// it can sync a single gbuffer with composite render loop

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
		BLOCK_DECL(MxcSwapTexture, MXC_NODE_CAPACITY) swap;
	} block;

} MxcCompositor;

// Should CompositorContext and Compositor merge into one!?

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

	bool isReady;

} MxcCompositorContext;

extern MxcCompositorContext compositorContext;

extern MxcCompositor cst;

typedef struct MxcCompositorCreateInfo {
	bool pEnabledCompositorModes[MXC_COMPOSITOR_MODE_COUNT];
} MxcCompositorCreateInfo;

void mxcCreateAndRunCompositorThread(VkSurfaceKHR surface);
