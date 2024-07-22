#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>

#ifdef MID_WINDOW_VULKAN
#include <vulkan/vulkan.h>
#endif

//
//// Debug
#ifndef MID_DEBUG
#define MID_DEBUG
extern void Panic(const char* file, int line, const char* message);
#define PANIC(message) Panic(__FILE__, __LINE__, message)
#define REQUIRE(condition, message)        \
  if (__builtin_expect(!(condition), 0)) { \
    PANIC(message);                        \
  }
#ifdef MID_WINDOW_IMPLEMENTATION
[[noreturn]] void Panic(const char* file, const int line, const char* message) {
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
  volatile bool running;
  HINSTANCE     hInstance;
  HWND          hWnd;
  int           width, height;
  POINT         localCenter, globalCenter;
  uint64_t      frequency, start, current;
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
#define MID_KEY_S keyChar['S' - '0']
#define MID_KEY_W keyChar['W' - '0']

extern MidWindow      midWindow;
extern MidWindowInput midWindowInput;

void midUpdateWindowInput();
void midCreateWindow();

void midWindowLockCursor();
void midWindowReleaseCursor();

#ifdef MID_WINDOW_VULKAN
void midCreateVulkanSurface(const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
#endif

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

#ifdef MID_WINDOW_VULKAN
const char*                              VKM_PLATFORM_SURFACE_EXTENSION_NAME = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_MEMORY_EXTENSION_NAME = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_SEMAPHORE_EXTENSION_NAME = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_FENCE_EXTENSION_NAME = VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;
const VkExternalMemoryHandleTypeFlagBits VKM_EXTERNAL_HANDLE_TYPE = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#endif

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
    static char titleBuffer[32];
    sprintf(titleBuffer, "%s | FPS=%.2f | TimeQuery=%.8f", WINDOW_NAME, 1.0f / midWindowInput.deltaTime, timeQueryMs);
    SetWindowText(midWindow.hWnd, titleBuffer);
  }
}

void midCreateWindow() {
  REQUIRE(midWindow.hInstance == NULL, "Window already created!");
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
  REQUIRE(midWindow.hWnd != NULL, "Failed to create window.");
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

#ifdef MID_WINDOW_VULKAN
void midCreateVulkanSurface(const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
  const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = midWindow.hInstance,
      .hwnd = midWindow.hWnd,
  };
  VKM_INSTANCE_FUNC(vkCreateWin32SurfaceKHR);
  VKM_REQUIRE(vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pSurface));
}
#endif

#endif