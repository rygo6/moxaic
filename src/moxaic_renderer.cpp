#include "moxaic_renderer.hpp"
#include "mid_vulkan.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"

#include <windows.h>

#include <vulkan/vulkan_win32.h>


using namespace Mid::Vk;

#define ASSERT_HANDLE(handle) assert(handle.Result() == VK_SUCCESS && "#handle != VK_SUCCESS")

Instance instanceHandle;

void Moxaic::Renderer::Init()
{
    instance = Instance::Create({
      .debugName{"MainInstance"},
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
    ASSERT_HANDLE(instance);

    physicalDevice = instance.CreatePhysicalDevice({
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
    ASSERT_HANDLE(physicalDevice);

    surface = instance.CreateSurface({.debugName = "MainWindow"});
    Window::Init();
    Window::InitSurface(instance.handle(), surface.pHandle());
    ASSERT_HANDLE(surface);

    auto graphicsQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::Yes,
      .compute = Support::Yes,
      .transfer = Support::Yes,
      .globalPriority = Support::Yes,
      .present = surface.handle(),
    });
    auto computeQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::No,
      .compute = Support::Yes,
      .transfer = Support::Yes,
      .globalPriority = Support::Yes,
      .present = surface.handle(),
    });
    auto transferQueueIndex = physicalDevice.FindQueueIndex({
      .graphics = Support::No,
      .compute = Support::No,
      .transfer = Support::Yes,
    });
    auto globalQueue = Moxaic::IsCompositor() ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT;
    auto queuePriority = Moxaic::IsCompositor() ? 1.0f : 0.0f;

    logicalDevice = physicalDevice.CreateLogicalDevice({
      .createInfo{
        .pQueueCreateInfos{
          {
            .pNext = DeviceQueueGlobalPriorityCreateInfo{.globalPriority = globalQueue},
            .queueFamilyIndex = graphicsQueueIndex,
            .pQueuePriorities{queuePriority},
          },
          {
            .pNext = DeviceQueueGlobalPriorityCreateInfo{.globalPriority = globalQueue},
            .queueFamilyIndex = computeQueueIndex,
            .pQueuePriorities{queuePriority},
          },
          {
            .queueFamilyIndex = transferQueueIndex,
            .pQueuePriorities{0},
          },
        },
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
    ASSERT_HANDLE(logicalDevice);

    auto graphicsQueue = logicalDevice.GetQueue({
      .debugName = "GraphicsQueue",
      .queueIndex = graphicsQueueIndex,
    });
    ASSERT_HANDLE(graphicsQueue);
    auto computeQueue = logicalDevice.GetQueue({
      .debugName = "ComputeQueue",
      .queueIndex = computeQueueIndex,
    });
    ASSERT_HANDLE(computeQueue);
    auto transferQueue = logicalDevice.GetQueue({
      .debugName = "TransferQueue",
      .queueIndex = transferQueueIndex,
    });
    ASSERT_HANDLE(transferQueue);

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
    ASSERT_HANDLE(renderPass);

    auto graphicsCommandPool = logicalDevice.CreateCommandPool({
      .debugName = "GraphicsCommandPool",
      .createInfo = CommandPoolCreateInfo{
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
      },
    });
    ASSERT_HANDLE(graphicsCommandPool);
    auto computeCommandPool = logicalDevice.CreateCommandPool({
      .debugName = "GraphicsCommandPool",
      .createInfo = CommandPoolCreateInfo{
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = computeQueueIndex,
      },
    });
    ASSERT_HANDLE(computeCommandPool);

    auto graphicsCommandBuffer = graphicsCommandPool.AllocateCommandBuffer({
      .debugName = "GraphicsCommandBuffer",
    });
    ASSERT_HANDLE(graphicsCommandBuffer);
    auto computeCommandBuffer = computeCommandPool.AllocateCommandBuffer({
      .debugName = "ComputeCommandBuffer",
    });
    ASSERT_HANDLE(computeCommandBuffer);

    auto queryPool = logicalDevice.CreateQueryPool({
      .debugName = "TimestampeQueryPool",
      .createInfo{
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
      },
    });
    ASSERT_HANDLE(queryPool);

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
    ASSERT_HANDLE(descriptorPool);

    auto linearSampler = logicalDevice.CreateSampler({
      .debugName = "LinearSampler",
      .createInfo{
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
      },
    });
    ASSERT_HANDLE(linearSampler);
    auto nearestSampler = logicalDevice.CreateSampler({
      .debugName = "NearestSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
    });
    ASSERT_HANDLE(nearestSampler);
    auto minSampler = logicalDevice.CreateSampler({
      .debugName = "MinSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
      .pReductionModeCreateInfo = SamplerReductionModeCreateInfo{
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
      },
    });
    ASSERT_HANDLE(minSampler);
    auto maxSampler = logicalDevice.CreateSampler({
      .debugName = "MaxSampler",
      .createInfo{
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
      },
      .pReductionModeCreateInfo = SamplerReductionModeCreateInfo{
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX,
      },
    });
    ASSERT_HANDLE(maxSampler);

    auto checkerImage = logicalDevice.CreateImage({
      .debugName = "CheckerImage",
      .createInfo = ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {DefaultWidth, DefaultHeight, 1},
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      },
    });
    ASSERT_HANDLE(checkerImage);

    auto imageDeviceMemory = logicalDevice.AllocateMemory({
      .debugName = "CheckerImageMemory",
      .allocateInfo = MemoryAllocateInfo{
        .allocationSize = checkerImage.state().memoryRequirements.size,
        .memoryTypeIndex = checkerImage.state().memoryTypeIndex},
    });
    ASSERT_HANDLE(imageDeviceMemory);

    checkerImage.BindImage(imageDeviceMemory);
    ASSERT_HANDLE(checkerImage);

    auto checkerImageView = checkerImage.CreateImageView(ImageViewDesc{
      .debugName = "CheckerImageView",
    });
    ASSERT_HANDLE(checkerImageView);

    auto swapchain = logicalDevice.CreateSwapchain(SwapchainDesc{
      .createInfo{
        .surface = surface.handle(),
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageExtent = {DefaultWidth, DefaultHeight},
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      },
    });
    ASSERT_HANDLE(swapchain);


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
}
