#include <stdio.h>
#include <stdlib.h>

#include "mid_math.h"
#include "mid_vulkan.h"
#include "mid_window.h"

#include "compositor.h"
#include "node.h"
#include "node_thread.h"
#include "window.h"

MxcView compositorView = MXC_VIEW_STEREO;
bool isCompositor = true;
bool isRunning = true;

int main(void)
{
	//	// we should make this to some kind of tests class
//	MxcSwapInfo info = {
//		.yScale = MXC_SWAP_SCALE_FULL,
//		.xScale = MXC_SWAP_SCALE_FULL,
//		.type = MXC_SWAP_TYPE_SINGLE,
//	};
//
//	swap_index_t swapsIndices[256];
//	int index = 0;
//	for (int i = 0; i < 260; ++i){
//		printf("Trying claim %d\n", i);
//		int newSwap = mxcClaimSwap(&info);
//		if (newSwap != -1) {
//			swapsIndices[index] = newSwap;
//			index++;
//		}
//	}
//
//	for (int i = 10; i < 256; i += 8){
//		printf("Trying release %d\n", i);
//		mxcReleaseSwap(&info, swapsIndices[i]);
//	}
//
//	index = 0;
//	for (int i = 0; i < 260; ++i){
//		printf("Trying claim %d\n", i);
//		int newSwap = mxcClaimSwap(&info);
//		if (newSwap != -1) {
//			swapsIndices[index] = newSwap;
//			index++;
//		}
//	}
//
//	return 0;

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

		vkInitializeInstance();
		VkContextCreateInfo contextCreateInfo = {
			.queueFamilyCreateInfos = {
				[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS] = {
					.supportsGraphics = VKM_SUPPORT_YES,
					.supportsCompute = VKM_SUPPORT_YES,
					.supportsTransfer = VKM_SUPPORT_YES,
					.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
					.queueCount = 1,
					.pQueuePriorities = (float[]){1.0f},
				},
				[VK_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = {
					.supportsGraphics = VKM_SUPPORT_NO,
					.supportsCompute = VKM_SUPPORT_YES,
					.supportsTransfer = VKM_SUPPORT_YES,
					.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
					.queueCount = 1,
					.pQueuePriorities = (float[]){1.0f},
				},
				[VK_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER] = {
					.supportsGraphics = VKM_SUPPORT_NO,
					.supportsCompute = VKM_SUPPORT_NO,
					.supportsTransfer = VKM_SUPPORT_YES,
					.queueCount = 1,
					.pQueuePriorities = (float[]){0.0f},
				},
			},
		};
		vkCreateContext(&contextCreateInfo);
		vkCreateVulkanSurface(midWindow.hInstance, midWindow.hWnd, VK_ALLOC, &vk.surfaces[0]);
		vkCreateGraphics();
		vkCreateLineGraphics();

		// I should do away with this, use basic renderpass and pipe in test node
//		mxcCreateNodeRenderPass();
//		vkCreateBasicPipe("./shaders/basic_material.vert.spv",
//						  "./shaders/basic_material.frag.spv",
//						  vkNode.basicPass,
//						  vk.context.basicPipeLayout,
//						  &vkNode.basicPipe);

		mxcInitializeNode();

#if defined(MOXAIC_COMPOSITOR)
		printf("Moxaic Compositor\n");
		isCompositor = true;
		mxcRequestAndRunCompositorNodeThread(vk.surfaces[0], mxcCompNodeThread);
		mxcServerInitializeInterprocess();

//#define TEST_NODE
#ifdef TEST_NODE
		NodeHandle testNodeHandle;
		mxcRequestNodeThread(mxcRunNodeThread, &testNodeHandle);

		NodeHandle testNodeHandle2;
		mxcRequestNodeThread(mxcRunNodeThread, &testNodeHandle2);
#endif

#elif defined(MOXAIC_NODE)
		printf("Moxaic node\n");
		isCompositor = false;
		mxcConnectInterprocessNode(true);
#endif

	}

	if (isCompositor) {  // Compositor Loop
		VkDevice device = vk.context.device;
		VkQueue  graphicsQueue = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		while (isRunning) {

			vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compositorContext.timeline);
			/// MXC_CYCLE_UPDATE_WINDOW_STATE

			mxcNodeInterprocessPoll(); // I am not too sure where I should put this

			midUpdateWindowInput();
			mxcProcessWindowInput();
			isRunning = midWindow.running;
			atomic_thread_fence(memory_order_release);

			vkTimelineSignal(device, compositorContext.baseCycleValue + MXC_CYCLE_PROCESS_INPUT, compositorContext.timeline);
			/// MXC_CYCLE_PROCESS_INPUT

			/// MXC_CYCLE_UPDATE_NODE_STATES

			/// MXC_CYCLE_COMPOSITOR_RECORD

			vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compositorContext.timeline);
			/// MXC_CYCLE_RENDER_COMPOSITE

			atomic_thread_fence(memory_order_acquire);
			compositorContext.baseCycleValue += MXC_CYCLE_COUNT;
			CmdSubmitPresent(
				compositorContext.gfxCmd,
				VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS,
				compositorContext.swapCtx,
				compositorContext.timeline,
				compositorContext.baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);

			mxcSubmitQueuedNodeCommandBuffers(graphicsQueue); // I am not too sure where I should put this

		}
	} else {

		const VkQueue graphicsQueue = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
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
	mxcServerShutdownInterprocess();
#elif defined(MOXAIC_NODE)
	mxcShutdownInterprocessNode();
#endif

	return EXIT_SUCCESS;
}
