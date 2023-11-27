#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

// inline constexpr VkFormat k_ColorBufferFormat = VK_FORMAT_B8G8R8A8_SRGB;
inline constexpr VkFormat k_ColorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
inline constexpr VkFormat k_NormalBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr VkFormat k_GBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr VkFormat k_DepthBufferFormat = VK_FORMAT_D32_SFLOAT;
// inline constexpr VkFormat k_DepthBufferFormat = VK_FORMAT_R32_SFLOAT;

#define VK_ALLOC nullptr

#define VK_CHK(command)                                          \
    ({                                                           \
        Moxaic::Vulkan::VkDebug.DebugLine = __LINE__;            \
        Moxaic::Vulkan::VkDebug.DebugFile = MXC_FILE_NO_PATH;    \
        Moxaic::Vulkan::VkDebug.DebugCommand = #command;         \
        VkResult result = command;                               \
        if (result != VK_SUCCESS) [[unlikely]] {                 \
            printf("(%s:%d) VKCheck fail on command: %s - %s\n", \
                   Moxaic::Vulkan::VkDebug.DebugFile,            \
                   Moxaic::Vulkan::VkDebug.DebugLine,            \
                   Moxaic::Vulkan::VkDebug.DebugCommand,         \
                   string_VkResult(result));                     \
            return false;                                        \
        }                                                        \
    })

#define VK_CHK_VOID(command)                                  \
    ({                                                        \
        Moxaic::Vulkan::VkDebug.DebugLine = __LINE__;         \
        Moxaic::Vulkan::VkDebug.DebugFile = MXC_FILE_NO_PATH; \
        Moxaic::Vulkan::VkDebug.DebugCommand = #command;      \
        command;                                              \
    })

namespace Moxaic::Vulkan
{
    bool Init(bool enableValidationLayers);

    VkInstance vkInstance();

    // should surface move into swap class?! or device class?
    VkSurfaceKHR vkSurface();

    enum class Queue {
        None,
        Graphics,
        Compute,
        FamilyExternal,
        Ignore,
    };

    struct Barrier
    {
        VkAccessFlags colorAccessMask;
        VkAccessFlags depthAccessMask;
        VkImageLayout colorLayout;
        VkImageLayout depthLayout;
        Queue queueFamilyIndex;
        VkPipelineStageFlags colorStageMask;
        VkPipelineStageFlags depthStageMask;

        VkAccessFlags GetAccessMask(const VkImageAspectFlags aspectMask) const
        {
            switch (aspectMask) {
                case VK_IMAGE_ASPECT_COLOR_BIT:
                    return colorAccessMask;
                case VK_IMAGE_ASPECT_DEPTH_BIT:
                    return depthAccessMask;
                default:
                    SDL_assert(false);
            }
        }

        VkImageLayout GetLayout(const VkImageAspectFlags aspectMask) const
        {
            switch (aspectMask) {
                case VK_IMAGE_ASPECT_COLOR_BIT:
                    return colorLayout;
                case VK_IMAGE_ASPECT_DEPTH_BIT:
                    return depthLayout;
                default:
                    SDL_assert(false);
            }
        }

        VkPipelineStageFlags GetStageMask(const VkImageAspectFlags aspectMask) const
        {
            switch (aspectMask) {
                case VK_IMAGE_ASPECT_COLOR_BIT:
                    return colorStageMask;
                case VK_IMAGE_ASPECT_DEPTH_BIT:
                    return depthStageMask;
                default:
                    SDL_assert(false);
            }
        }
    };

    inline constinit Barrier FromInitial{
        .colorAccessMask = VK_ACCESS_NONE,
        .depthAccessMask = VK_ACCESS_NONE,
        .colorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .depthLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .queueFamilyIndex = Queue::Ignore,
        .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      };

    /// Omits any of the incoming external data
    inline constinit Barrier AcquireFromExternal{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .depthLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .queueFamilyIndex = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };

    /// Retains data from prior external graphics attach
    inline constinit Barrier AcquireFromExternalGraphicsAttach{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .queueFamilyIndex = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };

    inline constinit Barrier FromComputeSwapPresent{
      .colorAccessMask = 0,
      .depthAccessMask = 0,
      .colorLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .depthLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };

    inline constinit Barrier FromComputeWrite{
      .colorAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };

    inline constinit Barrier FromComputeRead{
      .colorAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .depthAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };

    inline constinit Barrier AcquireFromComputeRead{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };

    // Seems acquireFromExternal can be used in place of this
    // if this is used it wants queueFamilyIndex to be ignore
    // for both src/dst which would need to implement some logic to do that
    inline constinit Barrier AcquireFromAnywhere{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .depthLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .queueFamilyIndex = Queue::Ignore,
      .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };

    inline constinit Barrier FromGraphicsAttach{
      .colorAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .queueFamilyIndex = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    };

    inline constinit Barrier AcquireFromGraphicsAttach{
      .colorAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .queueFamilyIndex = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };

    inline constinit Barrier ToGraphicsAttach{
      .colorAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .queueFamilyIndex = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    };

    inline constinit Barrier ToGraphicsRead{
      .colorAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .depthAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .queueFamilyIndex = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    };

    inline constinit Barrier ReleaseToComputeRead{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    };

    inline constinit Barrier ToComputeRead{
      .colorAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .depthAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };

    inline constinit Barrier ToComputeWrite{
      .colorAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .colorLayout = VK_IMAGE_LAYOUT_GENERAL,
      .depthLayout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };

    /// Release to retain data
    inline constinit Barrier ReleaseToExternalGraphicsRead{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .depthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .queueFamilyIndex = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    };

    inline constinit Barrier ToComputeSwapPresent{
      .colorAccessMask = VK_ACCESS_NONE,
      .depthAccessMask = VK_ACCESS_NONE,
      .colorLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .depthLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .queueFamilyIndex = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    };

    inline constinit VkImageSubresourceRange DefaultColorSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };

    inline constinit VkImageSubresourceRange DefaultDepthSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };

    enum class Locality : char {
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
#define VK_FUNCS                          \
    VK_FUNC(GetMemoryWin32HandleKHR)      \
    VK_FUNC(CmdDrawMeshTasksEXT)          \
    VK_FUNC(CreateDebugUtilsMessengerEXT) \
    VK_FUNC(GetSemaphoreWin32HandleKHR)   \
    VK_FUNC(ImportSemaphoreWin32HandleKHR)
#define VK_FUNC(func) PFN_vk##func func;
        VK_FUNCS
#undef VK_FUNC
    };
    inline Func VkFunc;
}// namespace Moxaic::Vulkan
