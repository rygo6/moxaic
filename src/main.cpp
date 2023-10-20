#include "main.hpp"

#include <iostream>
#include <SDL2/SDL.h>

#include "moxaic_window.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

namespace Moxaic
{
    bool g_ApplicationRunning = true;
}

int main(int argc, char *argv[])
{
    MXC_LOG("Starting Moxaic!");

    Moxaic::WindowInit();

    Moxaic::VulkanInit(Moxaic::g_pSDLWindow, true);

    while (Moxaic::g_ApplicationRunning) {
        Moxaic::WindowPoll();
    }

    Moxaic::WindowShutdown();

    return 0;
}
