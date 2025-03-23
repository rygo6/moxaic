#pragma once

#include "mid_vulkan.h"

#include "node.h"

typedef struct MxcTestNode {
	VkRenderPass     nodeRenderPass;
	VkPipelineLayout pipeLayout;
	VkPipeline       basicPipe;

	VkFramebuffer framebuffer;
	VkDedicatedTexture normalFramebuffers[VK_SWAP_COUNT * 2];

	VkDescriptorSetLayout nodeProcessSetLayout;
	VkPipelineLayout      nodeProcessPipeLayout;
	VkPipeline            nodeProcessBlitMipAveragePipe;
	VkPipeline            nodeProcessBlitDownPipe;

	VkDevice device;

	VkGlobalSetState*     pGlobalMapped;
	VkSharedDescriptorSet globalSet;

	VkDescriptorSet checkerMaterialSet;
	VkDescriptorSet sphereObjectSet;

	VkDedicatedTexture checkerTexture;

	VkMesh  sphereMesh;
	MidPose sphereTransform;

	VkObjectSetState  sphereObjectState;
	VkObjectSetState* pSphereObjectSetMapped;
	VkDeviceMemory    sphereObjectSetMemory;
	VkBuffer          sphereObjectSetBuffer;

} MxcTestNode;

void* mxcTestNodeThread(MxcNodeContext* pNodeContext);
