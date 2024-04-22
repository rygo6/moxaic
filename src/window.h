#pragma once

#include <stdbool.h>
#include <vulkan/vulkan.h>

typedef struct Input {
  float mouseDeltaX;
  float mouseDeltaY;
  bool  mouseLocked;

  bool moveForward;
  bool moveBack;
  bool moveRight;
  bool moveLeft;

} Input;

extern Input input;

void     mxUpdateWindowInput();
void     mxCreateWindow();
VkResult mxcCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
