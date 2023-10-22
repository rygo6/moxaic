#pragma once

#include "main.hpp"

#include <vulkan/vulkan.hpp>
#include <SDL2/SDL.h>

#ifdef WIN32
#include <windows.h>
#endif

#define MXC_COLOR_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
#define MXC_NORMAL_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
#define MXC_G_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
#define MXC_DEPTH_BUFFER_USAGE (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)

#define MXC_COLOR_BUFFER_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define MXC_NORMAL_BUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_G_BUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_DEPTH_BUFFER_FORMAT VK_FORMAT_D32_SFLOAT

#define MXC_VK_ALLOCATOR nullptr

#define MXC_VK_CHECK(command) \
({                        \
    Moxaic::g_VulkanDebugLine = __LINE__; \
    Moxaic::g_VulkanDebugFile = MXC_FILE_NO_PATH; \
    Moxaic::g_VulkanDebugCommand = #command; \
    VkResult result = command; \
    if (result != VK_SUCCESS) { \
        printf("(%s:%d) VKCheck fail on command: %s - %d\n", Moxaic::g_VulkanDebugFile, Moxaic::g_VulkanDebugLine, Moxaic::g_VulkanDebugCommand, result); \
        return false; \
    } \
})

namespace Moxaic
{
    bool VulkanInit(SDL_Window* pWindow, bool enableValidationLayers);

    extern int g_VulkanDebugLine;
    extern const char* g_VulkanDebugCommand;
    extern const char* g_VulkanDebugFile;
}