#include <stdio.h>
#include <stdlib.h>

#include "compositor.h"
#include "node.h"
#include "node_thread.h"
#include "window.h"

#include "mid_vulkan.h"
#include "mid_window.h"

MxcView compositorView = MXC_VIEW_STEREO;
bool isCompositor = true;
_Atomic bool isRunning = true;

int main(void)
{
//	int test[10] = {};
//	int a = test[11];

//	block_h h = HANDLE_GENERATION_SET(1000, 1);
//	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, (block_h) 10000);

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

	////
	//// Initialize
	////
	{
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

		mxcInitializeNode();

#if defined(MOXAIC_COMPOSITOR)
		printf("Moxaic Compositor\n");
		isCompositor = true;
		mxcCreateAndRunCompositorThread(vk.surfaces[0]);
		mxcServerInitializeInterprocess();

#define TEST_NODE
#ifdef TEST_NODE
//		node_h hTestNode; mxcRequestNodeThread(mxcRunNodeThread, &hTestNode);
//		MxcNodeShared* pTestNodeShrd = ARRAY_H(node.pShared, hTestNode);
//		pTestNodeShrd->compositorCycleSkip = 8;
//
//		node_h testNodeHandle2;
//		mxcRequestNodeThread(mxcRunNodeThread, &testNodeHandle2);
//		node.pShared[testNodeHandle]->compositorCycleSkip = 24;
#endif

#elif defined(MOXAIC_NODE)
		printf("Moxaic node\n");
		isCompositor = false;
		mxcConnectInterprocessNode(true);
#endif
	}

	////
	//// Main Compositor Loop
	////
	if (isCompositor) {

		VkDevice device = vk.context.device;
		VkQueue  graphicsQueue = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		while (isRunning) {

			/* MXC_CYCLE_UPDATE_WINDOW_STATE */
			vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, compositorContext.timeline);
			ATOMIC_FENCE_SCOPE {
				// This needs to be after a wait, and before a signal, as it will poll the IPC Message queue
				// and those may make changes to active nodes, or other, which subsequent states will rely on
				mxcNodeInterprocessPoll();
				vkSubmitQueuedCommandBuffers();

				midUpdateWindowInput();
				mxcProcessWindowInput();
				isRunning = midWindow.running;
			}

			vkTimelineSignal(device, compositorContext.baseCycleValue + MXC_CYCLE_PROCESS_INPUT, compositorContext.timeline);
			/* MXC_CYCLE_PROCESS_INPUT */

			/* MXC_CYCLE_UPDATE_NODE_STATES */

			/* MXC_CYCLE_COMPOSITOR_RECORD */

			/* MXC_CYCLE_RENDER_COMPOSITE */
			vkTimelineWait(device, compositorContext.baseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compositorContext.timeline);
			ATOMIC_FENCE_SCOPE {
				atomic_thread_fence(memory_order_acquire);
				compositorContext.baseCycleValue += MXC_CYCLE_COUNT;
				CmdSubmitPresent(
						compositorContext.gfxCmd,
						VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS,
						compositorContext.swapCtx,
						compositorContext.timeline,
						compositorContext.baseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE);
				vkSubmitQueuedCommandBuffers();
			}

		}

	////
	//// Main Node Loop
	////
	} else {

		VkQueue graphicsQueue = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
		while (isRunning) {
			midUpdateWindowInput();
			isRunning = midWindow.running;

			// I guess technically we just want to go as fast as possible in a node, but we would probably need to process and send input here first at some point?
			// we probably want to signal and wait on semaphore here
			// but we don't want this running if we aren't even using vulkan!
			vkSubmitQueuedCommandBuffers();
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
