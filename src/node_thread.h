#if defined(MOXAIC_COMPOSITOR) // we need diferent src list for comp and node

#pragma once

#include "mid_vulkan.h"

#include "node.h"

typedef struct MxcNodeThread {

	VkGlobalSetState* pGlobalSetMapped;
	VkSharedBuffer  globalBuffer;
	VkDescriptorSet globalSet;

	VkDescriptorSet checkerMaterialSet;
	VkDescriptorSet sphereObjectSet;
	VkDedicatedTexture checkerTexture;

	VkMesh  sphereMesh;
	MidPose    sphereTransform;

	VkObjectSetState  sphereObjectState;
	VkObjectSetState* pSphereObjectSetMapped;
	VkDeviceMemory    sphereObjectSetMemory;
	VkBuffer          sphereObjectSetBuffer;

	VkDedicatedTexture depthFramebufferTexture;

} MxcNodeThread;

void* mxcRunNodeThread(void* nodeHandle);

#endif