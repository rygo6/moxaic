#pragma once

#include <vulkan/vulkan.h>

int MxCreateWindow();
int MxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
