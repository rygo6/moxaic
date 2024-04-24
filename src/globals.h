#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t arena_offset;

extern void* static_arena;
static uint8_t dynamic_arena[1 << 16];
static uint16_t dynamic_arena_end;

#define A_OFFSET(arena, field) (void*)field - (void*)arena
#define A_PTR(arena, offset, type) ((type*)((void*)& arena + offset))
#define STC_A_PTR(offset, type) ((type*)(static_arena + offset))

extern void Panic(const char* file, int line, const char* message);
#define PANIC(message) Panic(__FILE__, __LINE__, message)
#define REQUIRE(condition, message)        \
  if (__builtin_expect(!(condition), 0)) { \
    PANIC(message);                        \
  }

#define CACHE_ALIGN __attribute((aligned(64)))
#define COUNT(array) sizeof(array) / sizeof(array[0])

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 1024

extern bool isCompositor;
extern bool isRunning;

extern const char* WindowExtensionName;
extern const char* ExternalMemoryExtensionName;
extern const char* ExternalSemaphoreExtensionName;
extern const char* ExternalFenceExtensionName;