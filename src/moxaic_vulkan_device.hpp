#pragma once

#include <vulkan/vulkan.hpp>

namespace Moxaic
{
    class VulkanDevice
    {
    public:
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
        virtual ~VulkanDevice();
        bool Init();

    private:
        VkInstance m_Instance;
        VkSurfaceKHR m_Surface;
        VkDevice m_Device;

        VkPhysicalDevice m_PhysicalDevice;

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

        VkPhysicalDeviceMeshShaderPropertiesEXT  m_PhysicalDeviceMeshShaderProperties;
        VkPhysicalDeviceProperties2  m_PhysicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;

        bool PickPhysicalDevice();
        bool FindQueues();
        bool CreateDevice();
        bool CreateRenderPass();
        bool CreateCommandBuffers();
        bool CreatePools();
        bool CreateSamplers();
    };
}
