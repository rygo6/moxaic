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

  double deltaTime;

} Input;

extern Input input;

void     vkmUpdateWindowInput();
void     vkmCreateWindow();
VkResult vkmCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface);
