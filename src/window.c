#include "mid_window.h"

#include "window.h"

MxcWindowInput mxcWindowInput;

void mxcProcessWindowInput()
{
	mxcWindowInput.move[MXC_MOVE_DIRECTION_FORWARD] = midWindowInput.MID_KEY_W == MID_PHASE_PRESS;
	mxcWindowInput.move[MXC_MOVE_DIRECTION_BACK] = midWindowInput.MID_KEY_S == MID_PHASE_PRESS;
	mxcWindowInput.move[MXC_MOVE_DIRECTION_LEFT] = midWindowInput.MID_KEY_A == MID_PHASE_PRESS;
	mxcWindowInput.move[MXC_MOVE_DIRECTION_RIGHT] = midWindowInput.MID_KEY_D == MID_PHASE_PRESS;
	mxcWindowInput.move[MXC_MOVE_DIRECTION_UP] = midWindowInput.MID_KEY_R == MID_PHASE_PRESS;
	mxcWindowInput.move[MXC_MOVE_DIRECTION_DOWN] = midWindowInput.MID_KEY_F == MID_PHASE_PRESS;

	mxcWindowInput.leftMouseButton = midWindowInput.leftMouse == MID_PHASE_PRESS || midWindowInput.leftMouse == MID_PHASE_HELD;

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
			mxcWindowInput.mouseDelta.x = midWindowInput.fMouseDeltaX;
			mxcWindowInput.mouseDelta.y = midWindowInput.fMouseDeltaY;
			break;
		default: break;
	}

	mxcWindowInput.iMouseCoord = IVEC2(midWindowInput.iMouseX, midWindowInput.iMouseY);
	mxcWindowInput.fMouseCoord = VEC2(midWindowInput.iMouseX, midWindowInput.iMouseY);
	mxcWindowInput.fDimensions = VEC2(midWindow.viewWidth, midWindow.viewHeight);
	mxcWindowInput.mouseUV.vec = mxcWindowInput.fMouseCoord.vec / mxcWindowInput.fDimensions.vec;
}
