#include "globals.h"
#include "renderer.h"
#include "window.h"

bool IsCompositor = 1;

int main(void) {

  if (mxCreateWindow()) {
    return 1;
  }
  if (mxcInitContext()) {
    return 1;
  }
  if (mxcRenderNode()) {
    return 1;
  }

  return 0;
}
