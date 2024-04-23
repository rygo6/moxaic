#include "window.h"
#include "globals.h"

#include <assert.h>
#include <windows.h>
#include <windowsx.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define WINDOW_NAME "moxaic"
#define CLASS_NAME  "MoxaicWindowClass"

const char* WindowExtensionName = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
const char* ExternalMemoryExtensionName = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
const char* ExternalSemaphoreExtensionName = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
const char* ExternalFenceExtensionName = VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;

Input input;

static struct {
  HINSTANCE hInstance;
  HWND      hWnd;
  int       width;
  int       height;
  POINT     localCenter;
  POINT     globalCenter;
} window;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_MOUSEMOVE:
      if (input.mouseLocked) {
        const int   xPos = GET_X_LPARAM(lParam);
        const int   yPos = GET_Y_LPARAM(lParam);
        const int   deltaXPos = xPos - window.localCenter.x;
        const int   deltaYPos = yPos - window.localCenter.y;
        const float sensitivity = .001f;
        input.mouseDeltaX = (float)deltaXPos * sensitivity;
        input.mouseDeltaY = (float)deltaYPos * sensitivity;
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

void vkmUpdateWindowInput() {
  input.mouseDeltaX = 0;
  input.mouseDeltaY = 0;
  static MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
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
}

VkResult vkmCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface) {
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(
      instance,
      "vkCreateWin32SurfaceKHR");
  assert(vkCreateWin32SurfaceKHR != NULL && "Couldn't load PFN_vkCreateWin32SurfaceKHR");
  VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = window.hInstance,
      .hwnd = window.hWnd,
  };
  return vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pVkSurface);
}