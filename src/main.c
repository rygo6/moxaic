#include "globals.h"
#include "node.h"
#include "renderer.h"
#include "test_node.h"

#include "comp_node.h"

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
  const VkSamplerCreateInfo linearSamplerInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 16.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };
  vkCreateSampler(context.device, &linearSamplerInfo, VKM_ALLOC, &context.linearSampler);
//  VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &context.linearSampler);
  // standard/common rendering
  VkmCreateStdRenderPass(&context.stdRenderPass);
  vkmCreateStdPipe(&context.stdPipe);
  // global set
  vkmCreateGlobalSet(&context.globalSet);
  vkmUpdateGlobalSet(&context.globalCameraTransform, &context.globalSetState, context.globalSet.pMapped);


  MxcBasicComp           basicComp;
  MxcBasicCompCreateInfo basicCompInfo = {
      .surface = surface,
  };
  mxcCreateBasicComp(&basicCompInfo, &basicComp);


  mxc_node_handle testNodeHandle = 0;
  MxcTestNode    testNode;
  MxcNodeContext* pTestNodeContext = &MXC_NODE[testNodeHandle];
  *pTestNodeContext = (MxcNodeContext) {
      .nodeType = MXC_NODE_TYPE_THREAD,
      .compCycleSkip = 16,
      .pNode = &testNode,
      .runFunc = mxcRunTestNode,
      .compTimeline = basicComp.timeline,
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
      .surface = surface,
      .transform = {0, 0, 0},
      .pFramebuffers = pTestNodeContext->framebuffers,
  };
  mxcCreateTestNode(&testNodeCreateInfo, &testNode);
  mxcCreateNodeContext(pTestNodeContext);
  MXC_NODE_SHARED[0] = (MxcNodeContextShared) {};
  MXC_NODE_SHARED[0].cmd = testNode.cmd; // we should request node slot first
  mxcRegisterCompNodeThread(testNodeHandle);
  MXC_NODE_COUNT = 1;


  mxcRunCompNode(&basicComp);


//  int result = pthread_join(testNodeContext.threadId, NULL);
//  if (result != 0) {
//    perror("Thread join failed");
//    return 1;
//  }

  return EXIT_SUCCESS;
}
