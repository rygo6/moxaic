#include "globals.h"
#include "renderer.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;

void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "%s:%d Error! %s\n", file, line, message);
  abort();
}

int main(void) {
  mxCreateWindow();
  mxcInitContext();

  if (mxcRenderNode()) {
    return 1;
  }

  return 0;
}
