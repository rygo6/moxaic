#pragma once

#include "BitFlags.hpp"

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Moxaic
{
    enum UserMove
    {
        Forward = 1 << 0,
        Back = 1 << 1,
        Left = 1 << 2,
        Right = 1 << 3,
        Rotation = 1 << 4,
    };

    struct UserCommand
    {
        bool mouseMoved;
        glm::vec2 mouseDelta;
        bool leftMouseButtonPressed;
        BitFlags<UserMove> userMove;
    };

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    const UserCommand& getUserCommand();

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;
}