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

    auto instance = Instance::Create({
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
      .debugUtilsMessengerCreateInfo{
        // .messageSeverity{
        //     VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        //     VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        //     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        //     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        // },
        // .messageType{
        //     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        //     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        //     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        // },
        .messageSeverity{VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT},
        .messageType{VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT},
      },
    });

    auto physicalDevice = instance.CreatePhysicalDevice({
      .physicalDeviceFeatures{
        .features{
          .robustBufferAccess = VK_TRUE,
        },
      },
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
      .physicalDeviceRobustness2Features{
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
      },
      .physicalDeviceGlobalPriorityQueryFeatures{
        .globalPriorityQuery = VK_TRUE,
      },
    });

    Window::Init();
    VkSurfaceKHR surface;
    Window::InitSurface(instance.handle(), &surface);

    auto graphicsQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::Yes,
      .compute = Support::Yes,
      .transfer = Support::Yes,
      .globalPriority = Support::Yes,
      .present = surface,
    });
    auto computeQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::No,
      .compute = Support::Yes,
      .transfer = Support::Yes,
      .globalPriority = Support::Yes,
      .present = surface,
    });
    auto transferQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::No,
      .compute = Support::No,
      .transfer = Support::Yes,
    });
    auto globalQueue = Moxaic::IsCompositor() ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT;
    auto queuePriority = Moxaic::IsCompositor() ? 1.0f : 0.0f;

    auto logicalDevice = physicalDevice.CreateLogicalDevice({
      .createInfo{
        .pQueueCreateInfos{
          {
            .pNext = DeviceQueueGlobalPriorityCreateInfo{
              .globalPriority = globalQueue,
            },
            .queueFamilyIndex = graphicsQueueIndex,
            .pQueuePriorities{queuePriority},
          },
          {
            .pNext = DeviceQueueGlobalPriorityCreateInfo{
              .globalPriority = globalQueue,
            },
            .queueFamilyIndex = computeQueueIndex,
            .pQueuePriorities{queuePriority},
          }},
        .pEnabledExtensionNames{
          VK_KHR_SWAPCHAIN_EXTENSION_NAME,
          VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
          VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
          VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
          VK_EXT_MESH_SHADER_EXTENSION_NAME,
          VK_KHR_SPIRV_1_4_EXTENSION_NAME,
          VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
          VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
          VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
        },
      },
    });

    Queue graphicsQueue = logicalDevice.GetQueue({
      .debugName = "GraphicsQueue",
      .queueIndex = graphicsQueueIndex,
    });
    Queue computeQueue = logicalDevice.GetQueue({
      .debugName = "ComputeQueue",
      .queueIndex = computeQueueIndex,
    });

    constexpr VkFormat ColorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr VkFormat NormalBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr VkFormat DepthBufferFormat = VK_FORMAT_D32_SFLOAT;
    constexpr uint32_t ColorAttachmentIndex = 0;
    constexpr uint32_t NormalAttachmentIndex = 1;
    constexpr uint32_t DepthAttachmentIndex = 2;

    auto renderPass = logicalDevice.CreateRenderPass({
      .debugName = "StandardRenderPass",
      .createInfo = {
        .pAttachments{
          {.format = ColorBufferFormat},
          {.format = NormalBufferFormat},
          {.format = DepthBufferFormat},
        },
        .pSubpasses{
          {
            .pColorAttachments{
              {.attachment = ColorAttachmentIndex},
              {.attachment = NormalAttachmentIndex},
            },
            .pDepthStencilAttachment{
              {.attachment = DepthAttachmentIndex},
            },
          },
        },
      },
    });

    auto graphicsCommandPool = logicalDevice.CreateCommandPool({
      .debugName = "GraphicsCommandPool",
      .createInfo = CommandPoolCreateInfo{
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
      },
    });
    auto computeCommandPool = logicalDevice.CreateCommandPool({
      .debugName = "GraphicsCommandPool",
      .createInfo = CommandPoolCreateInfo{
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = computeQueueIndex,
      },
    });

    auto graphicsCommandBuffer = graphicsCommandPool.AllocateCommandBuffer({
      .debugName = "GraphicsCommandBuffer",
    });
    auto computeCommandBuffer = computeCommandPool.AllocateCommandBuffer({
      .debugName = "ComputeCommandBuffer",
    });

    auto queryPool = logicalDevice.CreateQueryPool({
      .debugName = "TimestampeQueryPool",
      .createInfo{
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
      },
    });

    auto descriptorPool = logicalDevice.CreateDescriptorPool({
      .debugName = "TimestampeQueryPool",
      .createInfo{
        .maxSets = 30,
        .pPoolSizes{
          {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 10,
          },
          {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 10,
          },
          {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 10,
          },
        },
      },
    });

    auto linearSampler = logicalDevice.CreateSampler({
      .debugName = "LinearSampler",
      .createInfo{
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
      },
    });
    auto nearestSampler = logicalDevice.CreateSampler({
      .debugName = "LinearSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
    });
    auto minSampler = logicalDevice.CreateSampler({
      .debugName = "LinearSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
      .pReductionModeCreateInfo = SamplerReductionModeCreateInfo{
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
      },
    });
    auto maxSampler = logicalDevice.CreateSampler({
      .debugName = "LinearSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
      .pReductionModeCreateInfo = SamplerReductionModeCreateInfo{
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX,
      },
    });


    MXC_LOG(sizeof(InstanceState));
    MXC_LOG(sizeof(PhysicalDeviceState));
    MXC_LOG(sizeof(QueryPoolState));

}
