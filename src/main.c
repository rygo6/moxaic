#include "comp_node.h"
#include "globals.h"
#include "node.h"
#include "test_node.h"
#include "window.h"

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

volatile bool isCompositor = true;
volatile bool isRunning = true;
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
#if defined(MOXAIC_COMPOSITOR)
  printf("Moxaic Compositor\n");
  isCompositor = true;
  mxcInitializeIPCServer();
#elif defined(MOXAIC_NODE)
  printf("Moxaic node\n");
  isCompositor = false;
  mxcConnectNodeIPC();
#endif

  midCreateWindow();
  vkmInitialize();

  VkSurfaceKHR surface;
  midVkCreateVulkanSurface(midWindow.hInstance, midWindow.hWnd, MIDVK_ALLOC, &surface);

  {
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
  }


  // these probably should go elsewhere ?
  // global samplers
  VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);
  // standard/common rendering
  vkmCreateStdRenderPass();
  mxcCreateNodeRenderPass();
  vkmCreateStdPipeLayout();
  vkmCreateBasicPipe("./shaders/basic_material.vert.spv", "./shaders/basic_material.frag.spv", context.nodeRenderPass, context.stdPipeLayout.pipeLayout, &context.basicPipe);


  MxcCompNode compNode;
  {
    vkmBeginAllocationRequests();
    const MxcCompNodeCreateInfo compNodeInfo = {
        .compMode = MXC_COMP_MODE_TESS,
        .surface = surface,
    };
    mxcCreateCompNode(&compNodeInfo, &compNode);
    vkmEndAllocationRequests();
    mxcBindUpdateCompNode(&compNodeInfo, &compNode);
  }
  // move to register method like mxcRegisterCompNodeThread?
  compNodeShared = (MxcCompNodeShared){};
  compNodeShared.cmd = compNode.cmd;
  compNodeShared.compTimeline = compNode.timeline;
  compNodeShared.chain = compNode.swap.chain;
  compNodeShared.acquireSemaphore = compNode.swap.acquireSemaphore;
  compNodeShared.renderCompleteSemaphore = compNode.swap.renderCompleteSemaphore;
  const MxcNodeContext compNodeContext = {
      .nodeType = MXC_NODE_TYPE_THREAD,
      .pNode = &compNode,
      .runFunc = mxcCompNodeThread,
      .compTimeline = compNode.timeline,
  };
  mxcRunNode(&compNodeContext);


  NodeHandle  testNodeHandle;
  MxcTestNode testNode;
  {
    mxcRequestNodeThread(compNode.timeline, mxcTestNodeThread, &testNode, &testNodeHandle);
    const MxcTestNodeCreateInfo createInfo = {
        .transform = {0, 0, 0},
        .pFramebuffers = nodeContexts[testNodeHandle].framebufferTextures,
    };
    mxcCreateTestNode(&createInfo, &testNode);
  }
  mxcRegisterNodeThread(testNodeHandle);


  {
    VkDevice device = context.device;
    uint64_t compBaseCycleValue = 0;
    VkQueue  graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
    while (isRunning) {

      // we may not have to even wait... this could go faster
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compNodeShared.compTimeline);

      // somewhere input state needs to be copied to a need and only update when it knows the node needs it
      midUpdateWindowInput();
      isRunning = midWindow.running;
      mxcUpdateWindowInput();
      __atomic_thread_fence(__ATOMIC_RELEASE);

      // signal input ready to process!
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compNodeShared.compTimeline);

      // Try submitting nodes before waiting to render composite
      // We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
      mxcSubmitNodeThreadQueues(graphicsQueue);

      // wait for recording to be done
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compNodeShared.compTimeline);

      compBaseCycleValue += MXC_CYCLE_COUNT;
      vkmSubmitPresentCommandBuffer(compNodeShared.cmd,
                                    compNodeShared.chain,
                                    compNodeShared.acquireSemaphore,
                                    compNodeShared.renderCompleteSemaphore,
                                    compNodeShared.swapIndex,
                                    compNodeShared.compTimeline,
                                    compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);

      // Try submitting nodes before waiting to update window again.
      // We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
      mxcSubmitNodeThreadQueues(graphicsQueue);

      __atomic_thread_fence(__ATOMIC_ACQUIRE);
      if (ipcServerShared.requestNodeCreate) {
        NodeHandle  processNodeHandle;
        MxcTestNode processNode;
        mxcRequestNodeProcess(compNode.timeline, &processNode, &processNodeHandle);

        ipcServerShared.requestNodeCreate = false;
        ipcServerShared.createdNodeHandle = processNodeHandle;
        __atomic_thread_fence(__ATOMIC_RELEASE);

        sem_post(&ipcServerShared.createNodeWaitSemaphore);
      }
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
