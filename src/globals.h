#pragma once

#include <stdbool.h>

extern void Panic(const char* file, int line, const char* message);
#define PANIC(message) Panic(__FILE__, __LINE__, message)
#define REQUIRE(condition, message)        \
  if (__builtin_expect(!(condition), 0)) { \
    PANIC(message);                        \
  }

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 1024

extern bool isCompositor;
extern bool isRunning;

extern const char* WindowExtensionName;
extern const char* ExternalMemoryExntensionName;
extern const char* ExternalSemaphoreExntensionName;
extern const char* ExternalFenceExntensionName;