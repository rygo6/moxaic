#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#ifdef WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

#define MXC_COLOR_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
#define MXC_NORMAL_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
#define MXC_G_BUFFER_USAGE (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
#define MXC_DEPTH_BUFFER_USAGE (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)

#define MXC_COLOR_BUFFER_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define MXC_NORMAL_BUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_G_BUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_DEPTH_BUFFER_FORMAT VK_FORMAT_D32_SFLOAT

#define VK_ALLOC nullptr

#define VK_CHK(command) \
({ \
    Moxaic::VkDebug.VulkanDebugLine = __LINE__; \
    Moxaic::VkDebug.VulkanDebugFile = MXC_FILE_NO_PATH; \
    Moxaic::VkDebug.VulkanDebugCommand = #command; \
    VkResult result = command; \
    if (result != VK_SUCCESS) { \
        printf("(%s:%d) VKCheck fail on command: %s - %d\n", Moxaic::VkDebug.VulkanDebugFile, Moxaic::VkDebug.VulkanDebugLine, Moxaic::VkDebug.VulkanDebugCommand, result); \
        return false; \
    } \
})

namespace Moxaic
{
    bool VulkanInit(SDL_Window* pWindow, bool enableValidationLayers);

    VkInstance VulkanInstance();
    VkSurfaceKHR VulkanSurface();

    struct VulkanFunc
    {
#ifdef WIN32
        PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR;
#endif
        PFN_vkCmdDrawMeshTasksEXT CmdDrawMeshTasksEXT;
        PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    };

    extern struct VulkanFunc VkFunc;

    struct VulkanDebug
    {
        int VulkanDebugLine;
        const char *VulkanDebugCommand;
        const char *VulkanDebugFile;
    };

    extern struct VulkanDebug VkDebug;
}
