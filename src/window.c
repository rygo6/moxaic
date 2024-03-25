#include "window.h"
#include "constants.h"

#include <assert.h>
#include <windows.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

const char* const g_WindowExtensionName = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
const char* const g_ExternalMemoryExntensionName = VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
const char* const g_ExternalSemaphoreExntensionName = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
const char* const g_ExternalFenceExntensionName = VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;

struct {
  HINSTANCE hInstance;
  HWND      hwnd;
} window;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CLOSE:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

int MxCreateWindow() {
  window.hInstance = GetModuleHandle(NULL);

  const wchar_t CLASS_NAME[] = L"MoxaicWindowClass";

  WNDCLASS wc = {
      .lpfnWndProc = WindowProc,
      .hInstance = window.hInstance,
      .lpszClassName = CLASS_NAME,
  };

  RegisterClass(&wc);

  window.hwnd = CreateWindowEx(
      0,                   // Optional window styles.
      CLASS_NAME,          // Window class
      L"moxaic",           // Window text
      WS_OVERLAPPEDWINDOW, // Window style

      // Size and position
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

      NULL,             // Parent window
      NULL,             // Menu
      window.hInstance, // Instance handle
      NULL              // Additional application data
  );

  assert(window.hwnd != NULL && "Failed to create window.");

  ShowWindow(window.hwnd, SW_SHOW);
}

VkResult MxCreateSurface(VkInstance instance, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pVkSurface) {

  PFN_vkCreateWin32SurfaceKHR createWin32Surface = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
  assert(createWin32Surface != NULL && "Couldn't load PFN_vkCreateWin32SurfaceKHR");
  const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = window.hInstance,
      .hwnd = window.hwnd,
  };
  return createWin32Surface(instance, &win32SurfaceCreateInfo, pAllocator, pVkSurface);
}