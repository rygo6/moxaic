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

    enum class Phase : uint8_t {
        Pressed = 0,
        Released = 1,
    };

    enum class Button : uint8_t {
        None = 0,
        Left = 1,
        Middle = 2,
        Right = 3,
    };

    struct MouseClickEvent
    {
        Phase phase;
        Button button;
    };

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    inline constinit float k_MouseSensitivity = 0.01f;

    extern std::vector<void (*)(const MouseMotionEvent&)> g_MouseMotionSubscribers;
    extern std::vector<void (*)(const MouseClickEvent&)> g_MouseClickSubscribers;

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}