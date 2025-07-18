#pragma once

#include "mid_common.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//////////////////////
//// Mid Window Header
////
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 1024
#endif
#ifndef DEFAULT_WIDTH
#define DEFAULT_HEIGHT 1024
#endif
#ifndef DEFAULT_WINDOW_X_POSITION
#define DEFAULT_WINDOW_X_POSITION CW_USEDEFAULT
#endif
#ifndef DEFAULT_WINDOW_Y_POSITION
#define DEFAULT_WINDOW_Y_POSITION CW_USEDEFAULT
#endif

typedef struct MidWindow {
	HINSTANCE hInstance;
	HWND      hWnd;
	bool      windowResized;
	int       windowWidth, windowHeight;
	int       clientWidth, clientHeight;
	POINT     localCenter, globalCenter;
	uint64_t  frequency, start, current;
	bool      running;
} MidWindow;

typedef enum MidInputPhase : u8 {
	MID_PHASE_NONE,
	MID_PHASE_PRESS,
	MID_PHASE_HELD,
	MID_PHASE_RELEASE,
	MID_PHASE_CANCEL,
	MID_PHASE_DOUBLE_CLICK,
	MID_PHASE_COUNT,
} MidInputPhase;

typedef enum MidInputLock : u8 {
	MID_INPUT_LOCK_CURSOR_DISABLED,
	MID_INPUT_LOCK_CURSOR_ENABLED,
	MID_INPUT_LOCK_COUNT,
} MidInputLock;

typedef struct MidWindowInput {

	int iMouseX;
	int iMouseY;
	int iMouseDeltaX;
	int iMouseDeltaY;

	float fMouseX;
	float fMouseY;
	float fMouseDeltaX;
	float fMouseDeltaY;

	MidInputPhase leftMouse;
	MidInputPhase rightMouse;
	MidInputPhase middleMouse;

	MidInputPhase keyChar['Z' - '0'];

	double deltaTime;

	MidInputLock cursorLocked;

} MidWindowInput;

#define MID_KEY_A keyChar['A' - '0']
#define MID_KEY_D keyChar['D' - '0']
#define MID_KEY_F keyChar['F' - '0']
#define MID_KEY_R keyChar['R' - '0']
#define MID_KEY_S keyChar['S' - '0']
#define MID_KEY_W keyChar['W' - '0']

extern MidWindow      midWindow;
extern MidWindowInput midWindowInput;

void midUpdateWindowInput();
void midCreateWindow();

void midWindowLockCursor();
void midWindowReleaseCursor();

static inline uint64_t midQueryPerformanceCounter()
{
	LARGE_INTEGER value = {};
	QueryPerformanceCounter(&value);
	value.QuadPart *= 1000000;
	value.QuadPart /= midWindow.frequency;
	return value.QuadPart;
}

extern double timeQueryMs;

//////////////////////////////
//// Mid Window Implementation
////
#if defined(MID_WINDOW_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windowsx.h>

#include <stdio.h>

#define WINDOW_NAME "moxaic"
#define CLASS_NAME  "MoxaicWindowClass"

MidWindowInput midWindowInput;
MidWindow      midWindow;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {

		case WM_SIZE: if (wParam != SIZE_MINIMIZED) {
				RECT clientRect;
				GetClientRect(hWnd, &clientRect);
				if (midWindow.clientWidth != clientRect.right ||
					midWindow.clientHeight != clientRect.bottom)
					midWindow.windowResized = true;
				midWindow.clientWidth = clientRect.right;
				midWindow.clientHeight = clientRect.bottom;

				RECT windowRect;
				GetWindowRect(hWnd, &windowRect);
				if (midWindow.windowWidth != windowRect.right - windowRect.left ||
					midWindow.windowHeight != windowRect.bottom - windowRect.top)
					midWindow.windowResized = true;
				midWindow.windowWidth = windowRect.right - windowRect.left;
				midWindow.windowHeight = windowRect.bottom - windowRect.top;
			}

			return 0;

		case WM_SETCURSOR:

			switch (LOWORD(lParam)) {
				case HTCLIENT:
					SetCursor(LoadCursor(NULL, IDC_ARROW));  // Standard arrow
					// SetCursor(LoadCursor(NULL, IDC_HAND));   // Hand cursor
					// SetCursor(LoadCursor(NULL, IDC_CROSS));  // Crosshair
					return TRUE;
			}

			return DefWindowProc(hWnd, uMsg, wParam, lParam);

		case WM_MOUSEMOVE:
			int newX = GET_X_LPARAM(lParam);
			int newY = GET_Y_LPARAM(lParam);

			switch(midWindowInput.cursorLocked){
				case MID_INPUT_LOCK_CURSOR_ENABLED:
					midWindowInput.iMouseDeltaX = newX - midWindow.localCenter.x;
					midWindowInput.iMouseDeltaY = newY - midWindow.localCenter.y;
					SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);
					break;
				case MID_INPUT_LOCK_CURSOR_DISABLED:
					midWindowInput.iMouseDeltaX = newX - midWindowInput.iMouseX;
					midWindowInput.iMouseDeltaY = newY - midWindowInput.iMouseY;
					break;
				default:
					break;
			}

			midWindowInput.iMouseX = newX;
			midWindowInput.iMouseY = newY;

			midWindowInput.fMouseDeltaX = (float)midWindowInput.iMouseDeltaX;
			midWindowInput.fMouseDeltaY = (float)midWindowInput.iMouseDeltaY;
			midWindowInput.fMouseX = (float)midWindowInput.iMouseX;
			midWindowInput.fMouseY = (float)midWindowInput.iMouseY;

			return 0;

		case WM_KEYDOWN:
			if (wParam >= '0' && wParam <= 'Z')
				midWindowInput.keyChar[wParam - '0'] = MID_PHASE_PRESS;

			return 0;

		case WM_KEYUP:
			if (wParam >= '0' && wParam <= 'Z')
				midWindowInput.keyChar[wParam - '0'] = MID_PHASE_RELEASE;

			return 0;

		case WM_CLOSE:
			atomic_store_explicit(&midWindow.running, false, memory_order_release);
			return 0;

#define MOUSE_PHASE(macro_prefix, button_prefix)                      \
	case WM_##macro_prefix##BUTTONDOWN:                               \
		midWindowInput.button_prefix##Mouse = MID_PHASE_PRESS;        \
		return 0;                                                     \
	case WM_##macro_prefix##BUTTONUP:                                 \
		midWindowInput.button_prefix##Mouse = MID_PHASE_RELEASE;      \
		return 0;                                                     \
	case WM_##macro_prefix##BUTTONDBLCLK:                             \
		midWindowInput.button_prefix##Mouse = MID_PHASE_DOUBLE_CLICK; \
		return 0;

			MOUSE_PHASE(L, left)
			MOUSE_PHASE(R, right)
			MOUSE_PHASE(M, middle)

#undef MOUSE_PHASE

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);

	}
}

double timeQueryMs;

void midUpdateWindowInput()
{
	if (midWindowInput.leftMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.middleMouse = MID_PHASE_NONE;

	if (midWindowInput.leftMouse == MID_PHASE_PRESS) midWindowInput.leftMouse = MID_PHASE_HELD;
	if (midWindowInput.rightMouse == MID_PHASE_PRESS) midWindowInput.rightMouse = MID_PHASE_HELD;
	if (midWindowInput.middleMouse == MID_PHASE_PRESS) midWindowInput.middleMouse = MID_PHASE_HELD;

	if (midWindowInput.leftMouse == MID_PHASE_RELEASE) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_RELEASE) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_RELEASE) midWindowInput.middleMouse = MID_PHASE_NONE;

	midWindowInput.fMouseDeltaX = 0;
	midWindowInput.fMouseDeltaY = 0;

	static MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	u64 prior = midWindow.current;
	QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.current);
	u64 delta = ((midWindow.current - prior) * 1000000) / midWindow.frequency;
	midWindowInput.deltaTime = (double)delta * 0.000001f;

	static int titleUpdateRate = 64;
	if (!--titleUpdateRate) {
		titleUpdateRate = 64;
		constexpr int titleBufferSize = 64;
		static char   titleBuffer[titleBufferSize];
		snprintf(titleBuffer, titleBufferSize, "%s | FPS=%.2f | TimeQuery=%.8f", WINDOW_NAME, 1.0 / midWindowInput.deltaTime, timeQueryMs);
		SetWindowText(midWindow.hWnd, titleBuffer);
	}
}

void midCreateWindow()
{
	CHECK(midWindow.hInstance != NULL, "Window already created!");
	midWindow.hInstance = GetModuleHandle(NULL);
	atomic_store_explicit(&midWindow.running, true, memory_order_release);

	WNDCLASS wc = {.lpfnWndProc = WindowProc, .hInstance = midWindow.hInstance, .lpszClassName = CLASS_NAME};
	RegisterClass(&wc);

	DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	DWORD windowExStyle = WS_EX_APPWINDOW;
	midWindow.clientWidth = DEFAULT_WIDTH;
	midWindow.clientHeight = DEFAULT_HEIGHT;
	RECT  rect = {.right = midWindow.clientWidth, .bottom = midWindow.clientHeight};
	AdjustWindowRectEx(&rect, windowStyle, FALSE, windowExStyle);

	midWindow.windowWidth = rect.right - rect.left;
	midWindow.windowHeight = rect.bottom - rect.top;
	midWindow.hWnd = CreateWindowEx(windowExStyle, CLASS_NAME, WINDOW_NAME, windowStyle,
									DEFAULT_WINDOW_X_POSITION, DEFAULT_WINDOW_Y_POSITION,
									midWindow.windowWidth, midWindow.windowHeight,
									NULL, NULL, midWindow.hInstance, NULL);
	CHECK(midWindow.hWnd == NULL, "Failed to create window.");

	ShowWindow(midWindow.hWnd, SW_SHOW);
	UpdateWindow(midWindow.hWnd);

	QueryPerformanceFrequency((LARGE_INTEGER*)&midWindow.frequency);
	QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.start);
}

void midWindowLockCursor()
{
	ShowCursor(FALSE);
	SetCapture(midWindow.hWnd);

	RECT rect;
	GetClientRect(midWindow.hWnd, &rect);

	midWindow.globalCenter = midWindow.localCenter = (POINT){(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
	ClientToScreen(midWindow.hWnd, (POINT*)&midWindow.globalCenter);
	SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);

	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_ENABLED;
}

void midWindowReleaseCursor()
{
	ShowCursor(TRUE);
	ReleaseCapture();
	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_DISABLED;
}

#endif