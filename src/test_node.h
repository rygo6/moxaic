#pragma once

#include "renderer.h"

typedef struct MxcTestNodeCreateInfo {
//  VkmContext
  VkSurfaceKHR surface;
  VkmTransform transform;
} MxcTestNodeCreateInfo;

void mxcUpdateTestNode();
void mxcCreateTestNode(void* pArg);