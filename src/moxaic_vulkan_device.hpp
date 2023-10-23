#pragma once

#include <vulkan/vulkan.h>
#include "moxaic_vulkan.hpp"

namespace Moxaic
{
    class VulkanDevice
    {
    public:
        VulkanDevice(const VkInstance &instance,
                     const VkSurfaceKHR &surface);
        virtual ~VulkanDevice();
        bool Init();

        bool CreateImage(const VkImageCreateInfo &imageCreateInfo,
                         VkImage &outImage) const;

        bool AllocateMemory(const VkMemoryPropertyFlags &properties,
                            const VkImage &image,
                            VkDeviceMemory &outDeviceMemory) const;

        bool BindImageMemory(const VkImage &image,
                             const VkDeviceMemory &DeviceMemory) const
        { return vkBindImageMemory(m_Device, image, DeviceMemory, 0) == VK_SUCCESS; }

        bool CreateImageView(const VkImageViewCreateInfo &createInfo,
                             VkImageView &outImageView) const
        { return vkCreateImageView(m_Device, &createInfo, VK_ALLOC, &outImageView) == VK_SUCCESS; }

    private:
        const VkInstance &m_Instance;
        const VkSurfaceKHR &m_Surface;

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
