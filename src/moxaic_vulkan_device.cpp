// #define MXC_DISABLE_LOG

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
static MXC_RESULT MemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
                                           const VkMemoryPropertyFlags& requiredMemoryProperties,
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
    VK_CHK(vkEnumeratePhysicalDevices(Vulkan::GetVkInstance(), &deviceCount, nullptr));
    if (deviceCount == 0) {
        MXC_LOG_ERROR("Failed to find GPUs with Vulkan support!");
        return MXC_FAIL;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHK(vkEnumeratePhysicalDevices(Vulkan::GetVkInstance(), &deviceCount, devices.data()));

    // Todo Implement Query OpenVR for the physical. GetVkDevice to use
    vkPhysicalDevice = devices.front();


    pysicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalDeviceProperties.pNext = &pysicalDeviceMeshShaderProperties;
    vkGetPhysicalDeviceProperties2(vkPhysicalDevice, &physicalDeviceProperties);

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &physicalDeviceMemoryProperties);

    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.timestampPeriod);
    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[0]);
    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[1]);
    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[2]);
    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.maxComputeWorkGroupInvocations);
    MXC_LOG_NAMED(physicalDeviceProperties.properties.limits.maxTessellationGenerationLevel);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxTaskPayloadSize);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxTaskPayloadAndSharedMemorySize);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxMeshOutputPrimitives);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxMeshOutputVertices);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxMeshWorkGroupInvocations);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxTaskWorkGroupInvocations);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxPreferredMeshWorkGroupInvocations);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.maxPreferredTaskWorkGroupInvocations);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.prefersLocalInvocationPrimitiveOutput);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.prefersLocalInvocationVertexOutput);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.prefersCompactPrimitiveOutput);
    MXC_LOG_NAMED(pysicalDeviceMeshShaderProperties.prefersCompactVertexOutput);

    return MXC_SUCCESS;
}

MXC_RESULT Device::FindQueues()
{
    MXC_LOG("Vulkan::Device::FindQueues");

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(vkPhysicalDevice, &queueFamilyCount, nullptr);
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
    vkGetPhysicalDeviceQueueFamilyProperties2(vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundCompute = false;

    for (int i = 0; i < queueFamilyCount; ++i) {
        const bool globalQueueSupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;
        const bool graphicsSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        const bool computeSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice,
                                             i,
                                             Vulkan::GetVkSurface(),
                                             &presentSupport);

        if (!foundGraphics && graphicsSupport && presentSupport) {
            if (!globalQueueSupport) {
                MXC_LOG_ERROR("No Global Priority On Graphics!");
            }
            graphicsQueueFamilyIndex = i;
            foundGraphics = true;
            MXC_LOG_NAMED(graphicsQueueFamilyIndex);
        }

        if (!foundCompute && computeSupport && presentSupport && !graphicsSupport) {
            if (!globalQueueSupport) {
                MXC_LOG_ERROR("No Global Priority On Compute!");
            }
            computeQueueFamilyIndex = i;
            foundCompute = true;
            MXC_LOG_NAMED(computeQueueFamilyIndex);
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

    const float queuePriority = Moxaic::IsCompositor() ? 1.0f : 0.0f;
    const VkDeviceQueueGlobalPriorityCreateInfoEXT queueGlobalPriorityCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
      .globalPriority = Moxaic::IsCompositor() ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT};
    const StaticArray queueCreateInfos{
      (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = &queueGlobalPriorityCreateInfo,
        .queueFamilyIndex = graphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority},
      (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = &queueGlobalPriorityCreateInfo,
        .queueFamilyIndex = computeQueueFamilyIndex,
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
    vkGetPhysicalDeviceFeatures2(vkPhysicalDevice, &supportedPhysicalDeviceFeatures2);
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
      .taskShader = VK_TRUE,
      .meshShader = VK_TRUE,
    };
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
      .pNext = &physicalDeviceMeshShaderFeatures,
      .globalPriorityQuery = VK_TRUE,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
      .pNext = &physicalDeviceGlobalPriorityQueryFeatures,
      .robustBufferAccess2 = VK_TRUE,
      .robustImageAccess2 = VK_TRUE,
      .nullDescriptor = VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features enabledFeatures13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &physicalDeviceRobustness2Features,
      //            .synchronization2 = true,
      .robustImageAccess = VK_TRUE,
      .shaderDemoteToHelperInvocation = VK_TRUE,
      .shaderTerminateInvocation = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features enabledFeatures12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &enabledFeatures13,
      .shaderFloat16 = VK_TRUE,
      .shaderInt8 = VK_TRUE,
      .samplerFilterMinmax = VK_TRUE,
      .hostQueryReset = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
      .bufferDeviceAddressCaptureReplay = VK_TRUE,
    };
    VkPhysicalDeviceVulkan11Features enabledFeatures11 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &enabledFeatures12,
    };
    VkPhysicalDeviceFeatures2 enabledFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
      .pNext = &enabledFeatures11,
      .features = {
        .robustBufferAccess = VK_TRUE,
        .tessellationShader = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
        .vertexPipelineStoresAndAtomics = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE,
        .shaderStorageImageMultisample = VK_TRUE,
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
      // VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
      VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
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
    VK_CHK(vkCreateDevice(vkPhysicalDevice, &createInfo, VK_ALLOC, &vkDevice));

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
    const StaticArray subpass{
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
        .format = kColorBufferFormat,
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
        .format = kNormalBufferFormat,
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
        .format = kGBufferFormat,
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
        .format = kDepthBufferFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      }};
    const VkRenderPassCreateInfo renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = attachments.size(),
      .pAttachments = attachments.data(),
      .subpassCount = subpass.size(),
      .pSubpasses = subpass.data(),
      .dependencyCount = dependencies.size(),
      .pDependencies = dependencies.data(),
    };
    VK_CHK(vkCreateRenderPass(vkDevice, &renderPassInfo, VK_ALLOC, &vkRenderPass));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateCommandBuffers()
{
    MXC_LOG("Vulkan::Device::CreateCommandBuffers");
    // Graphics + Compute
    const VkCommandPoolCreateInfo graphicsPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(vkDevice,
                               &graphicsPoolInfo,
                               VK_ALLOC,
                               &vkGraphicsCommandPool));
    const VkCommandBufferAllocateInfo graphicsAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = vkGraphicsCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(vkDevice,
                                    &graphicsAllocateInfo,
                                    &vkGraphicsCommandBuffer));

    // Compute
    const VkCommandPoolCreateInfo computePoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = computeQueueFamilyIndex,
    };
    VK_CHK(vkCreateCommandPool(vkDevice,
                               &computePoolInfo,
                               VK_ALLOC,
                               &vkComputeCommandPool));
    const VkCommandBufferAllocateInfo computeAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = vkComputeCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(vkDevice,
                                    &computeAllocInfo,
                                    &vkComputeCommandBuffer));

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
    vkCreateQueryPool(vkDevice, &queryPoolCreateInfo, VK_ALLOC, &vkQueryPool);

    constexpr StaticArray poolSizes{
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 4,
      },
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 4,
      },
      (VkDescriptorPoolSize){
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 4,
      },
    };
    const VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 12,
      .poolSizeCount = poolSizes.size(),
      .pPoolSizes = poolSizes.data(),
    };
    VK_CHK(vkCreateDescriptorPool(vkDevice,
                                  &poolInfo,
                                  VK_ALLOC,
                                  &vkDescriptorPool));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateSamplers()
{
    MXC_LOG("Vulkan::Device::CreateSamplers");
    const VkSamplerCreateInfo linearSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHK(vkCreateSampler(vkDevice, &linearSamplerInfo, VK_ALLOC, &vkLinearSampler));

    const VkSamplerCreateInfo nearestSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHK(vkCreateSampler(vkDevice, &nearestSamplerInfo, VK_ALLOC, &vkNearestSampler));

    const VkSamplerReductionModeCreateInfoEXT samplerReductionModeCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,
      .pNext = nullptr,
      .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
    };
    const VkSamplerCreateInfo minSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = &samplerReductionModeCreateInfo,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHK(vkCreateSampler(vkDevice, &minSamplerInfo, VK_ALLOC, &vkMinSampler));

    return MXC_SUCCESS;
}

MXC_RESULT Device::Init()
{
    MXC_LOG("Vulkan::Device::Init");

    SDL_assert(Vulkan::GetVkInstance() != VK_NULL_HANDLE);
    SDL_assert(Vulkan::GetVkSurface() != VK_NULL_HANDLE);

    MXC_CHK(PickPhysicalDevice());
    MXC_CHK(FindQueues());
    MXC_CHK(CreateDevice());

    vkGetDeviceQueue(vkDevice, graphicsQueueFamilyIndex, 0, &vkGraphicsQueue);
    vkGetDeviceQueue(vkDevice, computeQueueFamilyIndex, 0, &vkComputeQueue);

    MXC_CHK(CreateRenderPass());
    MXC_CHK(CreateCommandBuffers());
    MXC_CHK(CreatePools());
    MXC_CHK(CreateSamplers());

    return MXC_SUCCESS;
}

MXC_RESULT Device::AllocateBindImageExport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlags externalHandleType,
                                           VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(vkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(physicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    // todo your supposed check if it wants dedicated memory
    //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
    //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    //            .image = pTestTexture->image,
    //            .buffer = VK_NULL_HANDLE,
    //    };
    const VkExportMemoryAllocateInfo exportAllocInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      //            .pNext =&dedicatedAllocInfo,
      .handleTypes = externalHandleType};
    const VkMemoryAllocateInfo allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &exportAllocInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(vkDevice,
                            &allocateInfo,
                            VK_ALLOC,
                            pDeviceMemory));
    VK_CHK(vkBindImageMemory(vkDevice,
                             image,
                             *pDeviceMemory,
                             0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::AllocateBindImageImport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlagBits externalHandleType,
                                           HANDLE const externalHandle,
                                           VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(vkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(physicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    // todo your supposed check if it wants dedicated memory
    //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
    //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    //            .image = pTestTexture->image,
    //            .buffer = VK_NULL_HANDLE,
    //    };
    const VkImportMemoryWin32HandleInfoKHR importMemoryInfo{
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      //            .pNext = &dedicatedAllocInfo,
      .handleType = externalHandleType,
      .handle = externalHandle,
    };
    const VkMemoryAllocateInfo allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &importMemoryInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(
      vkDevice,
      &allocateInfo,
      VK_ALLOC,
      pDeviceMemory));
    VK_CHK(vkBindImageMemory(
      vkDevice,
      image,
      *pDeviceMemory,
      0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::AllocateBindImage(const VkMemoryPropertyFlags properties,
                                     const VkImage image,
                                     VkDeviceMemory* pDeviceMemory) const
{
    VkMemoryRequirements memRequirements{};
    uint32_t memTypeIndex;
    vkGetImageMemoryRequirements(vkDevice,
                                 image,
                                 &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(physicalDeviceMemoryProperties,
                                     properties,
                                     memRequirements.memoryTypeBits,
                                     &memTypeIndex));
    const VkMemoryAllocateInfo allocateInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex,
    };
    VK_CHK(vkAllocateMemory(
      vkDevice,
      &allocateInfo,
      VK_ALLOC,
      pDeviceMemory));
    VK_CHK(vkBindImageMemory(
      vkDevice,
      image,
      *pDeviceMemory,
      0));
    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateAllocateBindBuffer(const VkBufferUsageFlags usage,
                                            const VkMemoryPropertyFlags properties,
                                            const VkDeviceSize bufferSize,
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

MXC_RESULT Device::CreateAllocateBindBuffer(const VkBufferUsageFlags usage,
                                            const VkMemoryPropertyFlags properties,
                                            const VkDeviceSize bufferSize,
                                            const Vulkan::Locality locality,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory,
                                            HANDLE* pExternalMemory) const
{
    constexpr VkExternalMemoryBufferCreateInfoKHR externalBufferInfo{
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR,
      .pNext = nullptr,
      .handleTypes = MXC_EXTERNAL_MEMORY_HANDLE};
    const VkBufferCreateInfo bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = locality == Locality::Local ? nullptr : &externalBufferInfo,
      .flags = 0,
      .size = bufferSize,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
    };
    VK_CHK(vkCreateBuffer(vkDevice,
                          &bufferCreateInfo,
                          nullptr,
                          pBuffer));

    VkMemoryRequirements memRequirements = {};
    uint32_t memTypeIndex;
    vkGetBufferMemoryRequirements(vkDevice,
                                  *pBuffer,
                                  &memRequirements);
    MXC_CHK(MemoryTypeFromProperties(physicalDeviceMemoryProperties,
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
    const VkExportMemoryAllocateInfo exportAllocInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = &exportMemoryWin32HandleInfo,
      .handleTypes = MXC_EXTERNAL_MEMORY_HANDLE};
    const VkMemoryAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = locality == Locality::Local ? nullptr : &exportAllocInfo,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = memTypeIndex};
    VK_CHK(vkAllocateMemory(vkDevice,
                            &allocInfo,
                            VK_ALLOC,
                            pDeviceMemory));
    VK_CHK(vkBindBufferMemory(vkDevice,
                              *pBuffer,
                              *pDeviceMemory,
                              0));

    if (locality == Locality::External) {
#if WIN32
        const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .memory = *pDeviceMemory,
          .handleType = MXC_EXTERNAL_MEMORY_HANDLE};
        VK_CHK(Vulkan::VkFunc.GetMemoryWin32HandleKHR(vkDevice,
                                                      &getWin32HandleInfo,
                                                      pExternalMemory));
#endif
    }

    return MXC_SUCCESS;
}

MXC_RESULT Device::CreateAndPopulateStagingBuffer(const void* srcData,
                                                  const VkDeviceSize bufferSize,
                                                  VkBuffer* pStagingBuffer,
                                                  VkDeviceMemory* pStagingBufferMemory) const
{
    MXC_CHK(CreateAllocateBindBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     bufferSize,
                                     pStagingBuffer,
                                     pStagingBufferMemory));
    void* dstData;
    vkMapMemory(vkDevice, *pStagingBufferMemory, 0, bufferSize, 0, &dstData);
    memcpy(dstData, srcData, bufferSize);
    vkUnmapMemory(vkDevice, *pStagingBufferMemory);
    return MXC_SUCCESS;
}

MXC_RESULT Device::CopyBufferToBuffer(const VkDeviceSize bufferSize,
                                      const VkBuffer srcBuffer,
                                      const VkBuffer dstBuffer) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    const VkBufferCopy copyRegion = {
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

MXC_RESULT Device::CopyBufferToImage(const VkExtent2D imageExtent,
                                     const VkBuffer srcBuffer,
                                     const VkImage dstImage) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    const VkBufferImageCopy region{
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

MXC_RESULT Device::CreateAllocateBindPopulateBufferViaStaging(const void* srcData,
                                                              const VkBufferUsageFlagBits usage,
                                                              const VkDeviceSize bufferSize,
                                                              VkBuffer* pBuffer,
                                                              VkDeviceMemory* pBufferMemory) const
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    MXC_CHK(CreateAndPopulateStagingBuffer(srcData,
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
    vkDestroyBuffer(vkDevice,
                    stagingBuffer,
                    VK_ALLOC);
    vkFreeMemory(vkDevice,
                 stagingBufferMemory,
                 VK_ALLOC);
    return MXC_SUCCESS;
}

// TODO implemented for unified graphics + transfer only right now
//  https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#transfer-dependencies
MXC_RESULT Device::TransitionImageLayoutImmediate(const VkImage image,
                                                  const VkImageLayout oldLayout,
                                                  const VkImageLayout newLayout,
                                                  const VkAccessFlags srcAccessMask,
                                                  const VkAccessFlags dstAccessMask,
                                                  const VkPipelineStageFlags srcStageMask,
                                                  const VkPipelineStageFlags dstStageMask,
                                                  const VkImageAspectFlags aspectMask) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    const VkImageMemoryBarrier imageMemoryBarrier{
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

MXC_RESULT Device::TransitionImageLayoutImmediate(const VkImage image,
                                                  const Barrier& src,
                                                  const Barrier& dst,
                                                  const VkImageAspectFlags aspectMask) const
{
    VkCommandBuffer commandBuffer;
    MXC_CHK(BeginImmediateCommandBuffer(&commandBuffer));
    const VkImageMemoryBarrier imageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = src.GetAccessMask(aspectMask),
      .dstAccessMask = dst.GetAccessMask(aspectMask),
      .oldLayout = src.GetLayout(aspectMask),
      .newLayout = dst.GetLayout(aspectMask),
      .srcQueueFamilyIndex = GetSrcQueue(src),
      .dstQueueFamilyIndex = GetDstQueue(src, dst),
      .image = image,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    vkCmdPipelineBarrier(commandBuffer,
                         src.GetStageMask(aspectMask),
                         dst.GetStageMask(aspectMask),
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
    const VkCommandBufferAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = vkGraphicsCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VK_CHK(vkAllocateCommandBuffers(vkDevice,
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
MXC_RESULT Device::EndImmediateCommandBuffer(const VkCommandBuffer commandBuffer) const
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
      .pSignalSemaphores = nullptr};
    VK_CHK(vkQueueSubmit(vkGraphicsQueue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    VK_CHK(vkQueueWaitIdle(vkGraphicsQueue));
    vkFreeCommandBuffers(vkDevice,
                         vkGraphicsCommandPool,
                         1,
                         &commandBuffer);
    return MXC_SUCCESS;
}

// todo pull from a pool of command buffers
VkCommandBuffer Device::BeginGraphicsCommandBuffer() const
{
    vkResetCommandBuffer(vkGraphicsCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    constexpr VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(vkGraphicsCommandBuffer, &beginInfo);
    const VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(Window::GetExtents().width),
      .height = static_cast<float>(Window::GetExtents().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetViewport(vkGraphicsCommandBuffer,
                     0,
                     1,
                     &viewport);
    const VkRect2D scissor{
      .offset = {0, 0},
      .extent = Window::GetExtents(),
    };
    vkCmdSetScissor(vkGraphicsCommandBuffer,
                    0,
                    1,
                    &scissor);
    return vkGraphicsCommandBuffer;
}

void Device::BeginRenderPass(const Framebuffer& framebuffer,
                             const VkClearColorValue& backgroundColor) const
{
    StaticArray<VkClearValue, 4> clearValues;
    clearValues[0].color = backgroundColor;
    clearValues[1].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[2].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    const VkRenderPassBeginInfo renderPassBeginInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = vkRenderPass,
      .framebuffer = framebuffer.GetVkFramebuffer(),
      .renderArea = {
        .offset = {0, 0},
        .extent = framebuffer.GetExtents(),
      },
      .clearValueCount = clearValues.size(),
      .pClearValues = clearValues.data(),
    };
    VK_CHK_VOID(vkCmdBeginRenderPass(vkGraphicsCommandBuffer,
                                     &renderPassBeginInfo,
                                     VK_SUBPASS_CONTENTS_INLINE));
}

MXC_RESULT Device::SubmitGraphicsQueue(Semaphore* const pTimelineSemaphore) const
{
    return SubmitQueue(vkGraphicsCommandBuffer,
                       vkGraphicsQueue,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       pTimelineSemaphore);
}

MXC_RESULT Device::SubmitGraphicsQueueAndPresent(const Swap& swap,
                                                 const uint32_t swapIndex,
                                                 Semaphore* const pTimelineSemaphore) const
{
    return SubmitQueueAndPresent(vkGraphicsCommandBuffer,
                                 vkGraphicsQueue,
                                 swap,
                                 swapIndex,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, pTimelineSemaphore);
}

VkCommandBuffer Device::BeginComputeCommandBuffer() const
{
    vkResetCommandBuffer(vkComputeCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    constexpr VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(vkComputeCommandBuffer, &beginInfo);
    return vkComputeCommandBuffer;
}

MXC_RESULT Device::SubmitComputeQueue(Semaphore* pTimelineSemaphore) const
{
    return SubmitQueue(vkComputeCommandBuffer,
                       vkComputeQueue,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       pTimelineSemaphore);
}

MXC_RESULT Device::SubmitComputeQueueAndPresent(const VkCommandBuffer commandBuffer,
                                                const Swap& swap,
                                                const uint32_t swapIndex,
                                                Semaphore* const pTimelineSemaphore) const
{
    return SubmitQueueAndPresent(commandBuffer,
                                 vkComputeQueue,
                                 swap,
                                 swapIndex,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 pTimelineSemaphore);
}

void Device::ResetTimestamps() const
{
    vkResetQueryPool(vkDevice, vkQueryPool, 0, QueryPoolCount);
}

void Device::WriteTimestamp(const VkCommandBuffer commandbuffer,
                            const VkPipelineStageFlagBits pipelineStage,
                            const uint32_t query) const
{
    vkCmdWriteTimestamp(commandbuffer, pipelineStage, vkQueryPool, query);
}

StaticArray<double, Device::QueryPoolCount> Device::GetTimestamps() const
{
    uint64_t timestampsNS[QueryPoolCount];
    vkGetQueryPoolResults(vkDevice,
                          vkQueryPool,
                          0,
                          QueryPoolCount,
                          sizeof(uint64_t) * QueryPoolCount,
                          timestampsNS,
                          sizeof(uint64_t),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    StaticArray<double, QueryPoolCount> timestampsMS;
    for (int i = 0; i < QueryPoolCount; ++i) {
        timestampsMS[i] = double(timestampsNS[i]) / double(1000000);// ns to ms
    }
    return timestampsMS;
}

MXC_RESULT Device::SubmitQueueAndPresent(const VkCommandBuffer& commandBuffer,
                                         const VkQueue& queue,
                                         const Swap& swap,
                                         const uint32_t swapIndex,
                                         const VkPipelineStageFlags& waitDstStageMask,
                                         Semaphore* const pTimelineSemaphore) const
{
    // https://www.khronos.org/blog/vulkan-timeline-semaphores
    const uint64_t waitValue = pTimelineSemaphore->localWaitValue_;
    pTimelineSemaphore->localWaitValue_++;
    const uint64_t signalValue = pTimelineSemaphore->localWaitValue_;
    const StaticArray waitSemaphoreValues{
      (uint64_t) waitValue,
      (uint64_t) 0,
    };
    const StaticArray signalSemaphoreValues{
      (uint64_t) signalValue,
      (uint64_t) 0,
    };
    const VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount = waitSemaphoreValues.size(),
      .pWaitSemaphoreValues = waitSemaphoreValues.data(),
      .signalSemaphoreValueCount = signalSemaphoreValues.size(),
      .pSignalSemaphoreValues = signalSemaphoreValues.data(),
    };
    const StaticArray waitSemaphores{
      pTimelineSemaphore->vkSemaphore(),
      swap.GetVkAcquireCompleteSemaphore(),
    };
    const StaticArray waitDstStageMasks{
      waitDstStageMask,
      waitDstStageMask,
    };
    const StaticArray signalSemaphores{
      pTimelineSemaphore->vkSemaphore(),
      swap.GetVkRenderCompleteSemaphore(),
    };
    const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSemaphoreSubmitInfo,
      .waitSemaphoreCount = waitSemaphores.size(),
      .pWaitSemaphores = waitSemaphores.data(),
      .pWaitDstStageMask = waitDstStageMasks.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
      .signalSemaphoreCount = signalSemaphores.size(),
      .pSignalSemaphores = signalSemaphores.data()};
    VK_CHK(vkQueueSubmit(queue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    MXC_CHK(swap.QueuePresent(queue,
                              swapIndex));
    return MXC_SUCCESS;
}

MXC_RESULT Device::SubmitQueue(const VkCommandBuffer& commandBuffer,
                               const VkQueue& queue,
                               const VkPipelineStageFlags& waitDstStageMask,
                               Semaphore* const pTimelineSemaphore) const
{
    // https://www.khronos.org/blog/vulkan-timeline-semaphores
    const uint64_t waitValue = pTimelineSemaphore->localWaitValue_;
    pTimelineSemaphore->localWaitValue_++;
    const uint64_t signalValue = pTimelineSemaphore->localWaitValue_;
    const StaticArray waitSemaphoreValues{
      waitValue,
    };
    const StaticArray signalSemaphoreValues{
      signalValue,
    };
    const VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount = waitSemaphoreValues.size(),
      .pWaitSemaphoreValues = waitSemaphoreValues.data(),
      .signalSemaphoreValueCount = signalSemaphoreValues.size(),
      .pSignalSemaphoreValues = signalSemaphoreValues.data(),
    };
    const StaticArray waitSemaphores{
      pTimelineSemaphore->vkSemaphore(),
    };
    const StaticArray waitDstStageMasks{
      waitDstStageMask,
    };
    const StaticArray signalSemaphores{
      pTimelineSemaphore->vkSemaphore(),
    };
    const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSemaphoreSubmitInfo,
      .waitSemaphoreCount = waitSemaphores.size(),
      .pWaitSemaphores = waitSemaphores.data(),
      .pWaitDstStageMask = waitDstStageMasks.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
      .signalSemaphoreCount = signalSemaphores.size(),
      .pSignalSemaphores = signalSemaphores.data()};
    VK_CHK(vkQueueSubmit(queue,
                         1,
                         &submitInfo,
                         VK_NULL_HANDLE));
    return MXC_SUCCESS;
}
