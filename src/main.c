#include "globals.h"
#include "renderer.h"
#include "test_node.h"
#include "window.h"

#define STB_IMAGE_IMPLEMENTATION
typedef const VkmContextCreateInfo info;
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;

//struct static_arena_memory {
//  VkmInstance instance;
//  VkmContext  context;
//};
//void* static_arena = &(struct static_arena_memory){};

void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  abort();
}

bool cycle;

DWORD TestNodeUpdate(LPVOID pVoid) {
  while (isRunning) {
    mxcTestNodeUpdate();
    cycle = true;
  }
  return 0;
}

int main(void) {
  //  assert(sizeof(struct static_arena_memory) <= 1 << 16);
  //  arena_offset aInstance = offsetof(struct static_arena_memory, instance);

  vkmCreateWindow();
  vkmInitialize();

  VkSurfaceKHR surface;
  vkmCreateSurface(VKM_ALLOC, &surface);

  info contextCreateInfo = {
      .maxDescriptorCount = 30,
      .uniformDescriptorCount = 10,
      .combinedImageSamplerDescriptorCount = 10,
      .storageImageDescriptorCount = 10,
      .presentSurface = surface,
      .queueFamilyCreateInfos = {
          [VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS] = {
              .supportsGraphics = VKM_SUPPORT_YES,
              .supportsCompute = VKM_SUPPORT_YES,
              .supportsTransfer = VKM_SUPPORT_YES,
              .globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
              .queueCount = 1,
              .pQueuePriorities = (float[]){1.0f},
          },
          //          [VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = {
          //              .supportsGraphics = VKM_SUPPORT_NO,
          //              .supportsCompute = VKM_SUPPORT_YES,
          //              .supportsTransfer = VKM_SUPPORT_YES,
          //              .globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
          //              .queueCount = 1,
          //              .pQueuePriorities = (float[]){1.0f},
          //          },
          //          [VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER] = {
          //              .supportsGraphics = VKM_SUPPORT_NO,
          //              .supportsCompute = VKM_SUPPORT_NO,
          //              .supportsTransfer = VKM_SUPPORT_YES,
          //              .queueCount = 1,
          //              .pQueuePriorities = (float[]){0.0f},
          //          },
      },
  };
//  VkmContext context;
  vkmCreateContext(&contextCreateInfo, &context);

  mxcCreateTestNodeContext(surface);

  HANDLE hThread;
  DWORD  threadId;
  hThread = CreateThread(NULL, 0, TestNodeUpdate, NULL, 0, &threadId);
  if (hThread == NULL) {
    MessageBox(NULL, TEXT("Failed to create thread!"), TEXT("Error"), MB_OK);
    return 1;
  }

  while (isRunning) {
    // wait for rendering
    vkmTimelineWait(context.device, &context.timeline);
    context.timeline.value++;

    vkmUpdateWindowInput();

    // signal input ready
    vkmTimelineSignal(context.device, &context.timeline);
    context.timeline.value++;
  }

  WaitForSingleObject(hThread, INFINITE);
  CloseHandle(hThread);

  //  while (isRunning) {
  //    vkmUpdateWindowInput();
  //    mxcTestNodeUpdate();
  //  }

  return 0;
}
