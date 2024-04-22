#include "globals.h"
#include "renderer.h"
#include "window.h"
#include "test_node.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;

void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  abort();
}

int main(void) {
  mxCreateWindow();
  const MvkContext* pContext = mxcInitRendererContext();
  mxcTestNodeInit(pContext);

  while (isRunning) {
    mxcTestNodeUpdate();
  }

  return 0;
}
