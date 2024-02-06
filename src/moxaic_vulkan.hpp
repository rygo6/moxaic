#pragma once

#include "moxaic_logging.hpp"

#include <cassert>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#ifdef WIN32
#include <windows.h>
#endif
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

// inline constexpr VkFormat k_ColorBufferFormat = VK_FORMAT_B8G8R8A8_SRGB;
inline constexpr VkFormat kColorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
inline constexpr VkFormat kNormalBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
// inline constexpr VkFormat kGBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr VkFormat kGBufferFormat = VK_FORMAT_R32_SFLOAT;//make same as depth until blit issue is solved
inline constexpr VkFormat kDepthBufferFormat = VK_FORMAT_D32_SFLOAT;

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

    VkInstance GetVkInstance();

    // should surface move into swap class?! or device class?
    VkSurfaceKHR GetVkSurface();

    enum class PipelineType {
        Graphics,
        Compute,
    };

    inline PipelineType CompositorPipelineType = PipelineType::Compute;

    enum class Queue {
        Ignore,
        Graphics,
        Compute,
        FamilyExternal,
    };

    struct Barrier2
    {
        VkAccessFlagBits2 colorAccessMask;
        VkAccessFlagBits2 depthAccessMask;
        VkImageLayout layout;
        Queue queueFamily;
        VkPipelineStageFlagBits2 colorStageMask;
        VkPipelineStageFlagBits2 depthStageMask;

        VkAccessFlagBits2 GetAccessMask(const VkImageAspectFlags aspectMask) const
        {
            switch (aspectMask) {
                case VK_IMAGE_ASPECT_COLOR_BIT:
                    return colorAccessMask;
                case VK_IMAGE_ASPECT_DEPTH_BIT:
                    return depthAccessMask;
                default:
                    assert(false);
            }
        }

        VkPipelineStageFlagBits2 GetStageMask(const VkImageAspectFlags aspectMask) const
        {
            switch (aspectMask) {
                case VK_IMAGE_ASPECT_COLOR_BIT:
                    return colorStageMask;
                case VK_IMAGE_ASPECT_DEPTH_BIT:
                    return depthStageMask;
                default:
                    assert(false);
            }
        }
    };

    inline constexpr Barrier2 FromUndefined2{
      .colorAccessMask = VK_ACCESS_2_NONE,
      .depthAccessMask = VK_ACCESS_2_NONE,
      .layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .queueFamily = Queue::Ignore,
      .colorStageMask = VK_PIPELINE_STAGE_2_NONE,
      .depthStageMask = VK_PIPELINE_STAGE_2_NONE,
    };

    inline constexpr Barrier2 GraphicsComputeWrite2{
        .colorAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .depthAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
        .queueFamily = Queue::Graphics,
        .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      };

    inline constexpr Barrier2 ComputeWrite2{
      .colorAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .layout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamily = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    };
    inline constexpr Barrier2 ToComputeWrite2{
      .colorAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .layout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamily = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    };

    inline constexpr Barrier2 ComputeReadWrite2{
        .colorAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        .depthAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
        .queueFamily = Queue::Compute,
        .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      };

    inline constexpr Barrier2 GraphicsComputeRead2{
        .colorAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .depthAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
        .queueFamily = Queue::Graphics,
        .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      };

    inline constexpr Barrier2 ComputeRead2{
      .colorAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .depthAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .layout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamily = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    };

    inline constexpr Barrier2 GraphicsRead2{
      .colorAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .depthAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
      .queueFamily = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    };

    inline constexpr Barrier2 GraphicsAttach2{
      .colorAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .queueFamily = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      .depthStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    };
    // inline constexpr Barrier2 ToGraphicsAttach2{
    //   .colorAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
    //   .depthAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    //   .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    //   .queueFamily = Queue::Graphics,
    //   .colorStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    //   .depthStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    // };

    /// Retains data from prior external graphics attach
    inline constexpr Barrier2 AcquireExternalGraphicsAttach2{
      .colorAccessMask = VK_ACCESS_2_NONE,
      .depthAccessMask = VK_ACCESS_2_NONE,
      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .queueFamily = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_2_NONE,
      .depthStageMask = VK_PIPELINE_STAGE_2_NONE,
    };

    inline constexpr Barrier2 ReleaseExternalGraphicsRead2{
      .colorAccessMask = VK_ACCESS_2_NONE,
      .depthAccessMask = VK_ACCESS_2_NONE,
      .layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
      .queueFamily = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_2_NONE,
      .depthStageMask = VK_PIPELINE_STAGE_2_NONE,
    };
    inline constexpr Barrier2 ReleaseExternalComputesRead2{
      .colorAccessMask = VK_ACCESS_2_NONE,
      .depthAccessMask = VK_ACCESS_2_NONE,
      .layout = VK_IMAGE_LAYOUT_GENERAL,
      .queueFamily = Queue::FamilyExternal,
      .colorStageMask = VK_PIPELINE_STAGE_2_NONE,
      .depthStageMask = VK_PIPELINE_STAGE_2_NONE,
    };

    constexpr Barrier2 ReleaseToExternalRead(const PipelineType pipelineType)
    {
        switch (pipelineType) {
            case PipelineType::Graphics:
                return ReleaseExternalGraphicsRead2;
            case PipelineType::Compute:
                return ReleaseExternalComputesRead2;
            default:
                assert(false);
        }
    }

    inline constexpr Barrier2 ToComputeSwapPresent2{
      .colorAccessMask = VK_ACCESS_2_NONE,
      .depthAccessMask = VK_ACCESS_2_NONE,
      .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .queueFamily = Queue::Compute,
      .colorStageMask = VK_PIPELINE_STAGE_2_NONE,
      .depthStageMask = VK_PIPELINE_STAGE_2_NONE,
    };

    inline constexpr Barrier2 BlitTransferSrc2{
      .colorAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
      .depthAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
      .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .queueFamily = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
    };

    inline constexpr Barrier2 BlitTransferDst2{
      .colorAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .depthAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .queueFamily = Queue::Graphics,
      .colorStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
      .depthStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
    };

    enum class Locality : char {
        Local,
        External,
        Imported,
    };

    inline const char* string_Locality(const Locality input_value)
    {
        switch (input_value) {
            case Locality::Local:
                return "Local";
            case Locality::External:
                return "External";
            case Locality::Imported:
                return "Imported";
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
#define VK_FUNCS                           \
    VK_FUNC(GetMemoryWin32HandleKHR)       \
    VK_FUNC(CmdDrawMeshTasksEXT)           \
    VK_FUNC(GetSemaphoreWin32HandleKHR)    \
    VK_FUNC(ImportSemaphoreWin32HandleKHR) \
    VK_FUNC(CreateDebugUtilsMessengerEXT)

#define VK_FUNC(func) PFN_vk##func func;
        VK_FUNCS
#undef VK_FUNC
    };
    inline Func VkFunc;
}// namespace Moxaic::Vulkan
