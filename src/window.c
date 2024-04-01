#include "window.h"
#include "globals.h"

#include <assert.h>
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define WINDOW_NAME "moxaic"
#define CLASS_NAME  "MoxaicWindowClass"

const char* WindowExtensionName = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
const char* ExternalMemoryExntensionName = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
const char* ExternalSemaphoreExntensionName = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
const char* ExternalFenceExntensionName = VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;

static struct {
  HINSTANCE hInstance;
  HWND      hwnd;
} window;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CLOSE:
    PostQuitMessage(0);
    break;
  default: return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

int mxCreateWindow() {
  window.hInstance = GetModuleHandle(NULL);

  DWORD windowStyle = WS_OVERLAPPEDWINDOW;

  WNDCLASS wc = {
      .lpfnWndProc = WindowProc,
      .hInstance = window.hInstance,
      .lpszClassName = CLASS_NAME,
  };

  RegisterClass(&wc);

  RECT rect = {
      .right = DEFAULT_WIDTH,
      .bottom = DEFAULT_HEIGHT,
  };
  AdjustWindowRect(&rect, windowStyle, FALSE);
  int windowWidth = rect.right - rect.left;
  int windowHeight = rect.bottom - rect.top;

  window.hwnd = CreateWindowEx(
      0,           // Optional window styles.
      CLASS_NAME,  // Window class
      WINDOW_NAME, // Window text
      windowStyle, // Window style

      // Size and position
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      windowWidth,
      windowHeight,

      NULL,             // Parent window
      NULL,             // Menu
      window.hInstance, // Instance handle
      NULL              // Additional application data
  );

  assert(window.hwnd != NULL && "Failed to create window.");

  ShowWindow(window.hwnd, SW_SHOW);
}

int mxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface) {
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(
      instance,
      "vkCreateWin32SurfaceKHR");
  assert(vkCreateWin32SurfaceKHR != NULL && "Couldn't load PFN_vkCreateWin32SurfaceKHR");
  VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = window.hInstance,
      .hwnd = window.hwnd,
  };
  return vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pVkSurface);
}