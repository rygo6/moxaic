#pragma once

#include <stdbool.h>

#include "mid_math.h"

typedef struct MxcInput {
  vec2 mouseDelta;
  // forward, back, left, right, up, down
  bool move[6];
} MxcInput;

extern MxcInput mxcInput;

void mxcProcessWindowInput();
