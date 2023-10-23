#include "main.hpp"

#include <SDL2/SDL.h>
#include <memory>

#include "moxaic_window.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "moxaic_logging.hpp"

namespace Moxaic
{
    bool g_ApplicationRunning = true;
    Role g_Role = Compositor;

    bool Init()
    {
        if (!WindowInit()) {
            MXC_LOG_ERROR("Window Init Fail!");
            return false;
        }

        if (!VulkanInit(g_pSDLWindow, true)) {
            MXC_LOG_ERROR("Vulkan Init Fail!");
            return false;
        }

        auto vulkanDevice = std::make_unique<VulkanDevice>(VulkanInstance(), VulkanSurface());
        vulkanDevice->Init();

        auto vulkanTexture = VulkanTexture(*vulkanDevice);
        vulkanTexture.Init(VK_FORMAT_R8G8B8A8_UNORM,
                           {10, 10, 1},
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT);

        while (g_ApplicationRunning) {
            WindowPoll();
        }

        WindowShutdown();

        return true;
    }
}

int main(int argc, char *argv[])
{
    MXC_LOG("Starting Moxaic!");

    Moxaic::Init();

    return 0;
}
