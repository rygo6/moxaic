#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>

#if WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

namespace Moxaic
{

    class VulkanDevice
    {
    public:
        explicit VulkanDevice(VkInstance instance, VkSurfaceKHR surface);

        virtual ~VulkanDevice();

        bool Init();

        bool AllocateMemory(const VkMemoryPropertyFlags &properties,
                            VkImage image,
                            VkDeviceMemory &outDeviceMemory) const;

    private:
        VkInstance m_Instance;
        VkSurfaceKHR m_Surface;

        VkDevice m_Device{VK_NULL_HANDLE};

        VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};

        VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
        uint32_t m_GraphicsQueueFamilyIndex{};

        VkQueue m_ComputeQueue{VK_NULL_HANDLE};
        uint32_t m_ComputeQueueFamilyIndex{};

        VkRenderPass m_RenderPass{VK_NULL_HANDLE};

        VkDescriptorPool m_DescriptorPool{VK_NULL_HANDLE};
        VkQueryPool m_QueryPool{VK_NULL_HANDLE};
        VkCommandPool m_GraphicsCommandPool{VK_NULL_HANDLE};
        VkCommandPool m_ComputeCommandPool{VK_NULL_HANDLE};

        VkCommandBuffer m_GraphicsCommandBuffer{VK_NULL_HANDLE};
        VkCommandBuffer m_ComputeCommandBuffer{VK_NULL_HANDLE};

        VkSampler m_LinearSampler{VK_NULL_HANDLE};
        VkSampler m_NearestSampler{VK_NULL_HANDLE};

        VkPhysicalDeviceMeshShaderPropertiesEXT m_PhysicalDeviceMeshShaderProperties{};
        VkPhysicalDeviceProperties2 m_PhysicalDeviceProperties{};
        VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties{};

        bool PickPhysicalDevice();
        bool FindQueues();
        bool CreateDevice();
        bool CreateRenderPass();
        bool CreateCommandBuffers();
        bool CreatePools();
        bool CreateSamplers();

    public: // vk inline
        VkResult VkCreateImage(const VkImageCreateInfo &imageCreateInfo, VkImage &outImage) const
        {
            return vkCreateImage(m_Device, &imageCreateInfo, VK_ALLOC, &outImage);
        }

        VkResult VkBindImageMemory(VkImage image, VkDeviceMemory DeviceMemory) const
        {
            return vkBindImageMemory(m_Device, image, DeviceMemory, 0);
        }

        VkResult VkCreateImageView(const VkImageViewCreateInfo &imageViewCreateInfo, VkImageView &outImageView) const
        {
            return vkCreateImageView(m_Device, &imageViewCreateInfo, VK_ALLOC, &outImageView);
        }

#if WIN32

        VkResult VkGetMemoryHandle(VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo, HANDLE &outHandle) const
        {
            return VkFunc.GetMemoryWin32HandleKHR(m_Device, &getWin32HandleInfo, &outHandle);
        }

#endif
    };

}
