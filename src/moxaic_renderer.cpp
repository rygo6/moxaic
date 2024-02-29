#include "moxaic_renderer.hpp"
#include "mid_vulkan.hpp"
#include "moxaic_logging.hpp"
#include "static_array.hpp"

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

    auto instance = Instance::Create({
      .createInfo{
        .pNext = ValidationFeatures{
          .pEnabledValidationFeatures{
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
          },
        },
        .pApplicationInfo = ApplicationInfo{
          .pApplicationName = "Moxaic",
        },
        .pEnabledLayerNames{
          "VK_LAYER_KHRONOS_validation",
        },
        .pEnabledExtensionNames{
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
          VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
          VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        },
      },
    });
    MXC_LOG(instance.ResultName());
    if (!instance.IsValid()) {
        MXC_LOG_ERROR(instance.ResultName());
    }
}