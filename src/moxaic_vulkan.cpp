// #define MXC_DISABLE_LOG

#include "moxaic_vulkan.hpp"

#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "moxaic_window.hpp"

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

using namespace Moxaic;

static VkInstance g_VulkanInstance;
static VkSurfaceKHR g_VulkanSurface;
static bool g_VulkanValidationLayers;
static VkDebugUtilsMessengerEXT g_VulkanDebugMessenger;

static const char* SeverityToName(const VkDebugUtilsMessageSeverityFlagBitsEXT severity)
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

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    const VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData)
{
    if (pCallbackData->messageIdNumber == -1841738615) {
        printf("%s\n", pCallbackData->pMessage + strlen("Validation Information: [ UNASSIGNED-DEBUG-PRINTF ] | MessageID = 0x92394c89 | "));
    } else {
        printf("%s %s %s\n",
               SeverityToName(messageSeverity),
               string_Role(Moxaic::role),
               pCallbackData->pMessage);
    }
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Moxaic::running = false;
    }
    return VK_FALSE;
}

static MXC_RESULT CheckVulkanInstanceLayerProperties(const std::vector<const char*>& requiredInstanceLayerNames)
{
    MXC_LOG_FUNCTION();
    unsigned int count = 0;
    VK_CHK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> properties(count);
    VK_CHK(vkEnumerateInstanceLayerProperties(&count, properties.data()));

    for (const auto requiredName: requiredInstanceLayerNames) {
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

static MXC_RESULT CheckVulkanInstanceExtensions(const std::vector<const char*>& requiredInstanceExtensionsNames)
{
    MXC_LOG_FUNCTION();
    unsigned int count = 0;
    VK_CHK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> properties(count);
    VK_CHK(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()));

    for (const auto requiredName: requiredInstanceExtensionsNames) {
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
    std::vector<const char*> requiredInstanceLayerNames;

    constexpr StaticArray enabledValidationFeatures{
      // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
      // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
      // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
      // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };
    const VkValidationFeaturesEXT validationFeatures{
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = nullptr,
      .enabledValidationFeatureCount = enabledValidationFeatures.size(),
      .pEnabledValidationFeatures = enabledValidationFeatures.data(),
      .disabledValidationFeatureCount = 0,
      .pDisabledValidationFeatures = nullptr,
    };
    if (g_VulkanValidationLayers) {
        requiredInstanceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
        createInfo.pNext = &validationFeatures;
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
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity{};
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    VkDebugUtilsMessageTypeFlagsEXT messageType{};
    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .flags = 0,
      .messageSeverity = messageSeverity,
      .messageType = messageType,
      .pfnUserCallback = DebugCallback,
      .pUserData = nullptr,
    };
    VK_CHK(Vulkan::VkFunc.CreateDebugUtilsMessengerEXT(g_VulkanInstance,
                                                       &debugUtilsMessengerCreateInfo,
                                                       VK_ALLOC,
                                                       &g_VulkanDebugMessenger));
    return MXC_SUCCESS;
}

MXC_RESULT Vulkan::Init(const bool enableValidationLayers)
{
    MXC_LOG("Vulkan::Init");

    g_VulkanValidationLayers = enableValidationLayers;

    MXC_CHK(CreateVulkanInstance());
    MXC_CHK(LoadVulkanFunctionPointers());
    if (g_VulkanValidationLayers) {
        MXC_CHK(CreateVulkanDebugOutput());
    }

    SDL_assert((Window::GetWindow() != nullptr) && "Window not initialized!");
    MXC_CHK(Window::InitSurface(g_VulkanInstance,
                                &g_VulkanSurface));

    return MXC_SUCCESS;
}

VkInstance Vulkan::GetVkInstance()
{
    SDL_assert((g_VulkanInstance != nullptr) && "Vulkan not initialized!");
    return g_VulkanInstance;
}

VkSurfaceKHR Vulkan::GetVkSurface()
{
    SDL_assert((g_VulkanSurface != nullptr) && "Vulkan not initialized!");
    return g_VulkanSurface;
}