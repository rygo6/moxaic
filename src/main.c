#include "globals.h"
#include "renderer.h"
#include "window.h"

bool isCompositor = true;
bool isRunning = true;

int main(void) {

  mxCreateWindow();
  mxcInitContext();

  if (mxcRenderNode()) {
    return 1;
  }

  return 0;
}
