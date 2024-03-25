#pragma once

#include <vulkan/vulkan.h>

int mxCreateWindow();
int mxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
