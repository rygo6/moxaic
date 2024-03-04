#include "moxaic_renderer.hpp"
#include "mid_vulkan.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"

#include <windows.h>
#include <vulkan/vulkan_win32.h>

using namespace Mid::Vk;

void Moxaic::Renderer::Init()
{
    // auto test = ComputePipelineDesc{
    //   .debugName{"ComputeCompositePipeline"},
    //   .createInfos{
    //     {
    //       .stage{
    //         .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    //         // .module = shader,
    //         .pName = "main"},
    //       // .layout = sharedVkPipelineLayout,
    //     },
    //   },
    // };

    auto instance = Instance::Create(InstanceDesc{
      .createInfo{
        .pApplicationInfo = ApplicationInfo{
          .pApplicationName = "Moxaic",
        },
        .pEnabledLayerNames = {
          "VK_LAYER_KHRONOS_validation",
        },
        .pEnabledExtensionNames{
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
          VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
          VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
          VK_KHR_SURFACE_EXTENSION_NAME,
          VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        },
      },
      .validationFeatures{
        .pEnabledValidationFeatures{
          // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
          // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
          // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
          VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
          // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        },
      },
      .debugUtilsMessageSeverityFlags{
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      },
      .debugUtilsMessageTypeFlags{
        // VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        // VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      },
    });

    instance.CreatePhysicalDevice(PhysicalDeviceDesc{
      .preferredDeviceIndex = 0,
      .physicalDeviceFeatures{
        .features{
          .robustBufferAccess = VK_TRUE,
        }},
      .physicalDeviceFeatures11{},
      .physicalDeviceFeatures12{
        .samplerFilterMinmax = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
      },
      .physicalDeviceFeatures13{
        .synchronization2 = VK_TRUE,
      },
      .physicalDeviceMeshShaderFeatures{
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
      },
      .physicalDeviceGlobalPriorityQueryFeatures{
        .globalPriorityQuery = VK_TRUE,
      },
    });

    Window::Init();
    VkSurfaceKHR surface;
    Window::InitSurface(instance, &surface);
}