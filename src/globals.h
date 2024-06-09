#pragma once

#include <stdbool.h>
#include <stdint.h>

extern void Panic(const char* file, int line, const char* message);
#define PANIC(message) Panic(__FILE__, __LINE__, message)
#define REQUIRE(condition, message)        \
  if (__builtin_expect(!(condition), 0)) { \
    PANIC(message);                        \
  }

#define CACHE_ALIGN  __attribute((aligned(64)))
#define ALIGN(size)  __attribute((aligned(size)))
#define COUNT(array) sizeof(array) / sizeof(array[0])

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 1024

#define HOT    __attribute__((hot))
#define LOCAL  _Thread_local
#define INLINE __attribute__((always_inline)) inline

extern bool          isCompositor;
extern volatile bool isRunning;

#define CHECK_RUNNING                      \
  if (__builtin_expect(!(isRunning), 0)) { \
    return;                                \
  }

typedef enum MxcCycle {
  MXC_CYCLE_UPDATE_WINDOW_STATE, // update window input, submit queues
  MXC_CYCLE_PROCESS_INPUT, // process input for nodes/comp to read
  MXC_CYCLE_UPDATE_NODE_STATES, // update state for nodes to render
  MXC_CYCLE_RECORD_COMPOSITE, // recording compositor commands
  MXC_CYCLE_RENDER_COMPOSITE, // compositor render
  MXC_CYCLE_COUNT
} MxcCycle;


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