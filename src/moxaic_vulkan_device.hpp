#pragma once

#include "moxaic_vulkan.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

#if WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Framebuffer;
    class Swap;
    class Semaphore;

    class Device
    {
    public:
        MXC_NO_VALUE_PASS(Device);

        constexpr static uint32_t QueryPoolCount = 2;

        Device() = default;
        virtual ~Device();

        MXC_RESULT Init();
        MXC_RESULT AllocateBindImageImport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlagBits externalHandleType,
                                           HANDLE const externalHandle,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImageExport(const VkMemoryPropertyFlags properties,
                                           const VkImage image,
                                           const VkExternalMemoryHandleTypeFlags externalHandleType,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImage(const VkMemoryPropertyFlags properties,
                                     const VkImage image,
                                     VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateStagingBuffer(const void* srcData,
                                       const VkDeviceSize& bufferSize,
                                       VkBuffer* pStagingBuffer,
                                       VkDeviceMemory* pStagingBufferMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(const VkBufferUsageFlags& usage,
                                            const VkMemoryPropertyFlags& properties,
                                            const VkDeviceSize& bufferSize,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(const VkBufferUsageFlags& usage,
                                            const VkMemoryPropertyFlags& properties,
                                            const VkDeviceSize& bufferSize,
                                            const Vulkan::Locality& locality,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory,
                                            HANDLE* pExternalMemory) const;
        MXC_RESULT CreateAllocateBindPopulateBufferViaStaging(const void* srcData,
                                                              const VkBufferUsageFlagBits& usage,
                                                              const VkDeviceSize& bufferSize,
                                                              VkBuffer* pBuffer,
                                                              VkDeviceMemory* pBufferMemory) const;
        MXC_RESULT CopyBufferToBuffer(const VkDeviceSize& bufferSize,
                                      const VkBuffer& srcBuffer,
                                      const VkBuffer& dstBuffer) const;
        MXC_RESULT CopyBufferToImage(const VkExtent2D& imageExtent,
                                     const VkBuffer& srcBuffer,
                                     const VkImage& dstImage) const;
        MXC_RESULT TransitionImageLayoutImmediate(const VkImage& image,
                                                  const VkImageLayout& oldLayout,
                                                  const VkImageLayout& newLayout,
                                                  const VkAccessFlags& srcAccessMask,
                                                  const VkAccessFlags& dstAccessMask,
                                                  const VkPipelineStageFlags& srcStageMask,
                                                  const VkPipelineStageFlags& dstStageMask,
                                                  const VkImageAspectFlags& aspectMask) const;
        MXC_RESULT BeginImmediateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
        MXC_RESULT EndImmediateCommandBuffer(const VkCommandBuffer& commandBuffer) const;
        MXC_RESULT BeginGraphicsCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
        void BeginRenderPass(const Framebuffer& framebuffer,
                             const VkClearColorValue& backgroundColor) const;
        MXC_RESULT SubmitGraphicsQueueAndPresent(const Swap& swap,
                                                 const uint32_t swapIndex,
                                                 Semaphore* const pTimelineSemaphore) const;
        MXC_RESULT SubmitGraphicsQueue(Semaphore* pTimelineSemaphore) const;
        MXC_RESULT BeginComputeCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
        bool SubmitComputeQueue(Semaphore* pTimelineSemaphore) const;
        MXC_RESULT SubmitComputeQueueAndPresent(const VkCommandBuffer commandBuffer,
                                                const Swap& swap,
                                                const uint32_t swapIndex,
                                                Semaphore* pTimelineSemaphore) const;

        void ResetTimestamps() const;
        void WriteTimestamp(const VkCommandBuffer commandbuffer,
                            const VkPipelineStageFlagBits pipelineStage,
                            const uint32_t query) const;
        StaticArray<double, QueryPoolCount> GetTimestamps() const;

        uint32_t GetQueue(const Queue queue) const
        {
            switch (queue) {
                case Queue::None:
                    return 0;
                case Queue::Graphics:
                    return m_GraphicsQueueFamilyIndex;
                case Queue::Compute:
                    return m_ComputeQueueFamilyIndex;
                case Queue::FamilyExternal:
                    return VK_QUEUE_FAMILY_EXTERNAL;
                default:
                    SDL_assert(false && "Unknown Queue Type");
                    return -1;
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
        MXC_GET(VkDevice);
        MXC_GET(VkPhysicalDevice);
        MXC_GET(VkGraphicsQueue);
        MXC_GET(VkComputeQueue);
        MXC_GET(VkRenderPass);
        MXC_GET(VkDescriptorPool);
        MXC_GET(VkQueryPool);
        MXC_GET(VkGraphicsCommandBuffer);
        MXC_GET(VkComputeCommandBuffer);
        MXC_GET(VkLinearSampler);
        MXC_GET(VkNearestSampler);

        MXC_GET(GraphicsQueueFamilyIndex);
        MXC_GET(ComputeQueueFamilyIndex);

        MXC_GET(PhysicalDeviceMeshShaderProperties);
        MXC_GET(PhysicalDeviceProperties);
        MXC_GET(PhysicalDeviceMemoryProperties);

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

        MXC_RESULT SubmitQueueAndPresent(const VkCommandBuffer& commandBuffer,
                                         const VkQueue& queue,
                                         const Swap& swap,
                                         const uint32_t swapIndex,
                                         const VkPipelineStageFlags& waitDstStageMask, Semaphore* const pTimelineSemaphore) const;
        MXC_RESULT SubmitQueue(const VkCommandBuffer& commandBuffer,
                               const VkQueue& queue,
                               const VkPipelineStageFlags& waitDstStageMask,
                               Semaphore* const pTimelineSemaphore) const;
    };
}// namespace Moxaic::Vulkan
