#include "main.hpp"

#include <SDL2/SDL.h>

#include "moxaic_window.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "moxaic_logging.hpp"

namespace Moxaic
{
    bool g_ApplicationRunning = true;
    Role g_Role = Compositor;
}

int main(int argc, char *argv[])
{
    MXC_LOG("Starting Moxaic!");

    if (!Moxaic::WindowInit()) {
        MXC_LOG_ERROR("Window Init Fail!");
        return 1;
    }

    if (!Moxaic::VulkanInit(Moxaic::g_pSDLWindow, true)) {
        MXC_LOG_ERROR("Vulkan Init Fail!");
        return 1;
    }

    auto vulkanDevice = Moxaic::VulkanDevice(Moxaic::VulkanInstance(), Moxaic::VulkanSurface());
    vulkanDevice.Init();

    auto vulkanTexture = Moxaic::VulkanTexture(vulkanDevice);
    vulkanTexture.Create(VK_FORMAT_R8G8B8A8_UNORM, {10,10, 1}, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, false);

    while (Moxaic::g_ApplicationRunning) {
        Moxaic::WindowPoll();
    }

    Moxaic::WindowShutdown();

    return 0;
}
