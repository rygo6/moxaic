#include "renderer.h"
#include "window.h"
#include "globals.h"

char IsCompositor = 1;

int main(void) {

  mxCreateWindow();
  mxRendererInitContext();

  return 0;
}
