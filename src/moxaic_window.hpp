#pragma once

#include "bit_flags.hpp"
#include "main.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace Moxaic::Window
{
    enum UserMove {
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

    MXC_RESULT Init();
    MXC_RESULT InitSurface(VkInstance vkInstance,
                           VkSurfaceKHR* pVkSurface);
    std::vector<char const*> GetVulkanInstanceExtentions();
    void Poll();
    void Cleanup();

    UserCommand const& userCommand();
    VkExtent2D const& extents();
    SDL_Window const* window();
}// namespace Moxaic::Window
