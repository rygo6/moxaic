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

    const UserCommand &userCommand();
    const VkExtent2D windowExtents();
    SDL_Window *window();
}