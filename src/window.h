#pragma once

#include <stdbool.h>

#include "mid_math.h"

typedef enum MxMoveDirection {
	MXC_MOVE_DIRECTION_FORWARD,
	MXC_MOVE_DIRECTION_BACK,
	MXC_MOVE_DIRECTION_LEFT,
	MXC_MOVE_DIRECTION_RIGHT,
	MXC_MOVE_DIRECTION_UP,
	MXC_MOVE_DIRECTION_DOWN,
	MXC_MOVE_DIRECTION_COUNT,
} MxMoveDirection;

typedef struct MxcWindowInput {
	bool leftMouseButton;

	ivec2 iDimensions;
	vec2  fDimensions;

	ivec2 iMouseCoord;
	vec2  fMouseCoord;

	vec2 mouseDelta;

	vec2 mouseUVDelta;
	vec2 priorMouseUV;
	vec2 mouseUV;

	// forward, back, left, right, up, down
	bool move[6];
} MxcWindowInput;

extern MxcWindowInput mxcWindowInput;

void mxcProcessWindowInput();
