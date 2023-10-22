#include "main.hpp"

#include <SDL2/SDL.h>

#include "moxaic_window.hpp"
#include "moxaic_vulkan.hpp"
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

    while (Moxaic::g_ApplicationRunning) {
        Moxaic::WindowPoll();
    }

    Moxaic::WindowShutdown();

    return 0;
}
