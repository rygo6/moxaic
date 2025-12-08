#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __clang__
#define ATOMIC _Atomic
#elif __GNUC__
#define ATOMIC
#endif

#ifdef __JETBRAINS_IDE__
#define MID_IDE_ANALYSIS
#endif

#define VK_MAX_VIEWPORT_WIDTH 4096
#define VK_MAX_VIEWPORT_HEIGHT 4096

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 1280

#define DEFAULT_WINDOW_X_POSITION 0
#define DEFAULT_WINDOW_Y_POSITION 0

typedef enum MXC_LIFECYCLE {
	MXC_LIFECYCLE_NONE,
	MXC_LIFECYCLE_INITIALIZING,
	MXC_LIFECYCLE_RUNNING,
	MXC_LIFECYCLE_EXITING,
	MXC_LIFECYCLE_COUNT,
} MXC_LIFECYCLE;

typedef enum MxcCycle {
  MXC_CYCLE_UPDATE_WINDOW_STATE, // update window input, submit queues
  MXC_CYCLE_PROCESS_INPUT,       // process input for nodes/comp to read
  MXC_CYCLE_UPDATE_NODE_STATES,  // update state for nodes to render
  MXC_CYCLE_COMPOSITOR_RECORD,   // recording compositor commands
  MXC_CYCLE_RENDER_COMPOSITE,    // compositor render
  MXC_CYCLE_COUNT,
} MxcCycle;

typedef enum MxcPostCycle {
  MXC_CYCLE_POST_RENDER_COMPOSITE_COMPLETE   = MXC_CYCLE_UPDATE_WINDOW_STATE,
  MXC_CYCLE_POST_WINDOW_UPDATE_COMPLETE      = MXC_CYCLE_PROCESS_INPUT,
  MXC_CYCLE_POST_PROCESS_INPUT_COMPLETE      = MXC_CYCLE_UPDATE_NODE_STATES,
  MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE = MXC_CYCLE_COMPOSITOR_RECORD,
  MXC_CYCLE_POST_COMPOSITOR_RECORD_COMPLETE  = MXC_CYCLE_RENDER_COMPOSITE,
  MXC_CYCLE_POST_COUNT
} MxcPostCycle;

extern struct Mxc {
	_Atomic MXC_LIFECYCLE lifecycle;
	bool isCompositor;
} mxc;

#define CHECK_RUNNING \
	if (UNLIKELY(ATOMIC_GET(mxc.lifecycle) == MXC_LIFECYCLE_EXITING)) return;

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