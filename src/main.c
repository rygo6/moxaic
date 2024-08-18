#include "comp_node.h"
#include "globals.h"
#include "node.h"
#include "test_node.h"
#include "window.h"

//#define TEST_NODE

#define MID_DEBUG
[[noreturn]] void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  __builtin_trap();
}

#define MID_VULKAN_IMPLEMENTATION
#include "mid_vulkan.h"

#define MID_SHAPE_IMPLEMENTATION
#include "mid_shape.h"

#define MID_WINDOW_IMPLEMENTATION
#define MID_WINDOW_VULKAN
#include "mid_window.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;
//typedef PFN_vkGetInstanceProcAddr GetInstanceProcAddrFunc;

int main(void) {

  // setup dynamic vk load at some point
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

  { // Initialize
    midCreateWindow();
    vkmInitialize();

    VkSurfaceKHR surface;
    midVkCreateVulkanSurface(midWindow.hInstance, midWindow.hWnd, MIDVK_ALLOC, &surface);

    const VkmContextCreateInfo contextCreateInfo = {
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
            [VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = {
                .supportsGraphics = VKM_SUPPORT_NO,
                .supportsCompute = VKM_SUPPORT_YES,
                .supportsTransfer = VKM_SUPPORT_YES,
                .globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
                .queueCount = 1,
                .pQueuePriorities = (float[]){1.0f},
            },
            [VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER] = {
                .supportsGraphics = VKM_SUPPORT_NO,
                .supportsCompute = VKM_SUPPORT_NO,
                .supportsTransfer = VKM_SUPPORT_YES,
                .queueCount = 1,
                .pQueuePriorities = (float[]){0.0f},
            },
        },
    };
    vkmCreateContext(&contextCreateInfo);

    // these probably should go elsewhere ?
    // global samplers
    VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);
    // standard/common rendering
    vkmCreateStdRenderPass();
    vkmCreateStdPipeLayout();

    // this need to ge in node create
    mxcCreateNodeRenderPass();
    vkmCreateBasicPipe("./shaders/basic_material.vert.spv", "./shaders/basic_material.frag.spv", context.nodeRenderPass, context.stdPipeLayout.pipeLayout, &context.basicPipe);

    // Comp
    mxcRequestAndRunCompNodeThread(surface, mxcCompNodeThread);

#if defined(MOXAIC_COMPOSITOR)
    printf("Moxaic Compositor\n");
    isCompositor = true;
    mxcInitializeIPCServer();
#elif defined(MOXAIC_NODE)
    printf("Moxaic node\n");
    isCompositor = false;
    mxcConnectNodeIPC();
#endif

    //     Test Nodes
#ifdef TEST_NODE
    NodeHandle testNodeHandle;
    mxcRequestNodeThread(&testNodeHandle);
    mxcRunNodeThread(mxcTestNodeThread, testNodeHandle);
#endif
  }


  if (isCompositor) { // Compositor Loop
    const VkDevice device = context.device;
    const VkQueue  graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
    uint64_t       compBaseCycleValue = 0;
    while (isRunning) {

      // we may not have to even wait... this could go faster
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compNodeContext.compTimeline);

      // somewhere input state needs to be copied to a node and only update when it knows the node needs it
      midUpdateWindowInput();
      isRunning = midWindow.running;
      mxcProcessWindowInput();
      __atomic_thread_fence(__ATOMIC_RELEASE);

      // signal input ready to process!
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compNodeContext.compTimeline);

      // Try submitting nodes before waiting to render composite
      // We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
      mxcSubmitNodeThreadQueues(graphicsQueue);

      // wait for recording to be done
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compNodeContext.compTimeline);

      compBaseCycleValue += MXC_CYCLE_COUNT;
      __atomic_thread_fence(__ATOMIC_ACQUIRE);
      const int swapIndex = compNodeContext.swapIndex;
      vkmSubmitPresentCommandBuffer(compNodeContext.cmd,
                                    compNodeContext.swap.chain,
                                    compNodeContext.swap.acquireSemaphore,
                                    compNodeContext.swap.renderCompleteSemaphore,
                                    swapIndex,
                                    compNodeContext.compTimeline,
                                    compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);

      // Try submitting nodes before waiting to update window again.
      // We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
      mxcSubmitNodeThreadQueues(graphicsQueue);
    }
  } else {

    const VkQueue  graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
    while (isRunning) {

      // I guess technically we just want to go as fast as possible in a node, but we would probably need to process and send input here first at some point?
      mxcSubmitNodeThreadQueues(graphicsQueue);
    }
  }



  //  int result = pthread_join(testNodeContext.threadId, NULL);
  //  if (result != 0) {
  //    perror("Thread join failed");
  //    return 1;
  //  }

#if defined(MOXAIC_COMPOSITOR)
  mxcShutdownIPCServer();
#elif defined(MOXAIC_NODE)
  mxcShutdownNodeIPC();
#endif

  return EXIT_SUCCESS;
}
