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
    if (result != VK_SUCCESS) [[unlikely]] { \
        printf("(%s:%d) VKCheck fail on command: %s - %s\n", \
                Moxaic::VkDebug.VulkanDebugFile, \
                Moxaic::VkDebug.VulkanDebugLine, \
                Moxaic::VkDebug.VulkanDebugCommand, \
                string_VkResult(result)); \
        return false; \
    } \
})

#define VK_CHK_VOID(command) \
({ \
    Moxaic::VkDebug.VulkanDebugLine = __LINE__; \
    Moxaic::VkDebug.VulkanDebugFile = MXC_FILE_NO_PATH; \
    Moxaic::VkDebug.VulkanDebugCommand = #command; \
    command; \
})

namespace Moxaic
{
    bool VulkanInit(SDL_Window *pWindow, bool enableValidationLayers);

    VkInstance vkInstance();

    // should surface move into swap class?! or device class?
    VkSurfaceKHR vkSurface();

    inline constinit VkImageSubresourceRange k_DefaultColorSubresourceRange {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
    };

    enum Locality : char
    {
        Local,
        External,
    };

    inline const char *string_BufferLocality(Locality input_value)
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

    struct VulkanDebug
    {
        int VulkanDebugLine;
        const char *VulkanDebugCommand;
        const char *VulkanDebugFile;
    };

    extern struct VulkanDebug VkDebug;

    struct VulkanFunc
    {
#define VK_FUNCS \
        VK_FUNC(GetMemoryWin32HandleKHR) \
        VK_FUNC(CmdDrawMeshTasksEXT) \
        VK_FUNC(CreateDebugUtilsMessengerEXT) \
        VK_FUNC(GetSemaphoreWin32HandleKHR)
#define VK_FUNC(func) PFN_vk##func func;
        VK_FUNCS
#undef VK_FUNC
    };

    extern struct VulkanFunc VkFunc;
}
