#include "moxaic_vulkan_device.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include "main.hpp"

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

#include <vector>
#include <array>

// From OVR Vulkan example. Is this better/same as vulkan tutorial!?
static bool MemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties &physicalDeviceMemoryProperties,
                                     const VkMemoryPropertyFlags &requiredMemoryProperties,
                                     uint32_t requiredMemoryTypeBits,
                                     uint32_t &outMemoryTypeBits)
{
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((requiredMemoryTypeBits & 1) == 1) {
            if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) ==
                requiredMemoryProperties) {
                outMemoryTypeBits = i;
                return true;
            }
        }
        requiredMemoryTypeBits >>= 1;
    }
    MXC_LOG_ERROR("Can't find memory type.", requiredMemoryProperties, requiredMemoryTypeBits);
    return false;
}

static bool ImageMemoryTypeFromProperties(const VkDevice &device,
                                          const VkImage &image,
                                          const VkPhysicalDeviceMemoryProperties &physicalDeviceMemoryProperties,
                                          const VkMemoryPropertyFlags properties,
                                          VkMemoryRequirements &outMemoryRequirements,
                                          uint32_t &outMemoryTypeBits)
{
    vkGetImageMemoryRequirements(device, image, &outMemoryRequirements);
    return MemoryTypeFromProperties(physicalDeviceMemoryProperties,
                                    properties,
                                    outMemoryRequirements.memoryTypeBits,
                                    outMemoryTypeBits);
}

static bool BufferMemoryTypeFromProperties(const VkDevice &device,
                                           const VkBuffer &buffer,
                                           const VkPhysicalDeviceMemoryProperties &physicalDeviceMemoryProperties,
                                           const VkMemoryPropertyFlags properties,
                                           VkMemoryRequirements *outMemoryRequirements,
                                           uint32_t &outMemoryTypeBits)
{
    vkGetBufferMemoryRequirements(device, buffer, outMemoryRequirements);
    return MemoryTypeFromProperties(physicalDeviceMemoryProperties,
                                    properties,
                                    outMemoryRequirements->memoryTypeBits,
                                    outMemoryTypeBits);
}

Moxaic::VulkanDevice::VulkanDevice(const VkInstance &instance,
                                   const VkSurfaceKHR &surface)
        : m_Instance(instance)
        , m_Surface(surface)
        , m_Device(VK_NULL_HANDLE)
        , m_PhysicalDevice(VK_NULL_HANDLE)
        , m_GraphicsQueue(VK_NULL_HANDLE)
        , m_GraphicsQueueFamilyIndex(-1)
        , m_ComputeQueue(VK_NULL_HANDLE)
        , m_ComputeQueueFamilyIndex(-1)
        , m_RenderPass(VK_NULL_HANDLE)
        , m_DescriptorPool(VK_NULL_HANDLE)
        , m_QueryPool(VK_NULL_HANDLE)
        , m_GraphicsCommandPool(VK_NULL_HANDLE)
        , m_ComputeCommandPool(VK_NULL_HANDLE)
        , m_GraphicsCommandBuffer(VK_NULL_HANDLE)
        , m_ComputeCommandBuffer(VK_NULL_HANDLE)
        , m_LinearSampler(VK_NULL_HANDLE)
        , m_NearestSampler(VK_NULL_HANDLE)
        , m_PhysicalDeviceMeshShaderProperties()
        , m_PhysicalDeviceProperties()
        , m_PhysicalDeviceMemoryProperties()
{}

Moxaic::VulkanDevice::~VulkanDevice() = default;

bool Moxaic::VulkanDevice::PickPhysicalDevice()
{
    MXC_LOG_FUNCTION();

    uint32_t deviceCount = 0;
    VK_CHK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));
    if (deviceCount == 0) {
        MXC_LOG_ERROR("Failed to find GPUs with Vulkan support!");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()));

    // Todo Implement Query OpenVR for the physical device to use
    m_PhysicalDevice = devices.front();

    m_PhysicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    m_PhysicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_PhysicalDeviceProperties.pNext = &m_PhysicalDeviceMeshShaderProperties;
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &m_PhysicalDeviceProperties);
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_PhysicalDeviceMemoryProperties);

    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.timestampPeriod);
    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[0]);
    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[1]);
    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[2]);
    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupInvocations);
    MXC_LOG_NAMED(m_PhysicalDeviceProperties.properties.limits.maxTessellationGenerationLevel);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxTaskPayloadSize);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxTaskPayloadAndSharedMemorySize);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxMeshOutputPrimitives);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxMeshOutputVertices);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxMeshWorkGroupInvocations);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxTaskWorkGroupInvocations);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxPreferredMeshWorkGroupInvocations);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.maxPreferredTaskWorkGroupInvocations);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.prefersLocalInvocationPrimitiveOutput);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.prefersLocalInvocationVertexOutput);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.prefersCompactPrimitiveOutput);
    MXC_LOG_NAMED(m_PhysicalDeviceMeshShaderProperties.prefersCompactVertexOutput);

    return true;
}

bool Moxaic::VulkanDevice::FindQueues()
{
    MXC_LOG_FUNCTION();

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        MXC_LOG_ERROR("Failed to get graphicsQueue properties.");
        return false;
    }
    std::vector<VkQueueFamilyGlobalPriorityPropertiesEXT> queueFamilyGlobalPriorityProperties(queueFamilyCount);
    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
    for (int i = 0; i < queueFamilyCount; ++i) {
        queueFamilyGlobalPriorityProperties[i] = {
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT
        };
        queueFamilies[i] = {
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
                .pNext = &queueFamilyGlobalPriorityProperties[i],
        };
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundCompute = false;

    for (int i = 0; i < queueFamilyCount; ++i) {
        bool globalQueueSupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;
        bool graphicsSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool computeSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);

        if (!foundGraphics && graphicsSupport && presentSupport) {
            if (!globalQueueSupport) {
                MXC_LOG_ERROR("No Global Priority On Graphics!");
            }
            m_GraphicsQueueFamilyIndex = i;
            foundGraphics = true;
            MXC_LOG_NAMED(m_GraphicsQueueFamilyIndex);
        }

        if (!foundCompute && computeSupport && presentSupport && !graphicsSupport) {
            if (!globalQueueSupport) {
                MXC_LOG_ERROR("No Global Priority On Compute!");
            }
            m_ComputeQueueFamilyIndex = i;
            foundCompute = true;
            MXC_LOG_NAMED(m_ComputeQueueFamilyIndex);
        }
    }

    if (!foundGraphics) {
        MXC_LOG_ERROR("Failed to find a graphicsQueue that supports both graphics and present!");
        return false;
    }

    if (!foundCompute) {
        MXC_LOG_ERROR("Failed to find a computeQueue!");
        return false;
    }

    return true;
}

bool Moxaic::VulkanDevice::CreateDevice()
{
    MXC_LOG_FUNCTION();

    const float queuePriority = Moxaic::IsCompositor() ?
                                1.0f :
                                0.0f;
    const VkDeviceQueueGlobalPriorityCreateInfoEXT queueGlobalPriorityCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
            .globalPriority = Moxaic::IsCompositor()
                              ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT
                              : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT
    };
    const std::array queueCreateInfos{
            (VkDeviceQueueCreateInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = &queueGlobalPriorityCreateInfo,
                    .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
            },
            (VkDeviceQueueCreateInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = &queueGlobalPriorityCreateInfo,
                    .queueFamilyIndex = m_ComputeQueueFamilyIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
            }
    };

    VkPhysicalDeviceMeshShaderFeaturesEXT supportedPhysicalDeviceMeshShaderFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
    };
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT supportedPhysicalDeviceGlobalPriorityQueryFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
            .pNext = &supportedPhysicalDeviceMeshShaderFeatures,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT supportedPhysicalDeviceRobustness2Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
            .pNext = &supportedPhysicalDeviceGlobalPriorityQueryFeatures
    };
    VkPhysicalDeviceVulkan13Features supportedPhysicalDeviceVulkan13Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &supportedPhysicalDeviceRobustness2Features
    };
    VkPhysicalDeviceVulkan12Features supportedPhysicalDeviceVulkan12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &supportedPhysicalDeviceVulkan13Features
    };
    VkPhysicalDeviceVulkan11Features supportedPhysicalDeviceVulkan11Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = &supportedPhysicalDeviceVulkan12Features
    };
    VkPhysicalDeviceFeatures2 supportedPhysicalDeviceFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &supportedPhysicalDeviceVulkan11Features,
    };
    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &supportedPhysicalDeviceFeatures2);
    if (!supportedPhysicalDeviceFeatures2.features.robustBufferAccess)
        MXC_LOG_ERROR("robustBufferAccess no support!");
    if (!supportedPhysicalDeviceFeatures2.features.samplerAnisotropy)
        MXC_LOG_ERROR("samplerAnisotropy no support!");
    if (!supportedPhysicalDeviceRobustness2Features.robustImageAccess2)
        MXC_LOG_ERROR("robustImageAccess2 no support!");
    if (!supportedPhysicalDeviceRobustness2Features.robustBufferAccess2)
        MXC_LOG_ERROR("robustBufferAccess2 no support!");
    if (!supportedPhysicalDeviceRobustness2Features.nullDescriptor)
        MXC_LOG_ERROR("nullDescriptor no support!");
    if (!supportedPhysicalDeviceVulkan13Features.synchronization2)
        MXC_LOG_ERROR("synchronization2 no support!");
    if (!supportedPhysicalDeviceVulkan13Features.robustImageAccess)
        MXC_LOG_ERROR("robustImageAccess no support!");
    if (!supportedPhysicalDeviceVulkan13Features.shaderDemoteToHelperInvocation)
        MXC_LOG_ERROR("shaderDemoteToHelperInvocation no support!");
    if (!supportedPhysicalDeviceVulkan13Features.shaderTerminateInvocation)
        MXC_LOG_ERROR("shaderTerminateInvocation no support!");
    if (!supportedPhysicalDeviceGlobalPriorityQueryFeatures.globalPriorityQuery)
        MXC_LOG_ERROR("globalPriorityQuery no support!");
    if (!supportedPhysicalDeviceMeshShaderFeatures.taskShader)
        MXC_LOG_ERROR("taskShader no support!");
    if (!supportedPhysicalDeviceMeshShaderFeatures.meshShader)
        MXC_LOG_ERROR("meshShader no support!");

    VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .taskShader = true,
            .meshShader = true,
    };
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
            .pNext = &physicalDeviceMeshShaderFeatures,
            .globalPriorityQuery = true,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
            .pNext = &physicalDeviceGlobalPriorityQueryFeatures,
            .robustBufferAccess2 = true,
            .robustImageAccess2 = true,
            .nullDescriptor = true,
    };
    VkPhysicalDeviceVulkan13Features enabledFeatures13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &physicalDeviceRobustness2Features,
//            .synchronization2 = true,
            .robustImageAccess = true,
            .shaderDemoteToHelperInvocation = true,
            .shaderTerminateInvocation = true,
    };
    VkPhysicalDeviceVulkan12Features enabledFeatures12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &enabledFeatures13,
            .imagelessFramebuffer = true,
            .hostQueryReset = true,
            .timelineSemaphore = true,
    };
    VkPhysicalDeviceVulkan11Features enabledFeatures11 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = &enabledFeatures12,
    };
    VkPhysicalDeviceFeatures2 enabledFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
            .pNext = &enabledFeatures11,
            .features = {
                    .robustBufferAccess = true,
                    .tessellationShader = true,
                    .fillModeNonSolid = true,
                    .samplerAnisotropy = true,
                    .vertexPipelineStoresAndAtomics = true,
                    .fragmentStoresAndAtomics = true,
            }
    };

    const std::array requiredDeviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            // Required by VK_KHR_spirv_1_4 - https://github.com/SaschaWillems/Vulkan/blob/master/examples/meshshader/meshshader.cpp
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
            VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
    };

    for (auto requiredDeviceExtension: requiredDeviceExtensions) {
        MXC_LOG_NAMED(requiredDeviceExtension);
    }

    VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &enabledFeatures
    };
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = requiredDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    VK_CHK(vkCreateDevice(m_PhysicalDevice, &createInfo, VK_ALLOC, &m_Device));

    return true;
}


bool Moxaic::VulkanDevice::CreateRenderPass()
{
    MXC_LOG_FUNCTION();

    // supposedly most correct https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present
    const std::array colorAttachments{
            (VkAttachmentReference) {
                    .attachment = 0,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            (VkAttachmentReference) {
                    .attachment = 1,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            (VkAttachmentReference) {
                    .attachment = 2,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
    };
    const VkAttachmentReference depthAttachmentReference = {
            .attachment = 3,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const std::array subpass = {
            (VkSubpassDescription) {
                    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                    .colorAttachmentCount = colorAttachments.size(),
                    .pColorAttachments = colorAttachments.data(),
                    .pDepthStencilAttachment = &depthAttachmentReference,
            }
    };
    const std::array dependencies{
            (VkSubpassDependency) {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask = VK_ACCESS_NONE_KHR,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = 0,
            },
            (VkSubpassDependency) {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .srcAccessMask = VK_ACCESS_NONE_KHR,
                    .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = 0,
            },
    };
    const std::array attachments{
            (VkAttachmentDescription) {
                    .flags = 0,
                    .format = MXC_COLOR_BUFFER_FORMAT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//                    .finalLayout = pVulkan->isChild ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
//                    .finalLayout = pVulkan->isChild ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            (VkAttachmentDescription) {
                    .flags = 0,
                    .format = MXC_NORMAL_BUFFER_FORMAT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            (VkAttachmentDescription) {
                    .flags = 0,
                    .format = MXC_G_BUFFER_FORMAT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            (VkAttachmentDescription) {
                    .flags = 0,
                    .format = MXC_DEPTH_BUFFER_FORMAT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
    };
    const VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = subpass.size(),
            .pSubpasses = subpass.data(),
            .dependencyCount = dependencies.size(),
            .pDependencies = dependencies.data(),
    };

    VK_CHK(vkCreateRenderPass(m_Device, &renderPassInfo, VK_ALLOC, &m_RenderPass));

    return true;
}

bool Moxaic::VulkanDevice::CreateCommandBuffers()
{
    MXC_LOG_FUNCTION();

    // Graphics + Compute
    const VkCommandPoolCreateInfo graphicsPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_Device,
                               &graphicsPoolInfo,
                               VK_ALLOC,
                               &m_GraphicsCommandPool));
    const VkCommandBufferAllocateInfo graphicsAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_GraphicsCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_Device,
                                    &graphicsAllocateInfo,
                                    &m_GraphicsCommandBuffer));

    // Compute
    const VkCommandPoolCreateInfo computePoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_ComputeQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_Device,
                               &computePoolInfo,
                               VK_ALLOC,
                               &m_ComputeCommandPool));
    const VkCommandBufferAllocateInfo computeAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_ComputeCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_Device,
                                    &computeAllocInfo,
                                    &m_ComputeCommandBuffer));

    return true;
}

bool Moxaic::VulkanDevice::CreatePools()
{
    MXC_LOG_FUNCTION();

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2,
            .pipelineStatistics = 0,
    };
    vkCreateQueryPool(m_Device, &queryPoolCreateInfo, VK_ALLOC, &m_QueryPool);

    const std::array poolSizes{
            (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 4,
            },
            (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    .descriptorCount = 4,
            },
            (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 4,
            }
    };
    const VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 12,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data(),
    };
    VK_CHK(vkCreateDescriptorPool(m_Device,
                                  &poolInfo,
                                  VK_ALLOC,
                                  &m_DescriptorPool));

    return true;
}

bool Moxaic::VulkanDevice::CreateSamplers()
{
    MXC_LOG_FUNCTION();

    const VkSamplerCreateInfo linearSamplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = m_PhysicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHK(vkCreateSampler(m_Device, &linearSamplerInfo, VK_ALLOC, &m_LinearSampler));

    const VkSamplerCreateInfo nearestSamplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = m_PhysicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHK(vkCreateSampler(m_Device, &nearestSamplerInfo, VK_ALLOC, &m_NearestSampler));

    return true;
}

bool Moxaic::VulkanDevice::Init()
{
    MXC_LOG_FUNCTION();

    if (!PickPhysicalDevice())
        return false;

    if (!FindQueues())
        return false;

    if (!CreateDevice())
        return false;

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_ComputeQueueFamilyIndex, 0, &m_ComputeQueue);

    if (!CreateRenderPass())
        return false;

    if (!CreateCommandBuffers())
        return false;

    if (!CreatePools())
        return false;

    if (!CreateSamplers())
        return false;

//    SDL_Vulkan_GetDrawableSize(window, &width, &height);

    return true;
}

bool Moxaic::VulkanDevice::AllocateMemory(const VkMemoryPropertyFlags &properties,
                                          const VkImage &image,
                                          VkDeviceMemory &outDeviceMemory) const
{
    VkMemoryRequirements memRequirements = {};
    uint32_t memTypeIndex;
    MXC_CHK(ImageMemoryTypeFromProperties(m_Device,
                                          image,
                                          m_PhysicalDeviceMemoryProperties,
                                          properties,
                                          memRequirements,
                                          memTypeIndex));

    const VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(m_Device,
                            &allocateInfo,
                            VK_ALLOC,
                            &outDeviceMemory));

    return true;
}

bool Moxaic::VulkanDevice::CreateImage(const VkImageCreateInfo &imageCreateInfo,
                                       VkImage &outImage) const
{
    VK_CHK(vkCreateImage(m_Device,
                         &imageCreateInfo,
                         VK_ALLOC,
                         &outImage));
    return true;
}
