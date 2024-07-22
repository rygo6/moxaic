#pragma once
#include <stdbool.h>
#include "mid_math.h"

typedef struct MxcInput {
  vec2 mouseDelta;
  // forward, back, left, right
  bool move[4];
} MxcInput;

extern MxcInput mxcInput;

void mxcUpdateWindowInput();
