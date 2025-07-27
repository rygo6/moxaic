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

} MxcTestNode;

void* mxcTestNodeThread(MxcNodeContext* pNode);
