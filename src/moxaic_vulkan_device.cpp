#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_swap.hpp"

#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"

#include "main.hpp"
#include "moxaic_vulkan_timeline_semaphore.hpp"

#include <vulkan/vulkan.h>
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

#include <vector>
#include <array>

// From OVR Vulkan example. Is this better/same as vulkan tutorial!?
static MXC_RESULT MemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties &physicalDeviceMemoryProperties,
                                           const VkMemoryPropertyFlags &requiredMemoryProperties,
                                           uint32_t requiredMemoryTypeBits,
                                           uint32_t &outMemTypeIndex)
{
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((requiredMemoryTypeBits & 1) == 1) {
            if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) ==
                requiredMemoryProperties) {
                outMemTypeIndex = i;
                return MXC_SUCCESS;
            }
        }
        requiredMemoryTypeBits >>= 1;
    }
    MXC_LOG_ERROR("Can't find memory type.", requiredMemoryProperties, requiredMemoryTypeBits);
    return MXC_FAIL;
}

Moxaic::VulkanDevice::VulkanDevice() = default;
Moxaic::VulkanDevice::~VulkanDevice() = default;

MXC_RESULT Moxaic::VulkanDevice::PickPhysicalDevice()
{
    MXC_LOG_FUNCTION();

    uint32_t deviceCount = 0;
    VK_CHK(vkEnumeratePhysicalDevices(vkInstance(), &deviceCount, nullptr));
    if (deviceCount == 0) {
        MXC_LOG_ERROR("Failed to find GPUs with Vulkan support!");
        return MXC_FAIL;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHK(vkEnumeratePhysicalDevices(vkInstance(), &deviceCount, devices.data()));

    // Todo Implement Query OpenVR for the physical vkDevice to use
    m_VkPhysicalDevice = devices.front();

    m_PhysicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    m_PhysicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_PhysicalDeviceProperties.pNext = &m_PhysicalDeviceMeshShaderProperties;
    vkGetPhysicalDeviceProperties2(m_VkPhysicalDevice, &m_PhysicalDeviceProperties);
    vkGetPhysicalDeviceMemoryProperties(m_VkPhysicalDevice, &m_PhysicalDeviceMemoryProperties);

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

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::FindQueues()
{
    MXC_LOG_FUNCTION();

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(m_VkPhysicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        MXC_LOG_ERROR("Failed to get graphicsQueue properties.");
        return MXC_FAIL;
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
    vkGetPhysicalDeviceQueueFamilyProperties2(m_VkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundCompute = false;

    for (int i = 0; i < queueFamilyCount; ++i) {
        bool globalQueueSupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;
        bool graphicsSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool computeSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_VkPhysicalDevice,
                                             i,
                                             vkSurface(),
                                             &presentSupport);

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
        return MXC_FAIL;
    }

    if (!foundCompute) {
        MXC_LOG_ERROR("Failed to find a computeQueue!");
        return MXC_FAIL;
    }

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::CreateDevice()
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
    vkGetPhysicalDeviceFeatures2(m_VkPhysicalDevice, &supportedPhysicalDeviceFeatures2);
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
    VK_CHK(vkCreateDevice(m_VkPhysicalDevice, &createInfo, VK_ALLOC, &m_VkDevice));

    return MXC_SUCCESS;
}


MXC_RESULT Moxaic::VulkanDevice::CreateRenderPass()
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
                    .format = k_ColorBufferFormat,
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
                    .format = k_NormalBufferFormat,
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
                    .format = k_GBufferFormat,
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
                    .format = k_DepthBufferFormat,
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

    VK_CHK(vkCreateRenderPass(m_VkDevice, &renderPassInfo, VK_ALLOC, &m_VkRenderPass));

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::CreateCommandBuffers()
{
    MXC_LOG_FUNCTION();

    // Graphics + Compute
    const VkCommandPoolCreateInfo graphicsPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_VkDevice,
                               &graphicsPoolInfo,
                               VK_ALLOC,
                               &m_VkGraphicsCommandPool));
    const VkCommandBufferAllocateInfo graphicsAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_VkGraphicsCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_VkDevice,
                                    &graphicsAllocateInfo,
                                    &m_VkGraphicsCommandBuffer));

    // Compute
    const VkCommandPoolCreateInfo computePoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_ComputeQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_VkDevice,
                               &computePoolInfo,
                               VK_ALLOC,
                               &m_VkComputeCommandPool));
    const VkCommandBufferAllocateInfo computeAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_VkComputeCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_VkDevice,
                                    &computeAllocInfo,
                                    &m_VkComputeCommandBuffer));

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::CreatePools()
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
    vkCreateQueryPool(m_VkDevice, &queryPoolCreateInfo, VK_ALLOC, &m_VkQueryPool);

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
    VK_CHK(vkCreateDescriptorPool(m_VkDevice,
                                  &poolInfo,
                                  VK_ALLOC,
                                  &m_VkDescriptorPool));

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::CreateSamplers()
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
    VK_CHK(vkCreateSampler(m_VkDevice, &linearSamplerInfo, VK_ALLOC, &m_VkLinearSampler));

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
    VK_CHK(vkCreateSampler(m_VkDevice, &nearestSamplerInfo, VK_ALLOC, &m_VkNearestSampler));

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::Init()
{
    MXC_LOG_FUNCTION();

    SDL_assert_always(vkInstance() != VK_NULL_HANDLE);
    SDL_assert_always(vkSurface() != VK_NULL_HANDLE);

    MXC_CHK(PickPhysicalDevice());
    MXC_CHK(FindQueues());
    MXC_CHK(CreateDevice());

    vkGetDeviceQueue(m_VkDevice, m_GraphicsQueueFamilyIndex, 0, &m_VkGraphicsQueue);
    vkGetDeviceQueue(m_VkDevice, m_ComputeQueueFamilyIndex, 0, &m_VkComputeQueue);

    MXC_CHK(CreateRenderPass());
    MXC_CHK(CreateCommandBuffers());
    MXC_CHK(CreatePools());
    MXC_CHK(CreateSamplers());

    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::AllocateMemory(const VkMemoryPropertyFlags &properties,
                                                const VkImage &image,
                                                const VkExternalMemoryHandleTypeFlags &externalHandleType,
                                                VkDeviceMemory &outDeviceMemory) const
{
    VkMemoryRequirements memRequirements = {};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(m_VkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     memTypeIndex));
    // todo your supposed check if it wants dedicated memory
//    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
//            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
//            .image = pTestTexture->image,
//            .buffer = VK_NULL_HANDLE,
//    };
    const VkExportMemoryAllocateInfo exportAllocInfo = {
            .sType =VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
//            .pNext =&dedicatedAllocInfo,
            .handleTypes = externalHandleType
    };
    const VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = externalHandleType == 0 ? nullptr : &exportAllocInfo,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(m_VkDevice,
                            &allocateInfo,
                            VK_ALLOC,
                            &outDeviceMemory));
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::AllocateMemory(const VkMemoryPropertyFlags &properties,
                                                const VkBuffer &buffer,
                                                const VkExternalMemoryHandleTypeFlags &externalHandleType,
                                                VkDeviceMemory &outDeviceMemory) const
{
    VkMemoryRequirements memRequirements = {};
    uint32_t memTypeIndex;
    vkGetBufferMemoryRequirements(m_VkDevice,
                                  buffer,
                                  &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     memTypeIndex));
    // dedicated needed??
//    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
//            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
//            .image = VK_NULL_HANDLE,
//            .buffer = *pRingBuffer,
//    };
    const VkExportMemoryWin32HandleInfoKHR exportMemoryWin32HandleInfo = {
            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            .pNext = nullptr,
            .pAttributes = nullptr,
            // This seems to not make the actual UBO read only, only the NT handle I presume
            .dwAccess = GENERIC_READ,
            .name = nullptr,
    };
    const VkExportMemoryAllocateInfo exportAllocInfo = {
            .sType =VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
            .pNext = &exportMemoryWin32HandleInfo,
            .handleTypes = externalHandleType
    };
    const VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = externalHandleType == 0 ? nullptr : &exportAllocInfo,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memTypeIndex
    };
    VK_CHK(vkAllocateMemory(m_VkDevice,
                            &allocInfo,
                            VK_ALLOC,
                            &outDeviceMemory));
    return MXC_SUCCESS;
}

// TODO implemented for unified graphics + transfer only right now
//  https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#transfer-dependencies
MXC_RESULT Moxaic::VulkanDevice::TransitionImageLayoutImmediate(VkImage image,
                                                                VkImageLayout oldLayout,
                                                                VkImageLayout newLayout,
                                                                VkAccessFlags srcAccessMask,
                                                                VkAccessFlags dstAccessMask,
                                                                VkPipelineStageFlags srcStageMask,
                                                                VkPipelineStageFlags dstStageMask,
                                                                VkImageAspectFlags aspectMask) const
{

    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(commandBuffer));
    const VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };
    vkCmdPipelineBarrier(commandBuffer,
                         srcStageMask,
                         dstStageMask,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &imageMemoryBarrier);
    MXC_CHK(EndImmediateCommandBuffer(commandBuffer));
    return MXC_SUCCESS;
}

// TODO make use transfer queue
MXC_RESULT Moxaic::VulkanDevice::BeginImmediateCommandBuffer(VkCommandBuffer &outCommandBuffer) const
{
    const VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_VkGraphicsCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_VkDevice,
                                    &allocInfo,
                                    &outCommandBuffer));
    const VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
    };
    VK_CHK(vkBeginCommandBuffer(outCommandBuffer,
                                &beginInfo));
    return MXC_SUCCESS;
}

// TODO make use transfer queue
MXC_RESULT Moxaic::VulkanDevice::EndImmediateCommandBuffer(const VkCommandBuffer &commandBuffer) const
{
    VK_CHK(vkEndCommandBuffer(commandBuffer));
    const VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
    };
    VK_CHK(vkQueueSubmit(m_VkGraphicsQueue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    VK_CHK(vkQueueWaitIdle(m_VkGraphicsQueue));
    vkFreeCommandBuffers(m_VkDevice,
                         m_VkGraphicsCommandPool,
                         1,
                         &commandBuffer);
    return MXC_SUCCESS;
}
MXC_RESULT Moxaic::VulkanDevice::BeginGraphicsCommandBuffer() const
{
    VK_CHK(vkResetCommandBuffer(m_VkGraphicsCommandBuffer,
                                VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
    const VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
    };
    VK_CHK(vkBeginCommandBuffer(m_VkGraphicsCommandBuffer,
                                &beginInfo));
    const VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) g_WindowDimensions.width,
            .height = (float) g_WindowDimensions.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };
    vkCmdSetViewport(m_VkGraphicsCommandBuffer,
                     0,
                     1,
                     &viewport);
    const VkRect2D scissor = {
            .offset = {0, 0},
            .extent = g_WindowDimensions,
    };
    vkCmdSetScissor(m_VkGraphicsCommandBuffer,
                    0,
                    1,
                    &scissor);
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::EndGraphicsCommandBuffer() const
{
    VK_CHK(vkEndCommandBuffer(m_VkGraphicsCommandBuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::BeginRenderPass(const VulkanFramebuffer &framebuffer) const
{
    std::array<VkClearValue, 4> clearValues;
    clearValues[0].color = (VkClearColorValue) {{0.1f, 0.2f, 0.3f, 0.0f}};
    clearValues[1].color = (VkClearColorValue) {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[2].color = (VkClearColorValue) {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].depthStencil = (VkClearDepthStencilValue) {1.0f, 0};
    const VkRenderPassBeginInfo vkRenderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = m_VkRenderPass,
            .framebuffer = framebuffer.vkFramebuffer(),
            .renderArea = {
                    .offset = {0, 0},
                    .extent = framebuffer.dimensions(),
            },
            .clearValueCount = clearValues.size(),
            .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(m_VkGraphicsCommandBuffer,
                         &vkRenderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::EndRenderPass() const
{
    vkCmdEndRenderPass(m_VkGraphicsCommandBuffer);
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanDevice::SubmitGraphicsQueueAndPresent(VulkanTimelineSemaphore &timelineSemaphore, const VulkanSwap &swap) const
{
    // https://www.khronos.org/blog/vulkan-timeline-semaphores
    const uint64_t waitValue = timelineSemaphore.waitValue();
    timelineSemaphore.IncrementWaitValue();
    const uint64_t signalValue = timelineSemaphore.waitValue();
    const uint64_t pWaitSemaphoreValues[] = {
            waitValue,
            0
    };
    const uint64_t pSignalSemaphoreValues[] = {
            signalValue,
            0
    };
    const VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreValueCount = 2,
            .pWaitSemaphoreValues =  pWaitSemaphoreValues,
            .signalSemaphoreValueCount = 2,
            .pSignalSemaphoreValues = pSignalSemaphoreValues,
    };
    const VkSemaphore pWaitSemaphores[] = {
            timelineSemaphore.vkSemaphore(),
            swap.acquireCompleteSemaphore(),
    };
    const VkPipelineStageFlags pWaitDstStageMask[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    const VkSemaphore pSignalSemaphores[] = {
            timelineSemaphore.vkSemaphore(),
            swap.renderCompleteSemaphore(),
    };
    const VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineSemaphoreSubmitInfo,
            .waitSemaphoreCount = 2,
            .pWaitSemaphores = pWaitSemaphores,
            .pWaitDstStageMask = pWaitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_VkGraphicsCommandBuffer,
            .signalSemaphoreCount = 2,
            .pSignalSemaphores = pSignalSemaphores
    };
    VK_CHK(vkQueueSubmit(m_VkGraphicsQueue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    // TODO want to use this?? https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_present_id.html
    auto renderCompleteSemaphore = swap.renderCompleteSemaphore();
    auto vkSwapchain = swap.vkSwapchain();
    auto blitIndex = swap.blitIndex();
    const VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderCompleteSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &vkSwapchain,
            .pImageIndices = &blitIndex,
    };
    VK_CHK(vkQueuePresentKHR(m_VkGraphicsQueue,
                             &presentInfo));
    return MXC_SUCCESS;
}

