#if defined(MOXAIC_COMPOSITOR) // we need diferent src list for comp and node

#pragma once

#include "mid_vulkan.h"

#include "node.h"

typedef struct MxcTestNode {

	VkDevice device;

	VkSharedBuffer  globalBuffer;
	VkDescriptorSet globalSet;

	VkDescriptorSet checkerMaterialSet;
	VkDescriptorSet sphereObjectSet;
	VkDedicatedTexture checkerTexture;

	VkMesh  sphereMesh;
	pose    sphereTransform;

	VkObjectSetState  sphereObjectState;
	VkObjectSetState* pSphereObjectSetMapped;
	VkDeviceMemory    sphereObjectSetMemory;
	VkBuffer          sphereObjectSetBuffer;

	VkDepthFramebufferTexture framebufferTexture;

} MxcTestNode;

void* mxcTestNodeThread(MxcNodeContext* pNode);

#endif