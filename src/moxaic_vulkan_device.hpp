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
        VulkanDevice();
        virtual ~VulkanDevice();

        bool Init();

        bool AllocateMemory(const VkMemoryPropertyFlags &properties,
                            VkImage image,
                            VkDeviceMemory &outDeviceMemory) const;

        bool BeginImmediateCommandBuffer(VkCommandBuffer &outCommandBuffer) const;

        bool EndImmediateCommandBuffer(const VkCommandBuffer &commandBuffer) const;

        inline auto vkDevice() const { return m_VkDevice; }
        inline auto vkPhysicalDevice() const { return m_VkPhysicalDevice; }
        inline auto vkGraphicsQueue() const { return m_VkGraphicsQueue; }
        inline auto vkComputeQueue() const { return m_VkComputeQueue; }
        inline auto vkRenderPass() const { return m_VkRenderPass; }
        inline auto vkDescriptorPool() const { return m_VkDescriptorPool; }
        inline auto vkQueryPool() const { return m_VkQueryPool; }
        inline auto vkGraphicsCommandPool() const { return m_VkGraphicsCommandPool; }
        inline auto vkComputeCommandPool() const { return m_VkComputeCommandPool; }
        inline auto vkGraphicsCommandBuffer() const { return m_VkGraphicsCommandBuffer; }
        inline auto vkComputeCommandBuffer() const { return m_VkComputeCommandBuffer; }
        inline auto vkLinearSampler() const { return m_VkLinearSampler; }
        inline auto vkNearestSampler() const { return m_VkNearestSampler; }

    private:
        VkDevice m_VkDevice{VK_NULL_HANDLE};

        VkPhysicalDevice m_VkPhysicalDevice{VK_NULL_HANDLE};

        VkQueue m_VkGraphicsQueue{VK_NULL_HANDLE};
        VkQueue m_VkComputeQueue{VK_NULL_HANDLE};

        VkRenderPass m_VkRenderPass{VK_NULL_HANDLE};

        VkDescriptorPool m_VkDescriptorPool{VK_NULL_HANDLE};
        VkQueryPool m_VkQueryPool{VK_NULL_HANDLE};
        VkCommandPool m_VkGraphicsCommandPool{VK_NULL_HANDLE};
        VkCommandPool m_VkComputeCommandPool{VK_NULL_HANDLE};

        VkCommandBuffer m_VkGraphicsCommandBuffer{VK_NULL_HANDLE};
        VkCommandBuffer m_VkComputeCommandBuffer{VK_NULL_HANDLE};

        VkSampler m_VkLinearSampler{VK_NULL_HANDLE};
        VkSampler m_VkNearestSampler{VK_NULL_HANDLE};

        uint32_t m_GraphicsQueueFamilyIndex{};
        uint32_t m_ComputeQueueFamilyIndex{};

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

    };
}
