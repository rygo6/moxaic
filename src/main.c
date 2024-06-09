#include "comp_node.h"
#include "globals.h"
#include "node.h"
#include "renderer.h"
#include "test_node.h"

#define MID_SHAPE_IMPLEMENTATION
#include "mid_shape.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool          isCompositor = true;
volatile bool isRunning = true;

[[noreturn]] void Panic(const char* file, const int line, const char* message) {
  fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
  __builtin_trap();
}

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

  vkmCreateWindow();
  vkmInitialize();

  VkSurfaceKHR surface;
  vkmCreateSurface(VKM_ALLOC, &surface);


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


  {
    // these probably should go elsewhere ?
    // global samplers
    VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);
    // standard/common rendering
    VkmCreateStdRenderPass(&context.stdRenderPass);
    vkmCreateStdPipe(&context.stdPipe);
  }


  {  // create comp
    MxcCompNode           compNode;
    MxcCompNodeCreateInfo compNodeInfo = {
        .compMode = MXC_COMP_MODE_TESS,
        //      .compMode = MXC_COMP_MODE_BASIC,
        .surface = surface,
    };
    mxcCreateCompNode(&compNodeInfo, &compNode);
    // more to register method like mxcRegisterCompNodeThread
    compNodeShared = (MxcCompNodeContextShared){};
    compNodeShared.cmd = compNode.cmd;
    compNodeShared.compTimeline = compNode.timeline;
    compNodeShared.swapIndex = compNode.swap.swapIndex;
    compNodeShared.chain = compNode.swap.chain;
    compNodeShared.acquireSemaphore = compNode.swap.acquireSemaphore;
    compNodeShared.renderCompleteSemaphore = compNode.swap.renderCompleteSemaphore;
    MxcNodeContext compNodeContext = {
        .nodeType = MXC_NODE_TYPE_THREAD,
        .pNode = &compNode,
        .runFunc = mxcRunCompNode,
        .compTimeline = compNode.timeline,
    };
    mxcCreateNodeContext(&compNodeContext);
  }


  {  // create temp test node
    NodeHandle      testNodeHandle = 0;
    MxcTestNode     testNode;
    MxcNodeContext* pTestNodeContext = &nodes[testNodeHandle];
    *pTestNodeContext = (MxcNodeContext){
        .nodeType = MXC_NODE_TYPE_THREAD,
        .compCycleSkip = 16,
        .pNode = &testNode,
        .runFunc = mxcRunTestNode,
        // should i just read directly from compNodeShared.compTimeline in node setup?
        .compTimeline = compNodeShared.compTimeline,
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
    MxcTestNodeCreateInfo testNodeCreateInfo = {
        .transform = {0, 0, 0},
        .pFramebuffers = pTestNodeContext->framebuffers,
    };
    mxcCreateTestNode(&testNodeCreateInfo, &testNode);
    mxcCreateNodeContext(pTestNodeContext);
    nodesShared[0] = (MxcNodeContextShared){};
    nodesShared[0].cmd = testNode.cmd;  // we should request node slot first
    mxcRegisterCompNodeThread(testNodeHandle);
    nodeCount = 1;
    __atomic_thread_fence(__ATOMIC_RELEASE);
  }


  {
    uint64_t compBaseCycleValue = 0;
    VkDevice device = context.device;
    VkQueue  graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
    while (isRunning) {

      // we may not have to even wait... this could go faster
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compNodeShared.compTimeline);

      vkmUpdateWindowInput();
      __atomic_thread_fence(__ATOMIC_RELEASE);

      // signal input ready to process!
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compNodeShared.compTimeline);

      // this could go on its own thread going even faster
      for (int i = 0; i < nodeCount; ++i) {
        {  // submit commands
          uint64_t value = nodesShared[i].pendingTimelineSignal;
          __atomic_thread_fence(__ATOMIC_ACQUIRE);
          if (value > nodesShared[i].lastTimelineSignal) {
            nodesShared[i].lastTimelineSignal = value;
            vkmSubmitCommandBuffer(nodesShared[i].cmd, graphicsQueue, nodesShared[i].nodeTimeline, value);
          }
        }
      }

      // wait for recording to be done
      vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compNodeShared.compTimeline);

      compBaseCycleValue += MXC_CYCLE_COUNT;
      vkmSubmitPresentCommandBuffer(compNodeShared.cmd,
                                    compNodeShared.chain,
                                    compNodeShared.acquireSemaphore,
                                    compNodeShared.renderCompleteSemaphore,
                                    compNodeShared.swapIndex,
                                    compNodeShared.compTimeline,
                                    compBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE,
                                    graphicsQueue);
    }
  }

  //  int result = pthread_join(testNodeContext.threadId, NULL);
  //  if (result != 0) {
  //    perror("Thread join failed");
  //    return 1;
  //  }

  return EXIT_SUCCESS;
}
