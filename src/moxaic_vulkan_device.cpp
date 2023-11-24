#include "moxaic_vulkan_device.hpp"
#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_swap.hpp"
#include "moxaic_window.hpp"
#include "static_array.hpp"

#include <vector>
#include <vulkan/vulkan.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define MXC_EXTERNAL_MEMORY_HANDLE VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

// From OVR Vulkan example. Is this better/same as vulkan tutorial!?
static MXC_RESULT MemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties const& physicalDeviceMemoryProperties,
                                           VkMemoryPropertyFlags const& requiredMemoryProperties,
                                           uint32_t requiredMemoryTypeBits,
                                           uint32_t* pMemTypeIndex)
{
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((requiredMemoryTypeBits & 1) == 1) {
            if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) ==
                requiredMemoryProperties) {
                *pMemTypeIndex = i;
                return MXC_SUCCESS;
            }
        }
        requiredMemoryTypeBits >>= 1;
    }
    MXC_LOG_ERROR("Can't find memory type.", requiredMemoryProperties, requiredMemoryTypeBits);
    return MXC_FAIL;
}

Device::~Device(){
  // todo should I even worry about cleaning up refs yet?
  // maybe when I deal with recreating losts device
};

MXC_RESULT Device::PickPhysicalDevice()
{
    MXC_LOG_FUNCTION();

    uint32_t deviceCount = 0;
    VK_CHK(vkEnumeratePhysicalDevices(Vulkan::vkInstance(), &deviceCount, nullptr));
    if (deviceCount == 0) {
        MXC_LOG_ERROR("Failed to find GPUs with Vulkan support!");
        return MXC_FAIL;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHK(vkEnumeratePhysicalDevices(Vulkan::vkInstance(), &deviceCount, devices.data()));

    // Todo Implement Query OpenVR for the physical.GetVkDevice() to use
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

MXC_RESULT Device::FindQueues()
{
    MXC_LOG("Vulkan::Device::FindQueues");

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
          .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT};
        queueFamilies[i] = {
          .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
          .pNext = &queueFamilyGlobalPriorityProperties[i],
        };
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(m_VkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundCompute = false;

    for (int i = 0; i < queueFamilyCount; ++i) {
        bool const globalQueueSupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;
        bool const graphicsSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool const computeSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_VkPhysicalDevice,
                                             i,
                                             Vulkan::vkSurface(),
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

MXC_RESULT Device::CreateDevice()
{
    MXC_LOG("Vulkan::Device::CreateDevice");

    float const queuePriority = Moxaic::IsCompositor() ? 1.0f : 0.0f;
    VkDeviceQueueGlobalPriorityCreateInfoEXT const queueGlobalPriorityCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
      .globalPriority = Moxaic::IsCompositor() ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT};
    StaticArray const queueCreateInfos{
      (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = &queueGlobalPriorityCreateInfo,
        .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority},
      (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = &queueGlobalPriorityCreateInfo,
        .queueFamilyIndex = m_ComputeQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority}};

    VkPhysicalDeviceMeshShaderFeaturesEXT supportedPhysicalDeviceMeshShaderFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
    };
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT supportedPhysicalDeviceGlobalPriorityQueryFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
      .pNext = &supportedPhysicalDeviceMeshShaderFeatures,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT supportedPhysicalDeviceRobustness2Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
      .pNext = &supportedPhysicalDeviceGlobalPriorityQueryFeatures};
    VkPhysicalDeviceVulkan13Features supportedPhysicalDeviceVulkan13Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &supportedPhysicalDeviceRobustness2Features};
    VkPhysicalDeviceVulkan12Features supportedPhysicalDeviceVulkan12Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &supportedPhysicalDeviceVulkan13Features};
    VkPhysicalDeviceVulkan11Features supportedPhysicalDeviceVulkan11Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &supportedPhysicalDeviceVulkan12Features};
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
      }};

    constexpr StaticArray requiredDeviceExtensions{
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
      .pNext = &enabledFeatures};
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = requiredDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    VK_CHK(vkCreateDevice(m_VkPhysicalDevice, &createInfo, VK_ALLOC, &m_VkDevice));

    return MXC_SUCCESS;
}


MXC_RESULT Device::CreateRenderPass()
{
    MXC_LOG("Vulkan::Device::CreateRenderPass");
    // supposedly most correct https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present
    constexpr StaticArray colorAttachments{
      (VkAttachmentReference){
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
      (VkAttachmentReference){
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
      (VkAttachmentReference){
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      }};
    constexpr VkAttachmentReference depthAttachmentReference = {
      .attachment = 3,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    StaticArray const subpass{
      (VkSubpassDescription){
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
        .pDepthStencilAttachment = &depthAttachmentReference,
      }};
    constexpr StaticArray dependencies{
      (VkSubpassDependency){
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE_KHR,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0,
      },
      (VkSubpassDependency){
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_NONE_KHR,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0,
      },
    };
    constexpr StaticArray attachments{
      (VkAttachmentDescription){
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
      (VkAttachmentDescription){
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
      (VkAttachmentDescription){
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
      (VkAttachmentDescription){
        .flags = 0,
        .format = k_DepthBufferFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      }};
    VkRenderPassCreateInfo const renderPassInfo = {
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

MXC_RESULT Device::CreateCommandBuffers()
{
    MXC_LOG("Vulkan::Device::CreateCommandBuffers");
    // Graphics + Compute
    VkCommandPoolCreateInfo const graphicsPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_VkDevice,
                               &graphicsPoolInfo,
                               VK_ALLOC,
                               &m_VkGraphicsCommandPool));
    VkCommandBufferAllocateInfo const graphicsAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_VkGraphicsCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_VkDevice,
                                    &graphicsAllocateInfo,
                                    &m_VkGraphicsCommandBuffer));

    // Compute
    VkCommandPoolCreateInfo const computePoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = m_ComputeQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(m_VkDevice,
                               &computePoolInfo,
                               VK_ALLOC,
                               &m_VkComputeCommandPool));
    VkCommandBufferAllocateInfo const computeAllocInfo = {
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

MXC_RESULT Device::CreatePools()
{
    MXC_LOG("Vulkan::Device::CreatePools");
    constexpr VkQueryPoolCreateInfo queryPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = Device::QueryPoolCount,
      .pipelineStatistics = 0,
    };
    vkCreateQueryPool(m_VkDevice, &queryPoolCreateInfo, VK_ALLOC, &m_VkQueryPool);
    constexpr StaticArray poolSizes{
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 4,
      },
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 4,
      },
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 4,
      }};
    VkDescriptorPoolCreateInfo const poolInfo{
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

MXC_RESULT Device::CreateSamplers()
{
    MXC_LOG("Vulkan::Device::CreateSamplers");
    VkSamplerCreateInfo const linearSamplerInfo{
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

    VkSamplerCreateInfo const nearestSamplerInfo{
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

MXC_RESULT Device::Init()
{
    MXC_LOG("Vulkan::Device::Init");

    SDL_assert(Vulkan::vkInstance() != VK_NULL_HANDLE);
    SDL_assert(Vulkan::vkSurface() != VK_NULL_HANDLE);

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

MXC_RESULT Device::AllocateBindImageExport(VkMemoryPropertyFlags const& properties,
                                           VkImage const& image,
                                           VkExternalMemoryHandleTypeFlags const& externalHandleType,
                                           VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(m_VkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    // todo your supposed check if it wants dedicated memory
    //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
    //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    //            .image = pTestTexture->image,
    //            .buffer = VK_NULL_HANDLE,
    //    };
    VkExportMemoryAllocateInfo const exportAllocInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      //            .pNext =&dedicatedAllocInfo,
      .handleTypes = externalHandleType};
    VkMemoryAllocateInfo const allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &exportAllocInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(m_VkDevice,
                            &allocateInfo,
                            VK_ALLOC,
                            pDeviceMemory));
    VK_CHK(vkBindImageMemory(m_VkDevice,
                             image,
                             *pDeviceMemory,
                             0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::AllocateBindImageImport(VkMemoryPropertyFlags const& properties,
                                           VkImage const& image,
                                           VkExternalMemoryHandleTypeFlagBits const& externalHandleType,
                                           HANDLE const& externalHandle,
                                           VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(m_VkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    // todo your supposed check if it wants dedicated memory
    //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
    //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    //            .image = pTestTexture->image,
    //            .buffer = VK_NULL_HANDLE,
    //    };
    VkImportMemoryWin32HandleInfoKHR const importMemoryInfo{
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      //            .pNext = &dedicatedAllocInfo,
      .handleType = externalHandleType,
      .handle = externalHandle,
    };
    VkMemoryAllocateInfo const allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &importMemoryInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(
      m_VkDevice,
      &allocateInfo,
      VK_ALLOC,
      pDeviceMemory));
    VK_CHK(vkBindImageMemory(
      m_VkDevice,
      image,
      *pDeviceMemory,
      0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::AllocateBindImage(VkMemoryPropertyFlags const& properties,
                                     VkImage const& image,
                                     VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(m_VkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    VkMemoryAllocateInfo const allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(
      m_VkDevice,
      &allocateInfo,
      VK_ALLOC,
      pDeviceMemory));
    VK_CHK(vkBindImageMemory(
      m_VkDevice,
      image,
      *pDeviceMemory,
      0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateAllocateBindBuffer(VkBufferUsageFlags const& usage,
                                            VkMemoryPropertyFlags const& properties,
                                            VkDeviceSize const& bufferSize,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory) const
{
    return CreateAllocateBindBuffer(usage,
                                    properties,
                                    bufferSize,
                                    Vulkan::Locality::Local,
                                    pBuffer,
                                    pDeviceMemory,
                                    nullptr);
}

MXC_RESULT Device::CreateAllocateBindBuffer(VkBufferUsageFlags const& usage,
                                            VkMemoryPropertyFlags const& properties,
                                            VkDeviceSize const& bufferSize,
                                            Vulkan::Locality const& locality,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory,
                                            HANDLE* pExternalMemory) const
{
    constexpr VkExternalMemoryBufferCreateInfoKHR externalBufferInfo{
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR,
      .pNext = nullptr,
      .handleTypes = MXC_EXTERNAL_MEMORY_HANDLE};
    VkBufferCreateInfo const bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = locality == Locality::Local ? nullptr : &externalBufferInfo,
      .flags = 0,
      .size = bufferSize,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
    };
    VK_CHK(vkCreateBuffer(m_VkDevice,
                          &bufferCreateInfo,
                          nullptr,
                          pBuffer));

    VkMemoryRequirements memRequirements = {};
    uint32_t memTypeIndex;
    vkGetBufferMemoryRequirements(m_VkDevice,
                                  *pBuffer,
                                  &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(m_PhysicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));

    // dedicated needed??
    //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
    //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    //            .image = VK_NULL_HANDLE,
    //            .buffer = *pRingBuffer,
    //    };
    constexpr VkExportMemoryWin32HandleInfoKHR exportMemoryWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .pAttributes = nullptr,
      // This seems to not make the actual UBO read only, only the NT handle I presume
      .dwAccess = GENERIC_READ,
      .name = nullptr,
    };
    VkExportMemoryAllocateInfo const exportAllocInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = &exportMemoryWin32HandleInfo,
      .handleTypes = MXC_EXTERNAL_MEMORY_HANDLE};
    VkMemoryAllocateInfo const allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = locality == Locality::Local ? nullptr : &exportAllocInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex};
    VK_CHK(vkAllocateMemory(m_VkDevice,
                            &allocInfo,
                            VK_ALLOC,
                            pDeviceMemory));
    VK_CHK(vkBindBufferMemory(m_VkDevice,
                              *pBuffer,
                              *pDeviceMemory,
                              0));

    if (locality == Locality::External) {
#if WIN32
        VkMemoryGetWin32HandleInfoKHR const getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .memory = *pDeviceMemory,
          .handleType = MXC_EXTERNAL_MEMORY_HANDLE};
        VK_CHK(Vulkan::VkFunc.GetMemoryWin32HandleKHR(m_VkDevice,
                                                      &getWin32HandleInfo,
                                                      pExternalMemory));
#endif
    }

    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateStagingBuffer(void const* srcData,
                                       VkDeviceSize const& bufferSize,
                                       VkBuffer* pStagingBuffer,
                                       VkDeviceMemory* pStagingBufferMemory) const
{
    MXC_CHK(CreateAllocateBindBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     bufferSize,
                                     pStagingBuffer,
                                     pStagingBufferMemory));
    void* dstData;
    vkMapMemory(m_VkDevice, *pStagingBufferMemory, 0, bufferSize, 0, &dstData);
    memcpy(dstData, srcData, bufferSize);
    vkUnmapMemory(m_VkDevice, *pStagingBufferMemory);
    return MXC_SUCCESS;
}

MXC_RESULT Device::CopyBufferToBuffer(VkDeviceSize const& bufferSize,
                                      VkBuffer const& srcBuffer,
                                      VkBuffer const& dstBuffer) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    VkBufferCopy const copyRegion = {
      .srcOffset = 0,
      .dstOffset = 0,
      .size = bufferSize,
    };
    vkCmdCopyBuffer(commandBuffer,
                    srcBuffer,
                    dstBuffer,
                    1,
                    &copyRegion);
    MXC_CHK(EndImmediateCommandBuffer(commandBuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CopyBufferToImage(VkExtent2D const& imageExtent,
                                     VkBuffer const& srcBuffer,
                                     VkImage const& dstImage) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    VkBufferImageCopy const region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .imageOffset = {0, 0, 0},
      .imageExtent = {
        imageExtent.width,
        imageExtent.height,
        1},
    };
    vkCmdCopyBufferToImage(
      commandBuffer,
      srcBuffer,
      dstImage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region);
    MXC_CHK(EndImmediateCommandBuffer(commandBuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateAllocateBindPopulateBufferViaStaging(void const* srcData,
                                                              VkBufferUsageFlagBits const& usage,
                                                              VkDeviceSize const& bufferSize,
                                                              VkBuffer* pBuffer,
                                                              VkDeviceMemory* pBufferMemory) const
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    MXC_CHK(CreateStagingBuffer(srcData,
                                bufferSize,
                                &stagingBuffer,
                                &stagingBufferMemory));
    MXC_CHK(CreateAllocateBindBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     bufferSize,
                                     pBuffer,
                                     pBufferMemory));
    CopyBufferToBuffer(bufferSize,
                       stagingBuffer,
                       *pBuffer);
    vkDestroyBuffer(m_VkDevice,
                    stagingBuffer,
                    VK_ALLOC);
    vkFreeMemory(m_VkDevice,
                 stagingBufferMemory,
                 VK_ALLOC);
    return MXC_SUCCESS;
}

// TODO implemented for unified graphics + transfer only right now
//  https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#transfer-dependencies
MXC_RESULT Device::TransitionImageLayoutImmediate(VkImage const& image,
                                                  VkImageLayout const& oldLayout,
                                                  VkImageLayout const& newLayout,
                                                  VkAccessFlags const& srcAccessMask,
                                                  VkAccessFlags const& dstAccessMask,
                                                  VkPipelineStageFlags const& srcStageMask,
                                                  VkPipelineStageFlags const& dstStageMask,
                                                  VkImageAspectFlags const& aspectMask) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    VkImageMemoryBarrier const imageMemoryBarrier{
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
        .layerCount = 1}};
    vkCmdPipelineBarrier(commandBuffer,
                         srcStageMask,
                         dstStageMask,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
    MXC_CHK(EndImmediateCommandBuffer(commandBuffer));
    return MXC_SUCCESS;
}

// TODO make use transfer queue
MXC_RESULT Device::BeginImmediateCommandBuffer(VkCommandBuffer* pCommandBuffer) const
{
    VkCommandBufferAllocateInfo const allocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = m_VkGraphicsCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(m_VkDevice,
                                    &allocInfo,
                                    pCommandBuffer));
    constexpr VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
    };
    VK_CHK(vkBeginCommandBuffer(*pCommandBuffer,
                                &beginInfo));
    return MXC_SUCCESS;
}

// TODO make use transfer queue
MXC_RESULT Device::EndImmediateCommandBuffer(VkCommandBuffer const& commandBuffer) const
{
    VK_CHK(vkEndCommandBuffer(commandBuffer));
    VkSubmitInfo const submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = nullptr};
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

MXC_RESULT Device::BeginGraphicsCommandBuffer() const
{
    VK_CHK(vkResetCommandBuffer(m_VkGraphicsCommandBuffer,
                                VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
    constexpr VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VK_CHK(vkBeginCommandBuffer(m_VkGraphicsCommandBuffer, &beginInfo));
    VkViewport const viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(Window::extents().width),
      .height = static_cast<float>(Window::extents().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetViewport(m_VkGraphicsCommandBuffer,
                     0,
                     1,
                     &viewport);
    VkRect2D const scissor{
      .offset = {0, 0},
      .extent = Window::extents(),
    };
    vkCmdSetScissor(m_VkGraphicsCommandBuffer,
                    0,
                    1,
                    &scissor);
    return MXC_SUCCESS;
}

MXC_RESULT Device::EndGraphicsCommandBuffer() const
{
    VK_CHK(vkEndCommandBuffer(m_VkGraphicsCommandBuffer));
    return MXC_SUCCESS;
}

void Device::BeginRenderPass(Framebuffer const& framebuffer, VkClearColorValue const& backgroundColor) const
{
    StaticArray<VkClearValue, 4> clearValues;
    clearValues[0].color = backgroundColor;
    clearValues[1].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[2].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    VkRenderPassBeginInfo const renderPassBeginInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = m_VkRenderPass,
      .framebuffer = framebuffer.vkFramebuffer(),
      .renderArea = {
        .offset = {0, 0},
        .extent = framebuffer.extents(),
      },
      .clearValueCount = clearValues.size(),
      .pClearValues = clearValues.data(),
    };
    VK_CHK_VOID(vkCmdBeginRenderPass(m_VkGraphicsCommandBuffer,
                                     &renderPassBeginInfo,
                                     VK_SUBPASS_CONTENTS_INLINE));
}

void Device::EndRenderPass() const
{
    VK_CHK_VOID(vkCmdEndRenderPass(m_VkGraphicsCommandBuffer));
}

MXC_RESULT Device::SubmitGraphicsQueue(Semaphore* pTimelineSemaphore) const
{
    // https://www.khronos.org/blog/vulkan-timeline-semaphores
    uint64_t const waitValue = pTimelineSemaphore->GetLocalWaitValue();
    pTimelineSemaphore->IncrementWaitValue();
    uint64_t const signalValue = pTimelineSemaphore->GetLocalWaitValue();
    StaticArray const waitSemaphoreValues{
      waitValue,
    };
    StaticArray const signalSemaphoreValues{
      signalValue,
    };
    VkTimelineSemaphoreSubmitInfo const timelineSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount = waitSemaphoreValues.size(),
      .pWaitSemaphoreValues = waitSemaphoreValues.data(),
      .signalSemaphoreValueCount = signalSemaphoreValues.size(),
      .pSignalSemaphoreValues = signalSemaphoreValues.data(),
    };
    StaticArray const waitSemaphores{
      pTimelineSemaphore->GetVkSemaphore(),
    };
    constexpr StaticArray waitDstStageMask{
      (VkPipelineStageFlags) VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    StaticArray const signalSemaphores{
      pTimelineSemaphore->GetVkSemaphore(),
    };
    VkSubmitInfo const submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSemaphoreSubmitInfo,
      .waitSemaphoreCount = waitSemaphores.size(),
      .pWaitSemaphores = waitSemaphores.data(),
      .pWaitDstStageMask = waitDstStageMask.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &m_VkGraphicsCommandBuffer,
      .signalSemaphoreCount = signalSemaphores.size(),
      .pSignalSemaphores = signalSemaphores.data()};
    VK_CHK(vkQueueSubmit(m_VkGraphicsQueue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    return MXC_SUCCESS;
}

void Device::ResetTimestamps() const
{
    vkResetQueryPool(m_VkDevice, m_VkQueryPool, 0, QueryPoolCount);
}

void Device::WriteTimestamp(VkPipelineStageFlagBits const& pipelineStage, uint32_t const& query) const
{
    vkCmdWriteTimestamp(m_VkGraphicsCommandBuffer, pipelineStage, m_VkQueryPool, query);
}

StaticArray<double, Device::QueryPoolCount> Device::GetTimestamps() const
{
    uint64_t timestampsNS[QueryPoolCount];
    vkGetQueryPoolResults(m_VkDevice,
                      m_VkQueryPool,
                      0,
                      QueryPoolCount,
                      sizeof(uint64_t) * QueryPoolCount,
                      timestampsNS,
                      sizeof(uint64_t),
                      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    StaticArray<double, QueryPoolCount> timestampsMS;
    for (int i = 0; i < QueryPoolCount; ++i) {
        timestampsMS[i] = double(timestampsNS[i]) / double(1000000); // ns to ms
    }
    return timestampsMS;
}

MXC_RESULT Device::SubmitGraphicsQueueAndPresent(Semaphore& timelineSemaphore,
                                                 Swap const& swap) const
{
    // https://www.khronos.org/blog/vulkan-timeline-semaphores
    uint64_t const waitValue = timelineSemaphore.GetLocalWaitValue();
    timelineSemaphore.IncrementWaitValue();
    uint64_t const signalValue = timelineSemaphore.GetLocalWaitValue();
    StaticArray const waitSemaphoreValues{
      (uint64_t) waitValue,
      (uint64_t) 0};
    StaticArray const signalSemaphoreValues{
      (uint64_t) signalValue,
      (uint64_t) 0};
    VkTimelineSemaphoreSubmitInfo const timelineSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount = waitSemaphoreValues.size(),
      .pWaitSemaphoreValues = waitSemaphoreValues.data(),
      .signalSemaphoreValueCount = signalSemaphoreValues.size(),
      .pSignalSemaphoreValues = signalSemaphoreValues.data(),
    };
    StaticArray const waitSemaphores{
      timelineSemaphore.GetVkSemaphore(),
      swap.GetVkAcquireCompleteSemaphore(),
    };
    constexpr StaticArray waitDstStageMask{
      (VkPipelineStageFlags) VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      (VkPipelineStageFlags) VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    StaticArray const signalSemaphores{
      timelineSemaphore.GetVkSemaphore(),
      swap.GetVkRenderCompleteSemaphore(),
    };
    VkSubmitInfo const submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSemaphoreSubmitInfo,
      .waitSemaphoreCount = waitSemaphores.size(),
      .pWaitSemaphores = waitSemaphores.data(),
      .pWaitDstStageMask = waitDstStageMask.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &m_VkGraphicsCommandBuffer,
      .signalSemaphoreCount = signalSemaphores.size(),
      .pSignalSemaphores = signalSemaphores.data()};
    VK_CHK(vkQueueSubmit(m_VkGraphicsQueue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    MXC_CHK(swap.QueuePresent());
    return MXC_SUCCESS;
}
