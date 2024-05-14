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
              .queueCount = 2,
              .pQueuePriorities = (float[]){1.0f, 1.0f},
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
  vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &context.globalSetMemory, &context.globalSetBuffer, (void**)&context.pGlobalSetMapped);
  vkUpdateDescriptorSets(context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(context.globalSet, context.globalSetBuffer), 0, NULL);
  vkmUpdateGlobalSet(&context.globalCameraTransform, &context.globalSetState, context.pGlobalSetMapped);
  // global samplers
  VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);


  mxc_node_handle testNodeHandle = 0;
  MxcNodeContext* pTestNodeContext = &MXC_NODE_CONTEXTS[testNodeHandle];
  MxcTestNode    testNode;
  *pTestNodeContext = (MxcNodeContext) {
      .nodeType = MXC_NODE_TYPE_CONTEXT_THREAD,
      .compCycleSkip = 8,
      .pNode = &testNode,
      .runFunc = mxcRunTestNode,
      .compTimeline = context.timeline.semaphore,
  };
  vkmCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pTestNodeContext->framebuffers);
  {
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineSemaphoreTypeCreateInfo,
    };
    VKM_REQUIRE(vkCreateSemaphore(context.device, &timelineSemaphoreCreateInfo, VKM_ALLOC, &pTestNodeContext->nodeTimeline));
  }
  context.timeline.value = 0;
  MxcTestNodeCreateInfo testNodeCreateInfo = {
      .surface = surface,
      .transform = {0, 0, 0},
      .pFramebuffers = pTestNodeContext->framebuffers,
  };
  mxcCreateTestNode(&testNodeCreateInfo, &testNode);
  mxcCreateNodeContext(pTestNodeContext);
  mxcCopyHotNodeContext(testNodeHandle);
  MXC_NODE_HANDLE_COUNT = 1;

  MxcCompNode compNode = {};
  MxcCompNodeCreateInfo compNodeCreateInfo = {
      .surface = surface,
  };
  mxcCreateCompNode(&compNodeCreateInfo, &compNode);
  MxcNodeContext compNodeContext = {
      .nodeType = MXC_NODE_TYPE_CONTEXT_THREAD,
      .pNode = &compNode,
      .runFunc = mxcRunCompNode,
      .compTimeline = context.timeline.semaphore,
  };
  mxcCreateNodeContext(&compNodeContext);


  while (isRunning) {
    // wait on even for rendering
    vkmTimelineWait(context.device, &context.timeline);
    context.timeline.value++;

    vkmUpdateWindowInput();

    if (vkmProcessInput(&context.globalCameraTransform)) {
      vkmUpdateGlobalSetView(&context.globalCameraTransform, &context.globalSetState, context.pGlobalSetMapped);
    }

    // signal odd for input ready
    vkmTimelineSignal(context.device, &context.timeline);
    context.timeline.value++;
  }

//  int result = pthread_join(testNodeContext.threadId, NULL);
//  if (result != 0) {
//    perror("Thread join failed");
//    return 1;
//  }

  return EXIT_SUCCESS;
}
