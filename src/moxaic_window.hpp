#pragma once

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

namespace Moxaic
{
    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}