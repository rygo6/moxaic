#pragma once

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>

namespace Moxaic
{
    struct MouseMotionEvent
    {
        glm::vec2 delta;
    };

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    inline constinit float k_MouseSensitivity = 0.01f;

    extern std::vector<void (*)(MouseMotionEvent&)> g_MouseMotionSubscribers;

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}