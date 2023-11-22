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
    MXC_RESULT InitSurface(const VkInstance& vkInstance,
                           VkSurfaceKHR* pVkSurface);
    std::vector<const char*> GetVulkanInstanceExtentions();
    void Poll();
    void Cleanup();

    const UserCommand& userCommand();
    const VkExtent2D& extents();
    const SDL_Window* window();
}// namespace Moxaic::Window
