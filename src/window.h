#pragma once

#include <vulkan/vulkan.h>

void mxUpdateWindow();
void mxCreateWindow();
VkResult mxcCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
