#include <stdio.h>
#include <stdlib.h>

#include "mid_math.h"
#include "mid_vulkan.h"
#include "mid_window.h"

#include "compositor.h"
#include "node.h"
#include "test_node.h"
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

		vkCreateBasicGraphics();
		vkCreateLineGraphics();

		// I should do away with this, use basic renderpass and pipe in test node
//		mxcCreateNodeRenderPass();
//		vkCreateBasicPipe("./shaders/basic_material.vert.spv",
//						  "./shaders/basic_material.frag.spv",
//						  vkNode.basicPass,
//						  vk.context.basicPipeLayout,
//						  &vkNode.basicPipe);

#if defined(MOXAIC_COMPOSITOR)
		printf("Moxaic Compositor\n");
		isCompositor = true;
		mxcRequestAndRunCompositorNodeThread(vk.surfaces[0], mxcCompNodeThread);
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
		VkDevice device = vk.context.device;
		VkQueue  graphicsQueue = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		uint64_t compositorBaseCycleValue = 0;
		while (atomic_load_explicit(&midWindow.running, memory_order_acquire)) {

			// we may not have to even wait... this could go faster
			vkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compositorContext.timeline);

			// interprocess polling could be a different thread?
			// we must do it here when the comp thread is not rendering otherwise we can't clear resources if one closes
			mxcNodeInterprocessPoll();

			// somewhere input state needs to be copied to a node and only update when it knows the node needs it
			midUpdateWindowInput();
			mxcProcessWindowInput();
			atomic_thread_fence(memory_order_release);

			// signal input ready to process!
			vkTimelineSignal(device, compositorBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compositorContext.timeline);

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
			vkTimelineWait(device, compositorBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compositorContext.timeline);

			compositorBaseCycleValue += MXC_CYCLE_COUNT;

			atomic_thread_fence(memory_order_acquire);
			// itd be good to come up with a mechanism that can actually deal with real multiple queues for debugging
			CmdSubmitPresent(compositorContext.gfxCmd,
							 compositorContext.swapCtx.chain,
							 compositorContext.swapCtx.acquireSemaphore,
							 compositorContext.swapCtx.renderCompleteSemaphore,
							 compositorContext.swapIdx,
							 compositorContext.timeline,
							 compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);

			// Try submitting nodes before waiting to update window again.
			// We want input update and composite render to happen ASAP so main thread waits on those events, but tries to update other nodes in between.
			mxcSubmitQueuedNodeCommandBuffers(graphicsQueue);
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
	mxcShutdownInterprocessServer();
#elif defined(MOXAIC_NODE)
	mxcShutdownInterprocessNode();
#endif

	return EXIT_SUCCESS;
}
