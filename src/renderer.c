#include "renderer.h"
#include "globals.h"
#include "window.h"

#include <assert.h>
#include <stdio.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define VK_VERSION VK_MAKE_API_VERSION(0, 1, 3, 2)
#define COUNT(array) sizeof(array) / sizeof(array[0])
#define VK_ALLOC NULL
#define CHECK_RESULT(command)                                             \
  {                                                                       \
    VkResult result = command;                                            \
    if (__builtin_expect(result != VK_SUCCESS, 1)) {                      \
      printf("!!! Error! %s = %s\n\n", command, string_VkResult(result)); \
      return result;                                                      \
    }                                                                     \
  }
#define ASSERT_RESULT(command)                                            \
  {                                                                       \
    VkResult result = command;                                            \
    if (__builtin_expect(result != VK_SUCCESS, 1)) {                      \
      printf("!!! Error! %s = %s\n\n", command, string_VkResult(result)); \
      assert(0 && "Vulkan Handle Error!");                                \
    }                                                                     \
  }

#define PFN_LOAD(vkFunction)                                                                            \
  PFN_##vkFunction vkFunction = (PFN_##vkFunction)vkGetInstanceProcAddr(context.instance, #vkFunction); \
  assert(vkFunction != NULL && "Couldn't load " #vkFunction)

#define COLOR_BUFFER_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define NORMAL_BUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define DEPTH_BUFFER_FORMAT VK_FORMAT_D32_SFLOAT
#define COLOR_ATTACHMENT_INDEX 0
#define NORMAL_ATTACHMENT_INDEX 1
#define DEPTH_ATTACHMENT_INDEX 2
#define SWAP_COUNT 2

static struct {
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VkPhysicalDevice physicalDevice;

  VkDevice device;

  VkSwapchainKHR swapchain;
  VkImage        swapImages[SWAP_COUNT];
  VkImageView    swapImageViews[SWAP_COUNT];

  VkDescriptorPool descriptorPool;

  VkSampler nearestSampler;
  VkSampler linearSampler;
  VkSampler minSampler;
  VkSampler maxSampler;

  VkRenderPass    renderPass;
  VkCommandPool   graphicsCommandPool;
  VkCommandBuffer graphicsCommandBuffer;
  uint32_t        graphicsQueueFamilyIndex;
  VkQueue         graphicsQueue;

  VkCommandPool   computeCommandPool;
  VkCommandBuffer computeCommandBuffer;
  uint32_t        computeQueueFamilyIndex;
  VkQueue         computeQueue;

  uint32_t transferQueueFamilyIndex;
  VkQueue  transferQueue;

  VkQueryPool timeQueryPool;

} context;

struct Node_t {
};

static VkBool32 DebugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                        pUserData) {
  printf("%s\n", pCallbackData->pMessage);
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    assert(0 && "Validation Error! Exiting!");
  }
  return VK_FALSE;
}

typedef enum Support {
  Optional,
  Yes,
  No,
} Support;
const char* string_Support(const Support support) {
  switch (support) {
  case Optional:
    return "Optional";
  case Yes:
    return "Yes";
  case No:
    return "No";
  default:
    assert(0);
  }
}

static uint32_t FindQueueIndex(
    VkPhysicalDevice physicalDevice,
    Support          graphics,
    Support          compute,
    Support          transfer,
    Support          globalPriority,
    VkSurfaceKHR     presentSurface) {
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties2                 queueFamilyProperties[queueFamilyCount] = {};
  VkQueueFamilyGlobalPriorityPropertiesEXT queueFamilyGlobalPriorityProperties[queueFamilyCount] = {};
  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    queueFamilyGlobalPriorityProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT;
    queueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    queueFamilyProperties[i].pNext = &queueFamilyGlobalPriorityProperties[i];
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties);

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    char graphicsSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    char computeSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    char transferSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
    char globalPrioritySupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;

    VkBool32 presentSupport = VK_FALSE;
    if (presentSurface != NULL)
      vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, presentSurface, &presentSupport);
    if (graphics == Yes && !graphicsSupport)
      continue;
    if (graphics == No && graphicsSupport)
      continue;
    if (compute == Yes && !computeSupport)
      continue;
    if (compute == No && computeSupport)
      continue;
    if (transfer == Yes && !transferSupport)
      continue;
    if (transfer == No && transferSupport)
      continue;
    if (globalPriority == Yes && !globalPrioritySupport)
      continue;
    if (globalPriority == No && globalPrioritySupport)
      continue;
    if (presentSurface != NULL && !presentSupport)
      continue;

    return i;
  }

  printf("Can't find queue family! graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
         string_Support(graphics),
         string_Support(compute),
         string_Support(transfer),
         string_Support(globalPriority),
         presentSurface == NULL ? "No" : "Yes");
  assert(0 && "Failed to find Queue Family!");
}

int mxRendererInitContext() {
  { // Instance
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = 0;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT messageType = 0;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = DebugCallback,
    };
    const char* ppEnabledLayerNames[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const char* ppEnabledInstanceExtensionNames[] = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        WindowExtensionName,
    };
    VkInstanceCreateInfo instanceCreationInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugUtilsMessengerCreateInfo,
        .pApplicationInfo = &(VkApplicationInfo){
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Moxaic",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Vulkan",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_VERSION,
        },
        .enabledLayerCount = COUNT(ppEnabledLayerNames),
        .ppEnabledLayerNames = ppEnabledLayerNames,
        .enabledExtensionCount = COUNT(ppEnabledInstanceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
    };
    ASSERT_RESULT(vkCreateInstance(&instanceCreationInfo, VK_ALLOC, &context.instance));
    printf("Instance Vulkan API version %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }

  { // PhysicalDevice
    uint32_t deviceCount = 0;
    ASSERT_RESULT(vkEnumeratePhysicalDevices(context.instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    ASSERT_RESULT(vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices));
    printf("Choosing PhysicalDevice Index: %d\n", 0);
    context.physicalDevice = devices[0];
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    assert(physicalDeviceProperties.properties.apiVersion >= VK_VERSION && "Insufficinet Vulkan API Version");

    context.graphicsQueueFamilyIndex = FindQueueIndex(context.physicalDevice, Yes, Yes, Yes, Yes, context.surface);
    context.computeQueueFamilyIndex = FindQueueIndex(context.physicalDevice, No, Yes, Yes, Yes, context.surface);
    context.transferQueueFamilyIndex = FindQueueIndex(context.physicalDevice, No, No, Yes, Optional, NULL);
  }

  { // Surface
    ASSERT_RESULT(mxCreateSurface(context.instance, VK_ALLOC, &context.surface));
  }

  { // Device
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
        .pNext = NULL,
        .globalPriorityQuery = VK_TRUE,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = &physicalDeviceGlobalPriorityQueryFeatures,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
    };
    VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = &physicalDeviceRobustness2Features,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &physicalDeviceMeshShaderFeatures,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &physicalDeviceVulkan13Features,
        .samplerFilterMinmax = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
    };
    VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &physicalDeviceVulkan12Features,
    };
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &physicalDeviceVulkan11Features,
        .features = {
            .robustBufferAccess = VK_TRUE,
        }};
    const char* ppEnabledDeviceExtensionNames[] = {
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
        VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        ExternalMemoryExntensionName,
        ExternalSemaphoreExntensionName,
        ExternalFenceExntensionName,
    };
    VkDeviceQueueGlobalPriorityCreateInfoEXT deviceQueueGlobalPriorityCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
        .globalPriority = IsCompositor ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT,
    };
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = 3,
        .pQueueCreateInfos = (VkDeviceQueueCreateInfo[]){
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.graphicsQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){IsCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.computeQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){IsCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = context.transferQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){0},
            },
        },
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    ASSERT_RESULT(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VK_ALLOC, &context.device));

    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    assert(context.graphicsQueue != NULL && "graphicsQueue not found!");
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    assert(context.computeQueue != NULL && "computeQueue not found!");
    vkGetDeviceQueue(context.device, context.transferQueueFamilyIndex, 0, &context.transferQueue);
    assert(context.transferQueue != NULL && "transferQueue not found!");
  }

  // PFN_LOAD(vkSetDebugUtilsObjectNameEXT);

  { // RenderPass
#define VK_DEFAULT_ATTACHMENT_DESCRIPTION             \
  .samples = VK_SAMPLE_COUNT_1_BIT,                   \
  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              \
  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,            \
  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   \
  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, \
  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         \
  .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments = (VkAttachmentDescription[]){
            {.format = COLOR_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = NORMAL_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = DEPTH_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
        },
        .subpassCount = 1,
        .pSubpasses = &(VkSubpassDescription){
            .colorAttachmentCount = 2,
            .pColorAttachments = (VkAttachmentReference[]){
                {.attachment = COLOR_ATTACHMENT_INDEX, .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                {.attachment = NORMAL_ATTACHMENT_INDEX, .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
            },
            .pDepthStencilAttachment = &(VkAttachmentReference){
                .attachment = DEPTH_ATTACHMENT_INDEX,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            },
        },
    };
    ASSERT_RESULT(vkCreateRenderPass(context.device, &renderPassCreateInfo, VK_ALLOC, &context.renderPass));
  }

  { // Pools
    VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex,
    };
    ASSERT_RESULT(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &context.graphicsCommandPool));
    VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.computeQueueFamilyIndex,
    };
    ASSERT_RESULT(vkCreateCommandPool(context.device, &computeCommandPoolCreateInfo, VK_ALLOC, &context.computeCommandPool));

    VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.graphicsCommandPool,
        .commandBufferCount = 1,
    };
    ASSERT_RESULT(vkAllocateCommandBuffers(context.device, &graphicsCommandBufferAllocateInfo, &context.graphicsCommandBuffer));
    VkCommandBufferAllocateInfo computeCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.computeCommandPool,
        .commandBufferCount = 1,
    };
    ASSERT_RESULT(vkAllocateCommandBuffers(context.device, &computeCommandBufferAllocateInfo, &context.computeCommandBuffer));

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 30,
        .poolSizeCount = 3,
        .pPoolSizes = (VkDescriptorPoolSize[]){
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
        },
    };
    ASSERT_RESULT(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, VK_ALLOC, &context.descriptorPool));

    VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    ASSERT_RESULT(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VK_ALLOC, &context.timeQueryPool));
  }

  { // CommandBuffers

  }

  {
    VkSamplerCreateInfo nearestSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .maxLod = 16.0,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };
    ASSERT_RESULT(vkCreateSampler(context.device, &nearestSampleCreateInfo, VK_ALLOC, &context.nearestSampler));

#define DEFAULT_LINEAR_SAMPLER                           \
  .magFilter = VK_FILTER_LINEAR,                         \
  .minFilter = VK_FILTER_LINEAR,                         \
  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,           \
  .maxLod = 16.0,                                        \
  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK

    VkSamplerCreateInfo linearSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        DEFAULT_LINEAR_SAMPLER,
    };
    ASSERT_RESULT(vkCreateSampler(context.device, &linearSampleCreateInfo, VK_ALLOC, &context.linearSampler));

    VkSamplerCreateInfo minSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    ASSERT_RESULT(vkCreateSampler(context.device, &minSamplerCreateInfo, VK_ALLOC, &context.minSampler));

    VkSamplerCreateInfo maxSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    ASSERT_RESULT(vkCreateSampler(context.device, &maxSamplerCreateInfo, VK_ALLOC, &context.minSampler));
  }

  { // Swapchain
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.surface,
        .minImageCount = 2,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageExtent = {DEFAULT_WIDTH, DEFAULT_HEIGHT},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    ASSERT_RESULT(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, VK_ALLOC, &context.swapchain));


  }
}

int mxRenderInitNode() {
  struct Node_t node;
}