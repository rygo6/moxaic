#include "mid_window.h"

#include "window.h"

MxcWindowInput mxcWindowInput;

void mxcProcessWindowInput() {
	mxcWindowInput.move[0] = midWindowInput.MID_KEY_W == MID_PHASE_PRESS;
	mxcWindowInput.move[1] = midWindowInput.MID_KEY_S == MID_PHASE_PRESS;
	mxcWindowInput.move[2] = midWindowInput.MID_KEY_A == MID_PHASE_PRESS;
	mxcWindowInput.move[3] = midWindowInput.MID_KEY_D == MID_PHASE_PRESS;
	mxcWindowInput.move[4] = midWindowInput.MID_KEY_R == MID_PHASE_PRESS;
	mxcWindowInput.move[5] = midWindowInput.MID_KEY_F == MID_PHASE_PRESS;

	mxcWindowInput.leftMouseButton = midWindowInput.leftMouse == MID_PHASE_PRESS ||  midWindowInput.leftMouse == MID_PHASE_HELD;

  switch (midWindowInput.rightMouse) {
	  case MID_PHASE_PRESS:
		  midWindowLockCursor();
		  break;
	  case MID_PHASE_RELEASE:
		  midWindowReleaseCursor();
		  break;
	  default: break;
  }

  switch (midWindowInput.cursorLocked) {
	  case MID_INPUT_LOCK_CURSOR_ENABLED:
		  mxcWindowInput.mouseDelta.x = midWindowInput.mouseDeltaX;
		  mxcWindowInput.mouseDelta.y = midWindowInput.mouseDeltaY;
		  break;
	  default: break;
  }
}
