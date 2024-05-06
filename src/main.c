#include "globals.h"
#include "node.h"
#include "renderer.h"
#include "test_node.h"
//#include "window.h"

#define STB_IMAGE_IMPLEMENTATION
typedef const VkmContextCreateInfo info;
#include "stb_image.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <Windows.h>

bool isCompositor = true;
volatile bool isRunning = true;

//struct static_arena_memory {
//  VkmInstance instance;
//  VkmContext  context;
//};
//void* static_arena = &(struct static_arena_memory){};

void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  __builtin_trap();
}

void* TestNodeUpdate(void* arg) {
  while (isRunning) {
    mxcUpdateTestNode();
  }
  return 0;
}

//typedef PFN_vkGetInstanceProcAddr GetInstanceProcAddrFunc;

int main(void) {

#ifdef __STDC_NO_THREADS__
  printf("Threads are not supported.\n");
#else
  printf("Threads are supported.\n");
#endif

  //  HMODULE vulkanLibrary = LoadLibrary("vulkan-1.dll");
  //  if (vulkanLibrary == NULL) {
  //    fprintf(stderr, "Failed to load Vulkan library.\n");
  //    return EXIT_FAILURE;
  //  }
  //
  //  GetInstanceProcAddrFunc vkGetInstanceProcAddr = (GetInstanceProcAddrFunc)GetProcAddress(vulkanLibrary, "vkGetInstanceProcAddr");
  //  if (vkGetInstanceProcAddr == NULL) {
  //    fprintf(stderr, "Failed to retrieve function pointer to vkGetInstanceProcAddr.\n");
  //    return EXIT_FAILURE;
  //  }

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
  vkmCreateContext(&contextCreateInfo, &context);

  //  mxcCreateTestNode(surface);

  MxcNodeCreateInfo nodeCreateInfo = {
      .nodeType = MXC_NODE_TYPE_LOCAL_THREAD,
      .pInitArg = &(MxcTestNodeCreateInfo){
          .surface = surface,
          .transform = {1, 0, 0},
      },
      .initFunc = mxcCreateTestNode,
      .updateFunc = mxcUpdateTestNode,
  };
  MxcNodeContext testNode;
  mxcCreateNodeContext(&nodeCreateInfo, &testNode);

//  pthread_t thread_id;
//  int       result;
//  result = pthread_create(&thread_id, NULL, TestNodeUpdate, NULL);
//  if (result != 0) {
//    perror("Thread creation failed");
//    return 1;
//  }

  while (isRunning) {
    // wait for rendering
    vkmTimelineWait(context.device, &context.timeline);
    context.timeline.value++;

    vkmUpdateWindowInput();

    // signal input ready
    vkmTimelineSignal(context.device, &context.timeline);
    context.timeline.value++;
  }

  int result = pthread_join(testNode.threadId, NULL);
  if (result != 0) {
    perror("Thread join failed");
    return 1;
  }

  //  while (isRunning) {
  //    vkmUpdateWindowInput();
  //    mxcTestNodeUpdate();
  //  }

  return 0;
}
