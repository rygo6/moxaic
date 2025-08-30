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

typedef enum MxcView {
	MXC_VIEW_UNINITIALIZED = 0,
	MXC_VIEW_MONO = 1,
	MXC_VIEW_STEREO = 2,
} MxcView;

extern MxcView compositorView;
extern bool isCompositor;

extern bool isRunning;
#define CHECK_RUNNING                 \
	if (UNLIKELY(!isRunning)) return;

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


//typedef uint16_t arena_offset;
//
//extern void*    static_arena;
//static uint8_t  dynamic_arena[1 << 16];
//static uint16_t dynamic_arena_end;
//
//#define A_OFFSET(arena, field)     (void*)field - (void*)arena
//#define A_PTR(arena, offset, type) ((type*)((void*)&arena + offset))
//#define STC_A_PTR(offset, type)    ((type*)(static_arena + offset))

//struct static_arena_memory {
//  VkmInstance instance;
//  VkmContext  context;
//};
//void* static_arena = &(struct static_arena_memory){};


//  assert(sizeof(struct static_arena_memory) <= 1 << 16);
//  arena_offset aInstance = offsetof(struct static_arena_memory, instance);