#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include "moxaic_logging.hpp"

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>

#ifdef WIN32

#include <vulkan/vulkan_win32.h>

#endif

#define MXC_LOAD_VULKAN_FUNCTION(function) \
    MXC_LOG_NAMED(#function); \
    VkFunc.function = (PFN_vk##function) vkGetInstanceProcAddr(g_VulkanInstance, "vk"#function); \
    if (VkFunc.function == nullptr) { \
        MXC_LOG_ERROR("Load Fail: ", #function); \
        return false; \
    }

struct Moxaic::VulkanDebug Moxaic::VkDebug;

static VkInstance g_VulkanInstance;
static VkSurfaceKHR g_VulkanSurface;
static bool g_VulkanValidationLayers;
static VkDebugUtilsMessengerEXT g_VulkanDebugMessenger;

static struct
{
#ifdef WIN32
    PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR;
#endif
    PFN_vkCmdDrawMeshTasksEXT CmdDrawMeshTasksEXT;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
} VkFunc;

static const char *SeverityToName(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return "";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "! ";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "!!! ";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{

    printf("%s(%s:%d) %s\n%s",
           SeverityToName(messageSeverity),
           Moxaic::VkDebug.VulkanDebugFile,
           Moxaic::VkDebug.VulkanDebugLine,
           Moxaic::VkDebug.VulkanDebugCommand,
           pCallbackData->pMessage);
    return VK_FALSE;
}

static bool CheckVulkanInstanceLayerProperties(const std::vector<const char *> &requiredInstanceLayerNames)
{
    MXC_LOG_FUNCTION();

    unsigned int count = 0;
    MXC_VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> properties(count);
    MXC_VK_CHECK(vkEnumerateInstanceLayerProperties(&count, properties.data()));

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

static bool CheckVulkanInstanceExtensions(const std::vector<const char *> &requiredInstanceExtensionsNames)
{
    MXC_LOG_FUNCTION();

    unsigned int count = 0;
    MXC_VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> properties(count);
    MXC_VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()));

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
    MXC_LOG_FUNCTION();

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
    if (g_VulkanValidationLayers) {
        requiredInstanceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    }
    CheckVulkanInstanceLayerProperties(requiredInstanceLayerNames);
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
    if (g_VulkanValidationLayers) {
        requiredInstanceExtensionsNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    CheckVulkanInstanceExtensions(requiredInstanceExtensionsNames);
    createInfo.enabledExtensionCount = requiredInstanceExtensionsNames.size();
    createInfo.ppEnabledExtensionNames = requiredInstanceExtensionsNames.data();

    MXC_VK_CHECK(vkCreateInstance(&createInfo, MXC_VK_ALLOCATOR, &g_VulkanInstance));

    return true;
}

static bool LoadVulkanFunctionPointers()
{
    MXC_LOG_FUNCTION();

#ifdef WIN32
    MXC_LOAD_VULKAN_FUNCTION(GetMemoryWin32HandleKHR);
#endif
    MXC_LOAD_VULKAN_FUNCTION(CmdDrawMeshTasksEXT);
    MXC_LOAD_VULKAN_FUNCTION(CreateDebugUtilsMessengerEXT);

    return true;
}

static bool CreateVulkanDebugOutput()
{
    MXC_LOG_FUNCTION();

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
    MXC_VK_CHECK(VkFunc.CreateDebugUtilsMessengerEXT(g_VulkanInstance,
                                                     &debugUtilsMessengerCreateInfo,
                                                     MXC_VK_ALLOCATOR,
                                                     &g_VulkanDebugMessenger));

    return true;
}

bool Moxaic::VulkanInit(SDL_Window *const pWindow, const bool enableValidationLayers)
{
    MXC_LOG_FUNCTION();

    g_VulkanValidationLayers = enableValidationLayers;

    if (!CreateVulkanInstance(pWindow))
        return false;

    if (!LoadVulkanFunctionPointers())
        return false;

    if (!CreateVulkanDebugOutput())
        return false;

    if (!SDL_Vulkan_CreateSurface(pWindow, g_VulkanInstance, &g_VulkanSurface))
        return false;

//    auto device = new VulkanDevice(g_VulkanInstance, g_VulkanSurface);
//    if (!device->Init())
//        return false;
//
//    g_pVulkanDevices.push_back(device);

    return true;
}

VkInstance Moxaic::VulkanInstance() {
    assert((g_VulkanInstance != nullptr) && "Vulkan not initialized!");
    return g_VulkanInstance;
}

VkSurfaceKHR Moxaic::VulkanSurface() {
    assert((g_VulkanSurface != nullptr) && "Vulkan not initialized!");
    return g_VulkanSurface;
}
