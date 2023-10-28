#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <array>

#if WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#include <vector>
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

        bool BeginImmediateCommandBuffer(VkCommandBuffer &outCommandBuffer) const;

        bool EndImmediateCommandBuffer(const VkCommandBuffer &commandBuffer) const;

    public: // getters

        inline VkRenderPass vkRenderPass() const
        { return m_VkRenderPass; }

    private:
        VkInstance m_VkInstance;
        VkSurfaceKHR m_VkSurface;

        VkDevice m_VkDevice{VK_NULL_HANDLE};

        VkPhysicalDevice m_VkPhysicalDevice{VK_NULL_HANDLE};

        VkQueue m_VkGraphicsQueue{VK_NULL_HANDLE};
        uint32_t m_GraphicsQueueFamilyIndex{};

        VkQueue m_VkComputeQueue{VK_NULL_HANDLE};
        uint32_t m_ComputeQueueFamilyIndex{};

        VkRenderPass m_VkRenderPass{VK_NULL_HANDLE};

        VkDescriptorPool m_VkDescriptorPool{VK_NULL_HANDLE};
        VkQueryPool m_VkQueryPool{VK_NULL_HANDLE};
        VkCommandPool m_VkGraphicsCommandPool{VK_NULL_HANDLE};
        VkCommandPool m_VkComputeCommandPool{VK_NULL_HANDLE};

        VkCommandBuffer m_VkGraphicsCommandBuffer{VK_NULL_HANDLE};
        VkCommandBuffer m_VkComputeCommandBuffer{VK_NULL_HANDLE};

        VkSampler m_VkLinearSampler{VK_NULL_HANDLE};
        VkSampler m_VkNearestSampler{VK_NULL_HANDLE};

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

        // not device on if I like this or should just be getters to all the vk handles...
        VkResult VkCreateImage(const VkImageCreateInfo &imageCreateInfo, VkImage &outImage) const
        {
            return vkCreateImage(m_VkDevice, &imageCreateInfo, VK_ALLOC, &outImage);
        }

        VkResult VkBindImageMemory(VkImage image, VkDeviceMemory DeviceMemory) const
        {
            return vkBindImageMemory(m_VkDevice, image, DeviceMemory, 0);
        }

        VkResult VkCreateImageView(const VkImageViewCreateInfo &imageViewCreateInfo, VkImageView &outImageView) const
        {
            return vkCreateImageView(m_VkDevice, &imageViewCreateInfo, VK_ALLOC, &outImageView);
        }

        VkResult VkCreateFramebuffer(const VkFramebufferCreateInfo &createInfo, VkFramebuffer &outFramebuffer) const
        {
            return (vkCreateFramebuffer(m_VkDevice, &createInfo, VK_ALLOC, &outFramebuffer));
        }

        VkResult VkCreateSemaphore(const VkSemaphoreCreateInfo &createInfo, VkSemaphore &outSemaphore) const
        {
            return vkCreateSemaphore(m_VkDevice, &createInfo, VK_ALLOC, &outSemaphore);
        }

#if WIN32

        VkResult VkGetMemoryHandle(VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo, HANDLE &outHandle) const
        {
            return VkFunc.GetMemoryWin32HandleKHR(m_VkDevice, &getWin32HandleInfo, &outHandle);
        }

#endif
    };
}
