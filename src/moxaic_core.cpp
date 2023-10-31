#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_descriptor.hpp"

using namespace Moxaic;

VulkanDevice* g_pDevice;

bool Moxiac::CoreInit()
{
    MXC_CHK(WindowInit());
    MXC_CHK(VulkanInit(g_pSDLWindow, true));

    g_pDevice = new VulkanDevice();
    MXC_CHK(g_pDevice->Init());

    VulkanFramebuffer vulkanFramebuffer = VulkanFramebuffer(*g_pDevice);
    MXC_CHK(vulkanFramebuffer.Init({k_DefaultWidth,
                            k_DefaultHeight},
                           Moxaic::BufferLocality::Local));

    Camera camera = Camera();
    camera.transform().position() = {0, 0, -2};

    GlobalDescriptor globalDescriptor = GlobalDescriptor(*g_pDevice);
    MXC_CHK(globalDescriptor.Init());
    globalDescriptor.Update(camera, g_WindowDimensions);

    return true;
}

bool Moxiac::CoreLoop()
{
    while (g_ApplicationRunning) {
        WindowPoll();

        g_pDevice->BeginGraphicsCommandBuffer();


        VK_CHK(vkEndCommandBuffer(g_pDevice->vkGraphicsCommandBuffer()));
    }

    WindowShutdown();

    return true;
}