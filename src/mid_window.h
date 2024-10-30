#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
#define NONLS             // All NLS defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions
#define NOGDI
#include <windows.h>

//
//// Debug
#ifndef MID_DEBUG
#define MID_DEBUG
extern void Panic(const char* file, int line, const char* message);
#define PANIC(_message) Panic(__FILE__, __LINE__, _message)
#define CHECK(_err, _message)                      \
	if (__builtin_expect(!!(_err), 0)) {           \
		fprintf(stderr, "Error Code: %d\n", _err); \
		PANIC(_message);                           \
	}
#ifdef MID_WINDOW_IMPLEMENTATION
[[noreturn]] void Panic(const char* file, int line, const char* message)
{
	fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
	__builtin_trap();
}
#endif
#endif

//
//// MidWindow Header
#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 1024

typedef struct MidWindow {
  HINSTANCE hInstance;
  HWND      hWnd;
  int       width, height;
  POINT     localCenter, globalCenter;
  uint64_t  frequency, start, current;
  bool      running;
} MidWindow;

typedef enum MidPhaseType {
  MID_PHASE_NONE,
  MID_PHASE_PRESS,
  MID_PHASE_HELD,
  MID_PHASE_RELEASE,
  MID_PHASE_CANCEL,
  MID_PHASE_DOUBLE_CLICK,
  MID_PHASE_COUNT,
} MidPhaseType;
typedef uint8_t MidPhase;

typedef struct MidWindowInput {

  int iMouseX;
  int iMouseY;
  int iMouseDeltaX;
  int iMouseDeltaY;

  float mouseX;
  float mouseY;
  float mouseDeltaX;
  float mouseDeltaY;

  MidPhase leftMouse;
  MidPhase rightMouse;
  MidPhase middleMouse;

  MidPhase keyChar['Z' - '0'];

  double deltaTime;

  bool cursorLocked;

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

static inline uint64_t midQueryPerformanceCounter(){
  LARGE_INTEGER value = {};
  QueryPerformanceCounter(&value);
  value.QuadPart *= 1000000;
  value.QuadPart /= midWindow.frequency;
  return value.QuadPart;
}

extern double timeQueryMs;

//
//// MidWindow Implementation
#ifdef MID_WINDOW_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windowsx.h>

#include <stdio.h>

#ifdef MID_WINDOW_VULKAN
#include <vulkan/vulkan_win32.h>
#endif

#define WINDOW_NAME "moxaic"
#define CLASS_NAME  "MoxaicWindowClass"

MidWindowInput midWindowInput;
MidWindow      midWindow;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_MOUSEMOVE:
      int newX = GET_X_LPARAM(lParam);
      int newY = GET_Y_LPARAM(lParam);

      midWindowInput.iMouseX = newX;
      midWindowInput.iMouseY = newY;

      if (midWindowInput.cursorLocked) {
        midWindowInput.iMouseDeltaX = newX - midWindow.localCenter.x;
        midWindowInput.iMouseDeltaY = newY - midWindow.localCenter.y;
        SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);
      } else {
        midWindowInput.iMouseDeltaX = newX - midWindowInput.iMouseX;
        midWindowInput.iMouseDeltaY = newY - midWindowInput.iMouseY;
      }

      midWindowInput.mouseDeltaX = (float)midWindowInput.iMouseDeltaX;
      midWindowInput.mouseDeltaY = (float)midWindowInput.iMouseDeltaY;
      midWindowInput.mouseX = (float)midWindowInput.iMouseX;
      midWindowInput.mouseY = (float)midWindowInput.iMouseY;
      return 0;

#define MOUSE_PHASE(macro_prefix, button_prefix)                  \
  case WM_##macro_prefix##BUTTONDOWN:                             \
    midWindowInput.button_prefix##Mouse = MID_PHASE_PRESS;        \
    return 0;                                                     \
  case WM_##macro_prefix##BUTTONUP:                               \
    midWindowInput.button_prefix##Mouse = MID_PHASE_RELEASE;      \
    return 0;                                                     \
  case WM_##macro_prefix##BUTTONDBLCLK:                           \
    midWindowInput.button_prefix##Mouse = MID_PHASE_DOUBLE_CLICK; \
    return 0;
      MOUSE_PHASE(L, left)
      MOUSE_PHASE(R, right)
      MOUSE_PHASE(M, middle)
#undef MOUSE_PHASE

    case WM_KEYDOWN:
      if (wParam >= '0' && wParam <= 'Z')
        midWindowInput.keyChar[wParam - '0'] = MID_PHASE_PRESS;
      return 0;
    case WM_KEYUP:
      if (wParam >= '0' && wParam <= 'Z')
        midWindowInput.keyChar[wParam - '0'] = MID_PHASE_RELEASE;
      return 0;

    case WM_CLOSE:
      midWindow.running = false;
      return 0;
    default:
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
}

double timeQueryMs;

void midUpdateWindowInput() {

  if (midWindowInput.leftMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.leftMouse = MID_PHASE_NONE;
  if (midWindowInput.rightMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.rightMouse = MID_PHASE_NONE;
  if (midWindowInput.middleMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.middleMouse = MID_PHASE_NONE;

  if (midWindowInput.leftMouse == MID_PHASE_PRESS) midWindowInput.leftMouse = MID_PHASE_HELD;
  if (midWindowInput.rightMouse == MID_PHASE_PRESS) midWindowInput.rightMouse = MID_PHASE_HELD;
  if (midWindowInput.middleMouse == MID_PHASE_PRESS) midWindowInput.middleMouse = MID_PHASE_HELD;

  if (midWindowInput.leftMouse == MID_PHASE_RELEASE) midWindowInput.leftMouse = MID_PHASE_NONE;
  if (midWindowInput.rightMouse == MID_PHASE_RELEASE) midWindowInput.rightMouse = MID_PHASE_NONE;
  if (midWindowInput.middleMouse == MID_PHASE_RELEASE) midWindowInput.middleMouse = MID_PHASE_NONE;

  midWindowInput.mouseDeltaX = 0;
  midWindowInput.mouseDeltaY = 0;

  static MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  uint64_t prior = midWindow.current;
  QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.current);
  uint64_t delta = ((midWindow.current - prior) * 1000000) / midWindow.frequency;
  midWindowInput.deltaTime = (double)delta * 0.000001f;

  static int titleUpdateRate = 64;
  if (!--titleUpdateRate) {
    titleUpdateRate = 64;
#define TITLE_BUFFER_SIZE 64
    static char titleBuffer[TITLE_BUFFER_SIZE];
    snprintf(titleBuffer, TITLE_BUFFER_SIZE, "%s | FPS=%.2f | TimeQuery=%.8f", WINDOW_NAME, 1.0f / midWindowInput.deltaTime, timeQueryMs);
    SetWindowText(midWindow.hWnd, titleBuffer);
  }
}

void midCreateWindow() {
  CHECK(midWindow.hInstance != NULL, "Window already created!");
  midWindow.hInstance = GetModuleHandle(NULL);
  midWindow.running = true;
  const DWORD    windowStyle = WS_OVERLAPPEDWINDOW;
  const WNDCLASS wc = {.lpfnWndProc = WindowProc, .hInstance = midWindow.hInstance, .lpszClassName = CLASS_NAME};
  RegisterClass(&wc);
  RECT rect = {.right = DEFAULT_WIDTH, .bottom = DEFAULT_HEIGHT};
  AdjustWindowRect(&rect, windowStyle, FALSE);
  midWindow.width = rect.right - rect.left;
  midWindow.height = rect.bottom - rect.top;
  midWindow.hWnd = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, windowStyle, CW_USEDEFAULT, CW_USEDEFAULT, midWindow.width, midWindow.height, NULL, NULL, midWindow.hInstance, NULL);
  CHECK(midWindow.hWnd == NULL, "Failed to create window.");
  ShowWindow(midWindow.hWnd, SW_SHOW);
  UpdateWindow(midWindow.hWnd);
  QueryPerformanceFrequency((LARGE_INTEGER*)&midWindow.frequency);
  QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.start);
}

void midWindowLockCursor() {
  ShowCursor(FALSE);
  SetCapture(midWindow.hWnd);
  RECT rect;
  GetClientRect(midWindow.hWnd, &rect);
  midWindow.globalCenter = midWindow.localCenter = (POINT){(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
  ClientToScreen(midWindow.hWnd, (POINT*)&midWindow.globalCenter);
  SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);
  midWindowInput.cursorLocked = true;
}

void midWindowReleaseCursor() {
  ShowCursor(TRUE);
  ReleaseCapture();
  midWindowInput.cursorLocked = false;
}

#endif