#include "renderer.h"
#include "window.h"
#include "constants.h"

char kIsCompositor = 1;

int main(void) {

  mxCreateWindow();
  mxRendererInit();

  return 0;
}
