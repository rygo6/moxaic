#include "moxaic_vulkan.hpp"
#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_window.hpp"

#include <cassert>
#include <vector>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

using namespace Moxaic;

static VkInstance g_VulkanInstance;
static VkSurfaceKHR g_VulkanSurface;
static bool g_VulkanValidationLayers;
static VkDebugUtilsMessengerEXT g_VulkanDebugMessenger;

static char const* SeverityToName(VkDebugUtilsMessageSeverityFlagBitsEXT const severity)
{
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "!";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "!!!";
        default:
            return "";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT const messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                                    void* pUserData)
{
    SetConsoleTextRed();
    printf("%s %s (%s:%d) %s\n%s\n",
           SeverityToName(messageSeverity),
           string_Role(Moxaic::Role),
           Vulkan::VkDebug.DebugFile,
           Vulkan::VkDebug.DebugLine,
           Vulkan::VkDebug.DebugCommand,
           pCallbackData->pMessage);
    SetConsoleTextDefault();
    return VK_FALSE;
}

static MXC_RESULT CheckVulkanInstanceLayerProperties(std::vector<char const*> const& requiredInstanceLayerNames)
{
    MXC_LOG_FUNCTION();
    unsigned int count = 0;
    VK_CHK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> properties(count);
    VK_CHK(vkEnumerateInstanceLayerProperties(&count, properties.data()));

    for (auto const requiredName: requiredInstanceLayerNames) {
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
    return MXC_SUCCESS;
}

static MXC_RESULT CheckVulkanInstanceExtensions(std::vector<char const*> const& requiredInstanceExtensionsNames)
{
    MXC_LOG_FUNCTION();
    unsigned int count = 0;
    VK_CHK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> properties(count);
    VK_CHK(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()));

    for (auto const requiredName: requiredInstanceExtensionsNames) {
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

    return MXC_SUCCESS;
}

static MXC_RESULT CreateVulkanInstance()
{
    MXC_LOG_FUNCTION();
    constexpr VkApplicationInfo applicationInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = Moxaic::ApplicationName,
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Vulkan",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_HEADER_VERSION_COMPLETE,
    };
    VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &applicationInfo,
    };

    // Instance Layers
    std::vector<char const*> requiredInstanceLayerNames;
    if (g_VulkanValidationLayers) {
        requiredInstanceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    }
    CheckVulkanInstanceLayerProperties(requiredInstanceLayerNames);
    createInfo.enabledLayerCount = requiredInstanceLayerNames.size();
    createInfo.ppEnabledLayerNames = requiredInstanceLayerNames.data();

    auto requiredInstanceExtensionsNames = Window::GetVulkanInstanceExtentions();
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

    VK_CHK(vkCreateInstance(&createInfo, VK_ALLOC, &g_VulkanInstance));

    return MXC_SUCCESS;
}

static MXC_RESULT LoadVulkanFunctionPointers()
{
    MXC_LOG_FUNCTION();
#define VK_FUNC(func)                                                                                 \
    MXC_LOG(#func);                                                                                   \
    Moxaic::Vulkan::VkFunc.func = (PFN_vk##func) vkGetInstanceProcAddr(g_VulkanInstance, "vk" #func); \
    if (Moxaic::Vulkan::VkFunc.func == nullptr) {                                                     \
        MXC_LOG_ERROR("Load Fail: ", #func);                                                          \
        return false;                                                                                 \
    }
    VK_FUNCS
#undef VK_FUNC
    return MXC_SUCCESS;
}

static MXC_RESULT CreateVulkanDebugOutput()
{
    MXC_LOG_FUNCTION();
    constexpr VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = DebugCallback,
    };
    VK_CHK(Vulkan::VkFunc.CreateDebugUtilsMessengerEXT(g_VulkanInstance,
                                                       &debugUtilsMessengerCreateInfo,
                                                       VK_ALLOC,
                                                       &g_VulkanDebugMessenger));
    return MXC_SUCCESS;
}

MXC_RESULT Vulkan::Init(bool const enableValidationLayers)
{
    MXC_LOG("Vulkan::Init");

    g_VulkanValidationLayers = enableValidationLayers;

    MXC_CHK(CreateVulkanInstance());
    MXC_CHK(LoadVulkanFunctionPointers());
    MXC_CHK(CreateVulkanDebugOutput());

    SDL_assert((Window::window() != nullptr) && "Window not initialized!");
    MXC_CHK(Window::InitSurface(g_VulkanInstance,
                                &g_VulkanSurface));

    return MXC_SUCCESS;
}

VkInstance Vulkan::vkInstance()
{
    SDL_assert((g_VulkanInstance != nullptr) && "Vulkan not initialized!");
    return g_VulkanInstance;
}

VkSurfaceKHR Vulkan::vkSurface()
{
    SDL_assert((g_VulkanSurface != nullptr) && "Vulkan not initialized!");
    return g_VulkanSurface;
}
