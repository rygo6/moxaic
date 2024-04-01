#include "globals.h"
#include "renderer.h"
#include "window.h"

#include <assert.h>

bool IsCompositor = 1;

int main(void) {

  mxCreateWindow();

  vkInitContext();

  return 0;
}
