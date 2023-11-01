#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_swap.hpp"
#include "moxaic_vulkan_timeline_semaphore.hpp"

using namespace Moxaic;

struct
{
    VulkanDevice *pDevice;
    VulkanFramebuffer *pFramebuffer;
    Camera *pCamera;
    GlobalDescriptor *pGlobalDescriptor;
    VulkanSwap *pSwap;
    VulkanTimelineSemaphore *pTimelineSemaphore;
} Core;

MXC_RESULT Moxaic::CoreInit()
{
    MXC_CHK(WindowInit());
    MXC_CHK(VulkanInit(g_pSDLWindow,
                       true));

    Core.pDevice = new VulkanDevice();
    MXC_CHK(Core.pDevice->Init());

    Core.pFramebuffer = new VulkanFramebuffer(*Core.pDevice);
    MXC_CHK(Core.pFramebuffer->Init(g_WindowDimensions,
                                    Locality::Local));

    Core.pCamera = new Camera();
    Core.pCamera->transform().position() = {0, 0, -2};

    Core.pGlobalDescriptor = new GlobalDescriptor(*Core.pDevice);
    MXC_CHK(Core.pGlobalDescriptor->Init());
    Core.pGlobalDescriptor->Update(*Core.pCamera,
                                   g_WindowDimensions);

    Core.pSwap = new VulkanSwap(*Core.pDevice);
    MXC_CHK(Core.pSwap->Init(g_WindowDimensions,
                             false));

    Core.pTimelineSemaphore = new VulkanTimelineSemaphore(*Core.pDevice);
    MXC_CHK(Core.pTimelineSemaphore->Init(false,
                                          Locality::Local));

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::CoreLoop()
{
    while (g_ApplicationRunning) {
        WindowPoll();

        Core.pDevice->BeginGraphicsCommandBuffer();
        Core.pDevice->BeginRenderPass(*Core.pFramebuffer);


        Core.pDevice->EndRenderPass();

        Core.pSwap->BlitToSwap(Core.pFramebuffer->colorTexture().vkImage());

        Core.pDevice->EndGraphicsCommandBuffer();

        Core.pDevice->SubmitGraphicsQueueAndPresent(*Core.pTimelineSemaphore, *Core.pSwap);

        Core.pTimelineSemaphore->Wait();
    }

    WindowShutdown();

    return MXC_SUCCESS;
}