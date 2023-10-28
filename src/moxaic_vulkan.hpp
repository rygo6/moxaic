#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL2/SDL.h>

#ifdef WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

inline constinit VkFormat k_ColorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
inline constinit VkFormat k_NormalBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constinit VkFormat k_GBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constinit VkFormat k_DepthBufferFormat = VK_FORMAT_D32_SFLOAT;

#define VK_ALLOC nullptr

#define VK_CHK(command) \
({ \
    Moxaic::VkDebug.VulkanDebugLine = __LINE__; \
    Moxaic::VkDebug.VulkanDebugFile = MXC_FILE_NO_PATH; \
    Moxaic::VkDebug.VulkanDebugCommand = #command; \
    VkResult result = command; \
    if (result != VK_SUCCESS) { \
        printf("(%s:%d) VKCheck fail on command: %s - %s\n", \
                Moxaic::VkDebug.VulkanDebugFile, \
                Moxaic::VkDebug.VulkanDebugLine, \
                Moxaic::VkDebug.VulkanDebugCommand, \
                string_VkResult(result)); \
        return false; \
    } \
})

namespace Moxaic
{
    bool VulkanInit(SDL_Window *pWindow, bool enableValidationLayers);

    VkInstance vkInstance();
    VkSurfaceKHR vkSurface();

    enum BufferLocality : char {
        Local,
        External,
    };

    inline const char *string_BufferLocality(BufferLocality input_value)
    {
        switch (input_value) {
            case Local:
                return "Local";
            case External:
                return "External";
            default:
                return "Unknown Locality";
        }
    }

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
