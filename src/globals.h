#pragma once

#include <stdbool.h>

#define PANIC(message)                                      \
  printf("%s:%d Error! %s\n", __FILE__, __LINE__, message); \
  exit(1);
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