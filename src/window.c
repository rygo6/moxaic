#include "window.h"
#include "mid_window.h"

MxcInput mxcInput;

void mxcProcessWindowInput() {
  mxcInput.move[0] = midWindowInput.MID_KEY_W == MID_PHASE_PRESS;
  mxcInput.move[1] = midWindowInput.MID_KEY_S == MID_PHASE_PRESS;
  mxcInput.move[2] = midWindowInput.MID_KEY_A == MID_PHASE_PRESS;
  mxcInput.move[3] = midWindowInput.MID_KEY_D == MID_PHASE_PRESS;
  mxcInput.move[4] = midWindowInput.MID_KEY_R == MID_PHASE_PRESS;
  mxcInput.move[5] = midWindowInput.MID_KEY_F == MID_PHASE_PRESS;

  mxcInput.mouseDelta.x = midWindowInput.mouseDeltaX;
  mxcInput.mouseDelta.y = midWindowInput.mouseDeltaY;

  if (midWindowInput.leftMouse == MID_PHASE_PRESS) {
    midWindowLockCursor();
  } else if (midWindowInput.leftMouse == MID_PHASE_RELEASE) {
    midWindowReleaseCursor();
  }
}
