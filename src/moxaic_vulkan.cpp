#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#define VK_ALLOCATOR nullptr

#define VK_CHECK(command) \
({ \
    VkResult result = command; \
    if (result != VK_SUCCESS) { \
        printf("(%s:%d) VKCheck Fail! - %s - %d\n", __FUNCTION__, __LINE__, #command, result); \
        return false; \
    } \
})

static bool g_EnableValidationLayers;
static VkInstance g_Instance;
static VkSurfaceKHR g_Surface;
static VkDebugUtilsMessengerEXT g_DebugMessenger;
static Moxaic::VulkanDevice *g_pDevice = nullptr;

static const char *SeverityToName(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return "(Vulkan) ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "(Vulkan Info) ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "! (Vulkan Warning) ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "!!! (Vulkan Error) ";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{
    std::cout << SeverityToName(messageSeverity) << pCallbackData->pMessage << '\n';
    return VK_FALSE;
}

static bool CheckInstanceLayerProperties(const std::vector<const char *> &requiredInstanceLayerNames)
{
    unsigned int count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> properties(count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, properties.data()));

    for (auto requiredName: requiredInstanceLayerNames) {
        MXC_LOG("Loading InstanceLayer: ", requiredName);
        bool found = false;
        for (VkLayerProperties property: properties) {
            if (strcmp(property.layerName, requiredName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            MXC_LOG_ERROR("Instance Layer Not Supported!");
            return false;
        }
    }

    return true;
}

static bool CheckInstanceExtensions(const std::vector<const char *> &requiredInstanceExtensionsNames)
{
    unsigned int count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> properties(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()));

    for (auto requiredName: requiredInstanceExtensionsNames) {
        MXC_LOG("Loading InstanceExtension:", requiredName);
        bool found = false;
        for (VkExtensionProperties property: properties) {
            if (strcmp(property.extensionName, requiredName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            MXC_LOG_ERROR("Instance Extension Not Supported!");
            return false;
        }
    }

    return true;
}

static bool CreateVulkanInstance(SDL_Window *const pWindow)
{
    MXC_LOG("Creating Vulkan Instance");
    const VkApplicationInfo applicationInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = Moxaic::k_ApplicationName,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Vulkan",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_HEADER_VERSION_COMPLETE,
    };
    VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &applicationInfo,
    };

    // Instance Layers
    std::vector<const char *> requiredInstanceLayerNames;
    if (g_EnableValidationLayers) {
        requiredInstanceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    }
    CheckInstanceLayerProperties(requiredInstanceLayerNames);
    createInfo.enabledLayerCount = requiredInstanceLayerNames.size();
    createInfo.ppEnabledLayerNames = requiredInstanceLayerNames.data();

    // Instance Extensions
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(pWindow, &extensionCount, nullptr);
    std::vector<const char *> requiredInstanceExtensionsNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(pWindow, &extensionCount, requiredInstanceExtensionsNames.data());

    requiredInstanceExtensionsNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    requiredInstanceExtensionsNames.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    requiredInstanceExtensionsNames.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
    requiredInstanceExtensionsNames.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
    if (g_EnableValidationLayers) {
        requiredInstanceExtensionsNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    CheckInstanceExtensions(requiredInstanceExtensionsNames);
    createInfo.enabledExtensionCount = requiredInstanceExtensionsNames.size();
    createInfo.ppEnabledExtensionNames = requiredInstanceExtensionsNames.data();

    VK_CHECK(vkCreateInstance(&createInfo, VK_ALLOCATOR, &g_Instance));

    return true;
}

static bool CreateDebugOutput()
{
    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
    };
    auto createDebugUtilsMessengerFunc = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_Instance,
                                                                                                    "vkCreateDebugUtilsMessengerEXT");
    assert((createDebugUtilsMessengerFunc != nullptr) && "Could not lodad PFN_vkCreateDebugUtilsMessengerEXT!");
    VK_CHECK(
            createDebugUtilsMessengerFunc(g_Instance, &debugUtilsMessengerCreateInfo, VK_ALLOCATOR, &g_DebugMessenger));

    return true;
}

bool Moxaic::VulkanInit(SDL_Window *const pWindow, const bool enableValidationLayers)
{
    MXC_LOG("Initializing Vulkan");

    g_EnableValidationLayers = enableValidationLayers;

    if (!CreateVulkanInstance(pWindow))
        return false;

    if (!CreateDebugOutput())
        return false;

    if (!SDL_Vulkan_CreateSurface(pWindow, g_Instance, &g_Surface))
        return false;

    g_pDevice = new VulkanDevice(g_Instance, g_Surface);
    if (!g_pDevice->Init())
        return false;

    return true;
}

Moxaic::VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
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

Moxaic::VulkanDevice::~VulkanDevice()
{

}

bool Moxaic::VulkanDevice::PickPhysicalDevice()
{
    MXC_LOG("Vulkan Pick Physical Device");
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));
    assert((deviceCount > 0) && "Failed to find GPUs with Vulkan support!");
    VkPhysicalDevice devices[deviceCount];
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices));

    // Todo Implement Query OpenVR for the physical device to use
    m_PhysicalDevice = devices[0];

    // Gather Physical VulkanDevice Properties
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_PhysicalDeviceMemoryProperties);
    m_PhysicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    m_PhysicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_PhysicalDeviceProperties.pNext = &m_PhysicalDeviceMeshShaderProperties;
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &m_PhysicalDeviceProperties);

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
    MXC_LOG("Vulkan Find Queues");
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        MXC_LOG_ERROR("Failed to get graphicsQueue properties.");
        return false;
    }
    VkQueueFamilyGlobalPriorityPropertiesEXT queueFamilyGlobalPriorityProperties[queueFamilyCount];
    VkQueueFamilyProperties2 queueFamilies[queueFamilyCount];
    for (int i = 0; i < queueFamilyCount; ++i) {
        queueFamilyGlobalPriorityProperties[i] = {
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT
        };
        queueFamilies[i] = {
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
                .pNext = &queueFamilyGlobalPriorityProperties[i],
        };
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(m_PhysicalDevice, &queueFamilyCount, queueFamilies);

    bool foundGraphics = false;
    bool foundCompute = false;

    for (int i = 0; i < queueFamilyCount; ++i) {
        bool globalQueueSupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;
        bool graphicsSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool computeSupport = queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, g_Surface, &presentSupport);

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
    MXC_LOG("Vulkan Create Device");
    const float queuePriority = Moxaic::IsCompositor() ?
                                0.0f :
                                1.0f;
    const VkDeviceQueueGlobalPriorityCreateInfoEXT queueGlobalPriorityCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
            .globalPriority = Moxaic::IsCompositor()
                              ? VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT
                              : VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT
    };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.push_back(
            {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = &queueGlobalPriorityCreateInfo,
                    .queueFamilyIndex = m_GraphicsQueueFamilyIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
            }
    );
    queueCreateInfos.push_back(
            {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = &queueGlobalPriorityCreateInfo,
                    .queueFamilyIndex = m_ComputeQueueFamilyIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
            }
    );

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

    std::vector<const char *> requiredDeviceExtensions;
    requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    // Required by VK_KHR_spirv_1_4 - https://github.com/SaschaWillems/Vulkan/blob/master/examples/meshshader/meshshader.cpp
    requiredDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME);
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
    VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, VK_ALLOCATOR, &m_Device));

    return true;
}


bool Moxaic::VulkanDevice::Init()
{
    if (!PickPhysicalDevice())
        return false;

    if (!FindQueues())
        return false;

    if (!CreateDevice())
        return false;

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_ComputeQueueFamilyIndex, 0, &m_ComputeQueue);

//    SDL_Vulkan_GetDrawableSize(window, &width, &height);


    return true;
}