#pragma once

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

namespace Moxaic
{
    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    inline constinit float k_MouseSensitivity = 0.001f;

    extern float g_DeltaMouseX;
    extern float g_DeltaMouseY;

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}