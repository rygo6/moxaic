#pragma once

#include <vulkan/vulkan.h>

void mxUpdateWindow();
void mxCreateWindow();
VkResult mxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
