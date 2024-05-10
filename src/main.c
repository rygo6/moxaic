#include "globals.h"
#include "node.h"
#include "renderer.h"
#include "test_node.h"

#include "comp_node.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

bool          isCompositor = true;
volatile bool isRunning = true;

[[noreturn]] void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  __builtin_trap();
}

typedef PFN_vkGetInstanceProcAddr GetInstanceProcAddrFunc;

static VkmContext mainContext;

int main(void) {

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

  vkmCreateWindow();
  vkmInitialize();

  VkSurfaceKHR surface;
  vkmCreateSurface(VKM_ALLOC, &surface);

  VkmContextCreateInfo contextCreateInfo = {
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
  vkmCreateContext(&contextCreateInfo, &mainContext);
  pContext = &mainContext;

  // standard/common rendering
  VkmCreateStandardRenderPass(&mainContext.standardRenderPass);
  vkmCreateStandardPipeline(mainContext.standardRenderPass, &mainContext.standardPipe);
  // global set
  vkmAllocateDescriptorSet(pContext->descriptorPool, &mainContext.standardPipe.globalSetLayout, &mainContext.globalSet);
  vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &mainContext.globalSetMemory, &mainContext.globalSetBuffer, (void**)&mainContext.pGlobalSetMapped);
  vkUpdateDescriptorSets(pContext->device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(pContext->globalSet, pContext->globalSetBuffer), 0, NULL);
  vkmUpdateGlobalSet(&mainContext.globalCameraTransform, &mainContext.globalSetState, pContext->pGlobalSetMapped);
  // global samplers
  VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &mainContext.linearSampler);


  MxcNodeContext testNode = {
      .nodeType = MXC_NODE_TYPE_LOCAL_THREAD,
      .pContext = &mainContext,
      .pInitArg = &(MxcTestNodeCreateInfo){
          .surface = surface,
          .transform = {1, 0, 0},
      },
      .createFunc = mxcCreateTestNode,
      .updateFunc = mxcUpdateTestNode,
  };
  mxcCreateNodeContext(&testNode);


  while (isRunning) {
    // wait for rendering
    vkmTimelineWait(mainContext.device, &mainContext.timeline);
    mainContext.timeline.value++;

    vkmUpdateWindowInput();

    if (vkmProcessInput(&mainContext.globalCameraTransform)) {
      vkmUpdateGlobalSetView(&mainContext.globalCameraTransform, &mainContext.globalSetState, mainContext.pGlobalSetMapped);
    }

    // signal input ready
    vkmTimelineSignal(mainContext.device, &mainContext.timeline);
    mainContext.timeline.value++;
  }

  int result = pthread_join(testNode.threadId, NULL);
  if (result != 0) {
    perror("Thread join failed");
    return 1;
  }

  return EXIT_SUCCESS;
}
