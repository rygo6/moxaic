#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

inline constexpr VkFormat k_ColorBufferFormat = VK_FORMAT_B8G8R8A8_SRGB;
inline constexpr VkFormat k_NormalBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr VkFormat k_GBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr VkFormat k_DepthBufferFormat = VK_FORMAT_D32_SFLOAT;

#define VK_ALLOC nullptr

#define VK_CHK(command) \
({ \
    Moxaic::Vulkan::VkDebug.DebugLine = __LINE__; \
    Moxaic::Vulkan::VkDebug.DebugFile = MXC_FILE_NO_PATH; \
    Moxaic::Vulkan::VkDebug.DebugCommand = #command; \
    VkResult result = command; \
    if (result != VK_SUCCESS) [[unlikely]] { \
        printf("(%s:%d) VKCheck fail on command: %s - %s\n", \
                Moxaic::Vulkan::VkDebug.DebugFile, \
                Moxaic::Vulkan::VkDebug.DebugLine, \
                Moxaic::Vulkan::VkDebug.DebugCommand, \
                string_VkResult(result)); \
        return false; \
    } \
})

#define VK_CHK_VOID(command) \
({ \
    Moxaic::Vulkan::VkDebug.DebugLine = __LINE__; \
    Moxaic::Vulkan::VkDebug.DebugFile = MXC_FILE_NO_PATH; \
    Moxaic::Vulkan::VkDebug.DebugCommand = #command; \
    command; \
})

namespace Moxaic::Vulkan
{
    bool Init(bool enableValidationLayers);

    VkInstance vkInstance();

    // should surface move into swap class?! or device class?
    VkSurfaceKHR vkSurface();

    inline constinit VkImageSubresourceRange defaultColorSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    enum class Locality : char
    {
        Local,
        External,
    };

    inline const char* string_Locality(const Locality input_value)
    {
        switch (input_value) {
            case Locality::Local:
                return "Local";
            case Locality::External:
                return "External";
            default:
                return "Unknown Locality";
        }
    }

    struct Debug
    {
        int DebugLine;
        const char* DebugCommand;
        const char* DebugFile;
    };
    inline Debug VkDebug;

    struct Func
    {
#define VK_FUNCS \
        VK_FUNC(GetMemoryWin32HandleKHR) \
        VK_FUNC(CmdDrawMeshTasksEXT) \
        VK_FUNC(CreateDebugUtilsMessengerEXT) \
        VK_FUNC(GetSemaphoreWin32HandleKHR) \
        VK_FUNC(ImportSemaphoreWin32HandleKHR)
#define VK_FUNC(func) PFN_vk##func func;
        VK_FUNCS
#undef VK_FUNC
    };
    inline Func VkFunc;
}
