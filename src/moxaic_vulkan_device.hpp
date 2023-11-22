#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>

#if WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan {
    class Framebuffer;
    class Swap;
    class Semaphore;

    class Device {
    public:
        MXC_NO_VALUE_PASS(Device);

        Device() = default;
        virtual ~Device();

        MXC_RESULT Init();
        MXC_RESULT AllocateBindImageImport(VkMemoryPropertyFlags properties,
                                           VkImage image,
                                           VkExternalMemoryHandleTypeFlagBits externalHandleType,
                                           HANDLE externalHandle,
                                           VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT AllocateBindImageExport(VkMemoryPropertyFlags properties,
                                           VkImage image,
                                           VkExternalMemoryHandleTypeFlags externalHandleType,
                                           VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT AllocateBindImage(VkMemoryPropertyFlags properties,
                                     VkImage image,
                                     VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT CreateStagingBuffer(const void *srcData,
                                       VkDeviceSize bufferSize,
                                       VkBuffer &outStagingBuffer,
                                       VkDeviceMemory &outStagingBufferMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties,
                                            VkDeviceSize bufferSize,
                                            VkBuffer &outBuffer,
                                            VkDeviceMemory &outDeviceMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties,
                                            VkDeviceSize bufferSize,
                                            Vulkan::Locality locality,
                                            VkBuffer &outBuffer,
                                            VkDeviceMemory &outDeviceMemory,
                                            HANDLE &outExternalMemory) const;
        MXC_RESULT CreateAllocateBindPopulateBufferViaStaging(const void *srcData,
                                                              VkBufferUsageFlagBits usage,
                                                              VkDeviceSize bufferSize,
                                                              VkBuffer &outBuffer,
                                                              VkDeviceMemory &outBufferMemory) const;
        MXC_RESULT CopyBufferToBuffer(VkDeviceSize bufferSize,
                                      VkBuffer srcBuffer,
                                      VkBuffer dstBuffer) const;
        MXC_RESULT CopyBufferToImage(VkExtent2D imageExtent,
                                     VkBuffer srcBuffer,
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
        void BeginRenderPass(const Framebuffer &framebuffer,
                             const VkClearColorValue &backgroundColor) const;
        void EndRenderPass() const;
        MXC_RESULT SubmitGraphicsQueueAndPresent(Semaphore &timelineSemaphore,
                                                 const Swap &swap) const;
        MXC_RESULT SubmitGraphicsQueue(Semaphore &timelineSemaphore) const;

        uint32_t GetQueue(const Queue queue) const {
            switch (queue) {
                case Queue::None:
                    return 0;
                case Queue::Graphics:
                    return m_GraphicsQueueFamilyIndex;
                case Queue::Compute:
                    return m_ComputeQueueFamilyIndex;
                case Queue::FamilyExternal:
                    return VK_QUEUE_FAMILY_EXTERNAL;
                default:;
            }
        }

        // VulkanHandles are not encapsulated. Deal with vk vars and methods with care.
        // Reason? Vulkan safety is better enforced by validation layers, not C++, and
        // the attempt to encapsulate these to more so rely on C++ type safety ends up
        // with something more complex and convoluted that doesn't add much over Vulkan
        // Validation layers. Classes are more to organize relationship of VulkanHandles.
        // Return vkHandles via const ref even though they are single pointers so that
        // the pointer can be taken to them in designated initializers without having to first
        // make a stack copy
        MXC_GET(vkDevice);
        MXC_GET(vkPhysicalDevice);
        MXC_GET(vkGraphicsQueue);
        MXC_GET(vkComputeQueue);
        MXC_GET(vkRenderPass);
        MXC_GET(vkDescriptorPool);
        MXC_GET(vkGraphicsCommandBuffer);
        MXC_GET(vkComputeCommandBuffer);
        MXC_GET(vkLinearSampler);
        MXC_GET(vkNearestSampler);

        MXC_GET(GraphicsQueueFamilyIndex);
        MXC_GET(ComputeQueueFamilyIndex);

        MXC_GET(PhysicalDeviceMeshShaderProperties);
        MXC_GET(PhysicalDeviceProperties);
        MXC_GET(PhysicalDeviceMemoryProperties);

    private:
        VkDevice m_vkDevice{VK_NULL_HANDLE};

        VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};

        VkQueue m_vkGraphicsQueue{VK_NULL_HANDLE};
        VkQueue m_vkComputeQueue{VK_NULL_HANDLE};

        VkRenderPass m_vkRenderPass{VK_NULL_HANDLE};

        VkDescriptorPool m_vkDescriptorPool{VK_NULL_HANDLE};
        VkQueryPool m_VkQueryPool{VK_NULL_HANDLE};
        VkCommandPool m_VkGraphicsCommandPool{VK_NULL_HANDLE};
        VkCommandPool m_VkComputeCommandPool{VK_NULL_HANDLE};

        VkCommandBuffer m_vkGraphicsCommandBuffer{VK_NULL_HANDLE};
        VkCommandBuffer m_vkComputeCommandBuffer{VK_NULL_HANDLE};

        VkSampler m_vkLinearSampler{VK_NULL_HANDLE};
        VkSampler m_vkNearestSampler{VK_NULL_HANDLE};

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
}// namespace Moxaic::Vulkan
