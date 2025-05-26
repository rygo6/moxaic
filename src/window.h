#pragma once

#include <stdbool.h>

#include "mid_math.h"

typedef struct MxcWindowInput {
	bool leftMouseButton;

	vec2 mouseDelta;

	// forward, back, left, right, up, down
	bool move[6];
} MxcWindowInput;

extern MxcWindowInput mxcWindowInput;

void mxcProcessWindowInput();
