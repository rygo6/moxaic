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
        return result; \
    } \
})

static bool g_EnableValidationLayers;
static VkInstance g_Instance;
static VkSurfaceKHR g_Surface;
static VkPhysicalDevice g_PhysicalDevice;
static VkDebugUtilsMessengerEXT g_DebugMessenger;
static VkPhysicalDeviceMeshShaderPropertiesEXT  g_PhysicalDeviceMeshShaderProperties;
static VkPhysicalDeviceProperties2  g_PhysicalDeviceProperties;
static VkPhysicalDeviceMemoryProperties g_PhysicalDeviceMemoryProperties;
static Moxaic::VulkanDevice g_Device;

static const char* SeverityToName(VkDebugUtilsMessageSeverityFlagBitsEXT severity){
    switch (severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return "Vulkan: ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "Vulkan Info: ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "Vulkan Warning: ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "Vulkan Error: ";
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

static bool CheckInstanceLayerProperties(const std::vector<const char *>& requiredInstanceLayerNames)
{
    unsigned int count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> properties(count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, properties.data()));

    for (const char *requiredName: requiredInstanceLayerNames) {
        MXC_LOG("Loading InstanceLayer: ", requiredName);
        bool found = false;
        for (VkLayerProperties property: properties) {
            if (strcmp(property.layerName, requiredName) == 0) {
                found = true;
                break;
            }
        }
        assert((found) && "Instance Layer Not Supported!");
    }

    return true;
}

static bool CheckInstanceExtensions(const std::vector<const char *>& requiredInstanceExtensionsNames)
{
    unsigned int count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> properties(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()));

    for (const char *requiredName: requiredInstanceExtensionsNames) {
        MXC_LOG("Loading InstanceExtension:", requiredName);
        bool found = false;
        for (VkExtensionProperties property: properties) {
            if (strcmp(property.extensionName, requiredName) == 0) {
                found = true;
                break;
            }
        }
        assert(found && "Instance Extension Not Supported!");
    }

    return true;
}

bool Moxaic::VulkanInit(SDL_Window *pWindow, bool enableValidationLayers)
{
    MXC_LOG("Initializing Vulkan");

    g_EnableValidationLayers = enableValidationLayers;

    // Create Instance
    const VkApplicationInfo applicationInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = k_ApplicationName,
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

    // Create Debug Output
    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
    };
    auto createDebugUtilsMessengerFunc = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_Instance,"vkCreateDebugUtilsMessengerEXT");
    assert((createDebugUtilsMessengerFunc != nullptr) && "Could not lodad PFN_vkCreateDebugUtilsMessengerEXT!");
    createDebugUtilsMessengerFunc(g_Instance, &debugUtilsMessengerCreateInfo, VK_ALLOCATOR, &g_DebugMessenger);

    // Create Surface
    SDL_Vulkan_CreateSurface(pWindow, g_Instance, &g_Surface);

    // Pick Physical VulkanDevice
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(g_Instance, &deviceCount, nullptr));
    assert((deviceCount > 0) && "Failed to find GPUs with Vulkan support!");
    VkPhysicalDevice devices[deviceCount];
    VK_CHECK(vkEnumeratePhysicalDevices(g_Instance, &deviceCount, devices));

    // Todo Implement Query OpenVR for the physical device to use
    g_PhysicalDevice = devices[0];

    // Gather Physical VulkanDevice Properties
    vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &g_PhysicalDeviceMemoryProperties);
    g_PhysicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    g_PhysicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    g_PhysicalDeviceProperties.pNext = &g_PhysicalDeviceMeshShaderProperties;
    vkGetPhysicalDeviceProperties2(g_PhysicalDevice, &g_PhysicalDeviceProperties);

    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.timestampPeriod);
    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[0]);
    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[1]);
    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupSize[2]);
    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.maxComputeWorkGroupInvocations);
    MXC_LOG_NAMED(g_PhysicalDeviceProperties.properties.limits.maxTessellationGenerationLevel);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxTaskPayloadSize);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxTaskPayloadAndSharedMemorySize);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxMeshOutputPrimitives);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxMeshOutputVertices);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxMeshWorkGroupInvocations);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxTaskWorkGroupInvocations);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxPreferredMeshWorkGroupInvocations);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.maxPreferredTaskWorkGroupInvocations);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.prefersLocalInvocationPrimitiveOutput);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.prefersLocalInvocationVertexOutput);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.prefersCompactPrimitiveOutput);
    MXC_LOG_NAMED(g_PhysicalDeviceMeshShaderProperties.prefersCompactVertexOutput);

    // Create Device

    g_Device.Init();

    return VK_SUCCESS;
}


Moxaic::VulkanDevice::VulkanDevice()
        : m_Device(VK_NULL_HANDLE)
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
{ }

Moxaic::VulkanDevice::~VulkanDevice()
{

}

bool Moxaic::VulkanDevice::Init()
{

    return false;
}