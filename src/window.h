#pragma once

#include <vulkan/vulkan.h>

void mxUpdateWindow();
void mxCreateWindow();
int mxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
