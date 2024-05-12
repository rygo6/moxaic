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

static MxcTestNode testNode;

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
  vkmCreateContext(&contextCreateInfo);

  // standard/common rendering
  VkmCreateStandardRenderPass(&context.standardRenderPass);
  vkmCreateStandardPipeline(context.standardRenderPass, &context.standardPipe);
  // global set
  vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.globalSetLayout, &context.globalSet);
  vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &context.globalSetMemory, &context.globalSetBuffer, (void**)&context.pGlobalSetMapped);
  vkUpdateDescriptorSets(context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(context.globalSet, context.globalSetBuffer), 0, NULL);
  vkmUpdateGlobalSet(&context.globalCameraTransform, &context.globalSetState, context.pGlobalSetMapped);
  // global samplers
  VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);


  MxcTestNodeCreateInfo testNodeCreateInfo = {
      .surface = surface,
//      .transform = {1, 0, 0},
  };
  mxcCreateTestNode(&testNodeCreateInfo, &testNode);
  MxcNodeContext testNodeContext = {
      .nodeType = MXC_NODE_TYPE_LOCAL_THREAD,
      .pNode = &testNode,
      .runFunc = mxcRunTestNode,
      .timeline = context.timeline.semaphore
  };
  mxcCreateNodeContext(&testNodeContext);


  while (isRunning) {
    // wait for rendering
    vkmTimelineWait(context.device, &context.timeline);
    context.timeline.value++;

    vkmUpdateWindowInput();

    if (vkmProcessInput(&context.globalCameraTransform)) {
      vkmUpdateGlobalSetView(&context.globalCameraTransform, &context.globalSetState, context.pGlobalSetMapped);
    }

    // signal input ready
    vkmTimelineSignal(context.device, &context.timeline);
    context.timeline.value++;
  }

  int result = pthread_join(testNodeContext.threadId, NULL);
  if (result != 0) {
    perror("Thread join failed");
    return 1;
  }

  return EXIT_SUCCESS;
}
