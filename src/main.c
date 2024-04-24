#include "globals.h"
#include "renderer.h"
#include "test_node.h"
#include "window.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;

//struct static_arena_memory {
//  VkmInstance instance;
//  VkmContext  context;
//};
//void* static_arena = &(struct static_arena_memory){};

void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  abort();
}

int main(void) {
  //  assert(sizeof(struct static_arena_memory) <= 1 << 16);
  //  arena_offset aInstance = offsetof(struct static_arena_memory, instance);

  vkmCreateWindow();
  vkmCreateInstance(isCompositor);
  const VkmContextCreateInfo contextCreateInfo = {
      .highPriority = isCompositor,
      .createGraphicsCommand = true,
      .createTransferCommand = true,
      .createComputeCommand = true,
  };
  vkmCreateContext(&contextCreateInfo);

  mxcCreateTestNodeContext();

  while (isRunning) {
    vkmUpdateWindowInput();

    mxcTestNodeUpdate();
  }

  return 0;
}
