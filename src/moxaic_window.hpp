#pragma once

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <functional>
#include <any>

namespace Moxaic
{
    enum class Phase : uint8_t
    {
        Pressed = 0,
        Released = 1,
    };

    enum class Button : uint8_t
    {
        None = 0,
        Left = 1,
        Middle = 2,
        Right = 3,
    };

    struct MouseMotionEvent
    {
        glm::vec2 delta;
    };

    struct MouseEvent
    {
        Phase phase;
        Button button;
    };

    struct KeyEvent
    {
        Phase phase;
        SDL_Keycode key;
    };

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    using MouseMotionCallback = void(const MouseMotionEvent &);
    using MouseCallback = void(const MouseEvent &);
    using KeyCallback = void(const KeyEvent &);

    extern std::vector<std::function<MouseMotionCallback>> g_MouseMotionSubscribers;
    extern std::vector<std::function<MouseCallback>> g_MouseSubscribers;
    extern std::vector<std::function<KeyCallback>> g_KeySubscribers;

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}