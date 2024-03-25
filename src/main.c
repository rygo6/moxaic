#include "renderer.h"
#include "window.h"
#include "constants.h"

char g_IsCompositor = 1;

int main(void) {

  MxCreateWindow();
  MxRendererInit();

  return 0;
}
