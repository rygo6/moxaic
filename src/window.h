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

  bool debugSwap;

} Input;

extern Input input;

void vkmUpdateWindowInput();
void vkmCreateWindow();
void vkmCreateSurface(const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

extern double timeQueryMs;