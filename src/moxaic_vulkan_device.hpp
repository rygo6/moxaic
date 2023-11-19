#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <array>

#if WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#include <vector>
#endif

namespace Moxaic::Vulkan
{
    class Framebuffer;
    class Swap;
    class Semaphore;

    class Device
    {
    public:
        Device();
        virtual ~Device();

        MXC_RESULT Init();
        MXC_RESULT AllocateBindImageImport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlagBits externalHandleType,
                                           const HANDLE externalHandle,
                                           VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT AllocateBindImageExport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlags externalHandleType,
                                           VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT AllocateBindImage(const VkMemoryPropertyFlags properties,
                                     const VkImage image,
                                     VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT CreateStagingBuffer(const void *srcData,
                                       const VkDeviceSize bufferSize,
                                       VkBuffer &outStagingBuffer,
                                       VkDeviceMemory &outStagingBufferMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(const VkBufferUsageFlags usage,
                                            const VkMemoryPropertyFlags properties,
                                            const VkDeviceSize bufferSize,
                                            VkBuffer &outBuffer,
                                            VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(const VkBufferUsageFlags usage,
                                            const VkMemoryPropertyFlags properties,
                                            const VkDeviceSize bufferSize,
                                            const Vulkan::Locality locality,
                                            VkBuffer &outBuffer,
                                            VkDeviceMemory &outDeviceMemory,
                                            HANDLE &outExternalMemory) const;
        MXC_RESULT CreateAllocateBindPopulateBufferViaStaging(const void *srcData,
                                                              const VkBufferUsageFlagBits usage,
                                                              const VkDeviceSize bufferSize,
                                                              VkBuffer &outBuffer,
                                                              VkDeviceMemory &outBufferMemory) const;
        MXC_RESULT CopyBufferToBuffer(const VkDeviceSize bufferSize,
                                      const VkBuffer srcBuffer,
                                      VkBuffer dstBuffer) const;
        MXC_RESULT CopyBufferToImage(const VkExtent2D imageExtent,
                                     const VkBuffer srcBuffer,
                                     VkImage dstImage) const;
        MXC_RESULT TransitionImageLayoutImmediate(VkImage image,
                                                  VkImageLayout oldLayout,
                                                  VkImageLayout newLayout,
                                                  VkAccessFlags srcAccessMask,
                                                  VkAccessFlags dstAccessMask,
                                                  VkPipelineStageFlags srcStageMask,
                                                  VkPipelineStageFlags dstStageMask,
                                                  VkImageAspectFlags aspectMask) const;
        MXC_RESULT BeginImmediateCommandBuffer(VkCommandBuffer &outCommandBuffer) const;
        MXC_RESULT EndImmediateCommandBuffer(const VkCommandBuffer &commandBuffer) const;
        MXC_RESULT BeginGraphicsCommandBuffer() const;
        MXC_RESULT EndGraphicsCommandBuffer() const;
        MXC_RESULT BeginRenderPass(const Framebuffer &framebuffer) const;
        MXC_RESULT EndRenderPass() const;
        MXC_RESULT SubmitGraphicsQueueAndPresent(Semaphore &timelineSemaphore,
                                                 const Swap &swap) const;

        // VulkanHandles are not encapsulated. Deal with vk vars and methods with care.
        // Reason? Vulkan safety is better enforced by validation layers, not C++, and
        // the attempt to encapsulate these to more so rely on C++ type safety ends up
        // with something more complex and convoluted that doesn't add much over Vulkan
        // Validation layers. Classes are more to organize relationship of VulkanHandles.
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

        inline auto graphicsQueueFamilyIndex() const { return m_GraphicsQueueFamilyIndex; }
        inline auto computeQueueFamilyIndex() const { return m_ComputeQueueFamilyIndex; }

        inline const auto &physicalDeviceMeshShaderProperties() const { return m_PhysicalDeviceMeshShaderProperties; }
        inline const auto &physicalDeviceProperties() const { return m_PhysicalDeviceProperties; }
        inline const auto &physicalDeviceMemoryProperties() const { return m_PhysicalDeviceMemoryProperties; }

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
