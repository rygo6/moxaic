#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __JETBRAINS_IDE__
#define MID_IDE_ANALYSIS
#endif

#define MID_DEBUG
extern void Panic(const char* file, int line, const char* message);
#define PANIC(_message) Panic(__FILE__, __LINE__, _message)
#define CHECK(_err, _message)                      \
	if (__builtin_expect(!!(_err), 0)) {           \
		fprintf(stderr, "Error Code: %d\n", _err); \
		PANIC(_message);                           \
	}

#define CACHE_ALIGN __attribute((aligned(64)))
#define PACKED __attribute__((packed))
#define ALIGN(size) __attribute((aligned(size)))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COUNT(_array) (sizeof(_array) / sizeof(_array[0]))
#define CONTAINS(_array, _count, _)        \
	({                                     \
		bool found = false;                \
		for (int i = 0; i < _count; ++i) { \
			if (_array[i] == _) {          \
				found = true;              \
				break;                     \
			}                              \
		}                                  \
		found;                             \
	})

#define CONCAT(_a, _b) #_a #_b

#define DEFAULT_WIDTH 1024
#define DEFAULT_HEIGHT 1024

#define HOT    __attribute__((hot))
#define INLINE __attribute__((always_inline)) static inline

// to mid common
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef _Float16 f16;
typedef float    f32;
typedef double   f64;

typedef enum MxcView {
	MXC_VIEW_UNINITIALIZED = 0,
	MXC_VIEW_MONO = 1,
	MXC_VIEW_STEREO = 2,
} MxcView;

extern MxcView compositorView;
extern bool isCompositor;

extern bool isRunning;
#define CHECK_RUNNING                      \
  if (__builtin_expect(!(isRunning), 0)) { \
    return;                                \
  }

typedef enum MxcCycle {
  MXC_CYCLE_UPDATE_WINDOW_STATE,  // update window input, submit queues
  MXC_CYCLE_PROCESS_INPUT,        // process input for nodes/comp to read
  MXC_CYCLE_UPDATE_NODE_STATES,   // update state for nodes to render
  MXC_CYCLE_COMPOSITOR_RECORD,     // recording compositor commands
  MXC_CYCLE_RENDER_COMPOSITE,     // compositor render
  MXC_CYCLE_COUNT,
} MxcCycle;

typedef enum MxcPostCycle {
  MXC_CYCLE_POST_RENDER_COMPOSITE_COMPLETE = MXC_CYCLE_UPDATE_WINDOW_STATE,
  MXC_CYCLE_POST_WINDOW_UPDATE_COMPLETE = MXC_CYCLE_PROCESS_INPUT,
  MXC_CYCLE_POST_PROCESS_INPUT_COMPLETE = MXC_CYCLE_UPDATE_NODE_STATES,
  MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE = MXC_CYCLE_COMPOSITOR_RECORD,
  MXC_CYCLE_POST_COMPOSITOR_RECORD_COMPLETE = MXC_CYCLE_RENDER_COMPOSITE,
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