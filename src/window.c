#include "window.h"
#include "globals.h"
#include "renderer.h"

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define WINDOW_NAME "moxaic"
#define CLASS_NAME  "MoxaicWindowClass"

const char*                              VKM_PLATFORM_SURFACE_EXTENSION_NAME = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_MEMORY_EXTENSION_NAME = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_SEMAPHORE_EXTENSION_NAME = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
const char*                              VKM_EXTERNAL_FENCE_EXTENSION_NAME = VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;
const VkExternalMemoryHandleTypeFlagBits VKM_EXTERNAL_HANDLE_TYPE = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

Input input;

static struct {
  HINSTANCE hInstance;
  HWND      hWnd;
  int       width, height;
  POINT     localCenter, globalCenter;
  uint64_t  frequency, start, current;
} window;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_MOUSEMOVE:
      if (input.mouseLocked) {
        input.mouseDeltaX = (float)(GET_X_LPARAM(lParam) - window.localCenter.x);
        input.mouseDeltaY = (float)(GET_Y_LPARAM(lParam) - window.localCenter.y);
        SetCursorPos(window.globalCenter.x, window.globalCenter.y);
      }
      return 0;
    case WM_LBUTTONDOWN:
      ShowCursor(FALSE);
      SetCapture(window.hWnd);
      RECT rect;
      GetClientRect(window.hWnd, &rect);
      window.globalCenter = window.localCenter = (POINT){(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
      ClientToScreen(window.hWnd, (POINT*)&window.globalCenter);
      SetCursorPos(window.globalCenter.x, window.globalCenter.y);
      input.mouseLocked = true;
      return 0;
    case WM_LBUTTONUP:
      ShowCursor(TRUE);
      ReleaseCapture();
      input.mouseLocked = false;
      return 0;
    case WM_KEYDOWN:
      switch (wParam) {
        case 'W': input.moveForward = true; return 0;
        case 'S': input.moveBack = true; return 0;
        case 'D': input.moveRight = true; return 0;
        case 'A': input.moveLeft = true; return 0;
        case 'Z': input.debugSwap = !input.debugSwap; return 0;
        default:  return 0;
      }
    case WM_KEYUP:
      switch (wParam) {
        case 'W': input.moveForward = false; return 0;
        case 'S': input.moveBack = false; return 0;
        case 'D': input.moveRight = false; return 0;
        case 'A': input.moveLeft = false; return 0;
        default:  return 0;
      }
    case WM_CLOSE:
      isRunning = false;
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
}

double timeQueryMs;

void vkmUpdateWindowInput() {
  input.mouseDeltaX = 0;
  input.mouseDeltaY = 0;
  static MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  uint64_t prior = window.current;
  QueryPerformanceCounter((LARGE_INTEGER*)&window.current);
  uint64_t delta = ((window.current - prior) * 1000000) / window.frequency;
  input.deltaTime = (double)delta * 0.000001f;

  static int titleUpdateRate = 64;
  if (!--titleUpdateRate) {
    titleUpdateRate = 64;
    static char titleBuffer[32];
    sprintf(titleBuffer, "%s | FPS=%.2f | TimeQuery=%.8f", WINDOW_NAME, 1.0f / input.deltaTime, timeQueryMs);
    SetWindowText(window.hWnd, titleBuffer);
  }
}

void vkmCreateWindow() {
  window.hInstance = GetModuleHandle(NULL);
  const DWORD    windowStyle = WS_OVERLAPPEDWINDOW;
  const WNDCLASS wc = {.lpfnWndProc = WindowProc, .hInstance = window.hInstance, .lpszClassName = CLASS_NAME};
  RegisterClass(&wc);
  RECT rect = {.right = DEFAULT_WIDTH, .bottom = DEFAULT_HEIGHT};
  AdjustWindowRect(&rect, windowStyle, FALSE);
  window.width = rect.right - rect.left;
  window.height = rect.bottom - rect.top;
  window.hWnd = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, windowStyle, CW_USEDEFAULT, CW_USEDEFAULT, window.width, window.height, NULL, NULL, window.hInstance, NULL);
  REQUIRE(window.hWnd != NULL, "Failed to create window.");
  ShowWindow(window.hWnd, SW_SHOW);
  UpdateWindow(window.hWnd);
  QueryPerformanceFrequency((LARGE_INTEGER*)&window.frequency);
  QueryPerformanceCounter((LARGE_INTEGER*)&window.start);
}

void vkmCreateSurface(const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
  const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = window.hInstance,
      .hwnd = window.hWnd,
  };
  VKM_INSTANCE_FUNC(vkCreateWin32SurfaceKHR);
  VKM_REQUIRE(vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pSurface));
}