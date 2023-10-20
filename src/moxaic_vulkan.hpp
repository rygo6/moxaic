#pragma once

#include "main.hpp"

#include <vulkan/vulkan.hpp>
#include <SDL2/SDL.h>

namespace Moxaic
{
    class VulkanDevice
    {
    public:
        VulkanDevice();
        virtual ~VulkanDevice();
        bool Init();

    private:
        VkDevice m_Device;

        VkQueue m_GraphicsQueue;
        uint32_t m_GraphicsQueueFamilyIndex;

        VkQueue m_ComputeQueue;
        uint32_t m_ComputeQueueFamilyIndex;

        VkRenderPass m_RenderPass;

        VkDescriptorPool m_DescriptorPool;
        VkQueryPool m_QueryPool;
        VkCommandPool m_GraphicsCommandPool;
        VkCommandPool m_ComputeCommandPool;

        VkCommandBuffer m_GraphicsCommandBuffer;
        VkCommandBuffer m_ComputeCommandBuffer;

        VkSampler m_LinearSampler;
        VkSampler m_NearestSampler;
    };

    bool VulkanInit(SDL_Window *pWindow, bool enableValidationLayers);

    VulkanDevice GetVulkanDevice();

//    extern bool g_EnableValidationLayers;
//    extern VkInstance g_Instance;
//    extern VkSurfaceKHR g_Surface;
//    extern VkPhysicalDevice g_PhysicalDevice;
//    extern VkPhysicalDeviceMeshShaderPropertiesEXT  g_PhysicalDeviceMeshShaderProperties;
//    extern VkPhysicalDeviceProperties2  g_PhysicalDeviceProperties;
//    extern VkPhysicalDeviceMemoryProperties g_PhysicalDeviceMemoryProperties;
//    extern VulkanDevice g_Device;
}