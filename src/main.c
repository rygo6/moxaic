#include "comp_node.h"
#include "node.h"
#include "test_node.h"
#include "window.h"

// build
//#include "comp_node.c"
//#include "node.c"
//#include "test_node.c"
//#include "window.c"
//#include "mid_vulkan.c"

#define MID_DEBUG
[[noreturn]] void Panic(const char* file, int line, const char* message)
{
	fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
	__builtin_trap();
}

#define MID_OPENXR_IMPLEMENTATION
#include "mid_openxr_runtime.h"

#define MID_MATH_IMPLEMENTATION
#include "mid_math.h"

#define MID_VULKAN_IMPLEMENTATION
#include "mid_vulkan.h"

#define MID_SHAPE_IMPLEMENTATION
#include "mid_shape.h"

#define MID_WINDOW_IMPLEMENTATION
#include "mid_window.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

bool isCompositor = true;
bool isRunning = true;

int main(void)
{
	//typedef PFN_vkGetInstanceProcAddr GetInstanceProcAddrFunc;
	//    HMODULE vulkanLibrary = LoadLibrary("vulkan-1.dll");
	//    if (vulkanLibrary == NULL) {
	//      fprintf(stderr, "Failed to load Vulkan library.\n");
	//      return EXIT_FAILURE;
	//    }
	//    GetInstanceProcAddrFunc vkGetInstanceProcAddr = (GetInstanceProcAddrFunc)GetProcAddress(vulkanLibrary, "vkGetInstanceProcAddr");
	//    if (vkGetInstanceProcAddr == NULL) {
	//      fprintf(stderr, "Failed to retrieve function pointer to vkGetInstanceProcAddr.\n");
	//      return EXIT_FAILURE;
	//    }

	{  // Initialize
		midCreateWindow();
		midVkInitialize();

		midVkCreateVulkanSurface(midWindow.hInstance, midWindow.hWnd, MIDVK_ALLOC, &midVk.surfaces[0]);

		MidVkContextCreateInfo contextCreateInfo = {
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
		midVkCreateContext(&contextCreateInfo);

		// these probably should go elsewhere ?
		// global samplers
		VkmSamplerCreateInfo samplerCreateInfo = {
			.filter = VK_FILTER_LINEAR,
			.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,
		};
		midVkCreateSampler(&samplerCreateInfo, &midVk.context.linearSampler);
		// standard/common rendering
		midVkCreateStdRenderPass();
		midVkCreateStdPipeLayout();

		// this need to ge in node create
		mxcCreateNodeRenderPass();
		midVkCreateBasicPipe("./shaders/basic_material.vert.spv",
							 "./shaders/basic_material.frag.spv",
							 midVk.context.nodeRenderPass,
							 midVk.context.basicPipeLayout.pipeLayout,
							 &midVk.context.basicPipe);

#if defined(MOXAIC_COMPOSITOR)
		printf("Moxaic Compositor\n");
		isCompositor = true;
		mxcRequestAndRunCompositorNodeThread(midVk.surfaces[0], mxcCompNodeThread);
		mxcInitializeInterprocessServer();

//#define TEST_NODE
#ifdef TEST_NODE
		NodeHandle testNodeHandle;
		mxcRequestNodeThread(mxcTestNodeThread, &testNodeHandle);
#endif

#elif defined(MOXAIC_NODE)
		printf("Moxaic node\n");
		isCompositor = false;
		mxcConnectInterprocessNode(true);
#endif
	}


	if (isCompositor) {  // Compositor Loop
		const VkDevice device = midVk.context.device;
		const VkQueue  graphicsQueue = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		uint64_t compositorBaseCycleValue = 0;
		while (isRunning) {

			// we may not have to even wait... this could go faster
			midVkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compositorNodeContext.compTimeline);

			// interprocess polling could be a different thread?
			// we must do it here when the comp thread is not rendering otherwise we can't clear resources if one closes
			mxcInterprocessQueuePoll();

			// somewhere input state needs to be copied to a node and only update when it knows the node needs it
			midUpdateWindowInput();
			isRunning = midWindow.running;
			mxcProcessWindowInput();
			__atomic_thread_fence(__ATOMIC_RELEASE);

			// signal input ready to process!
			midVkTimelineSignal(device, compositorBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compositorNodeContext.compTimeline);

			// MXC_CYCLE_COMPOSITOR_RECORD occurs here

			// Compositor processes input and updates nodes.
			// Nodes record command buffers.
			// Very unlikely to make it for this present, will be in the next one.
			// After MXC_CYCLE_PROCESS_INPUT and MXC_CYCLE_COMPOSITOR_RECORD we want to get that submitted ASAP

			// Try submitting nodes before waiting to render composite
			// We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
			// does this really make that big a difference?
			//      mxcSubmitQueuedNodeCommandBuffers(graphicsQueue);

			// wait for recording to be done
			midVkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compositorNodeContext.compTimeline);

			compositorBaseCycleValue += MXC_CYCLE_COUNT;
			__atomic_thread_fence(__ATOMIC_RELEASE); // should use atomic add ?

			__atomic_thread_fence(__ATOMIC_ACQUIRE);
			const int swapIndex = compositorNodeContext.swapIndex;
			midVkSubmitPresentCommandBuffer(compositorNodeContext.cmd,
											compositorNodeContext.swap.chain,
											compositorNodeContext.swap.acquireSemaphore,
											compositorNodeContext.swap.renderCompleteSemaphore,
											swapIndex,
											compositorNodeContext.compTimeline,
											compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);

			// Try submitting nodes before waiting to update window again.
			// We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
			mxcSubmitQueuedNodeCommandBuffers(graphicsQueue);
		}
	} else {

		const VkQueue graphicsQueue = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		while (isRunning) {
			midUpdateWindowInput();
			isRunning = midWindow.running;

			// I guess technically we just want to go as fast as possible in a node, but we would probably need to process and send input here first at some point?
			// we probably want to signal and wait on semaphore here
			mxcSubmitQueuedNodeCommandBuffers(graphicsQueue);
		}
	}

	// todo rejoin running threads
	//  int result = pthread_join(testNodeContext.threadId, NULL);
	//  if (result != 0) {
	//    perror("Thread join failed");
	//    return 1;
	//  }

#if defined(MOXAIC_COMPOSITOR)
	mxcShutdownInterprocessServer();
#elif defined(MOXAIC_NODE)
	mxcShutdownInterprocessNode();
#endif

	return EXIT_SUCCESS;
}
