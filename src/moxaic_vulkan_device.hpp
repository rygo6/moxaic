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
        MXC_RESULT AllocateBindImageImport(VkMemoryPropertyFlags const& properties,
                                           VkImage const& image,
                                           VkExternalMemoryHandleTypeFlagBits const& externalHandleType,
                                           HANDLE const& externalHandle,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImageExport(VkMemoryPropertyFlags const& properties,
                                           VkImage const& image,
                                           VkExternalMemoryHandleTypeFlags const& externalHandleType,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImage(VkMemoryPropertyFlags const& properties,
                                     VkImage const& image,
                                     VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateStagingBuffer(void const* srcData,
                                       VkDeviceSize const& bufferSize,
                                       VkBuffer* pStagingBuffer,
                                       VkDeviceMemory* pStagingBufferMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags const& usage,
                                            VkMemoryPropertyFlags const& properties,
                                            VkDeviceSize const& bufferSize,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags const& usage,
                                            VkMemoryPropertyFlags const& properties,
                                            VkDeviceSize const& bufferSize,
                                            Vulkan::Locality const& locality,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory,
                                            HANDLE* pExternalMemory) const;
        MXC_RESULT CreateAllocateBindPopulateBufferViaStaging(void const* srcData,
                                                              VkBufferUsageFlagBits const& usage,
                                                              VkDeviceSize const& bufferSize,
                                                              VkBuffer* pBuffer,
                                                              VkDeviceMemory* pBufferMemory) const;
        MXC_RESULT CopyBufferToBuffer(VkDeviceSize const& bufferSize,
                                      VkBuffer const& srcBuffer,
                                      VkBuffer const& dstBuffer) const;
        MXC_RESULT CopyBufferToImage(VkExtent2D const& imageExtent,
                                     VkBuffer const& srcBuffer,
                                     VkImage const& dstImage) const;
        MXC_RESULT TransitionImageLayoutImmediate(VkImage const& image,
                                                  VkImageLayout const& oldLayout,
                                                  VkImageLayout const& newLayout,
                                                  VkAccessFlags const& srcAccessMask,
                                                  VkAccessFlags const& dstAccessMask,
                                                  VkPipelineStageFlags const& srcStageMask,
                                                  VkPipelineStageFlags const& dstStageMask,
                                                  VkImageAspectFlags const& aspectMask) const;
        MXC_RESULT BeginImmediateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
        MXC_RESULT EndImmediateCommandBuffer(VkCommandBuffer const& commandBuffer) const;
        MXC_RESULT BeginGraphicsCommandBuffer() const;
        MXC_RESULT EndGraphicsCommandBuffer() const;
        void BeginRenderPass(Framebuffer const& framebuffer,
                             VkClearColorValue const& backgroundColor) const;
        void EndRenderPass() const;
        MXC_RESULT SubmitGraphicsQueueAndPresent(Semaphore& timelineSemaphore,
                                                 Swap const& swap) const;
        MXC_RESULT SubmitGraphicsQueue(Semaphore* pTimelineSemaphore) const;

        void ResetTimestamps() const;
        void WriteTimestamp(VkPipelineStageFlagBits const& pipelineStage,
                                    uint32_t const& query) const;
        StaticArray<double, QueryPoolCount> GetTimestamps() const;

        uint32_t GetQueue(Queue const queue) const
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
    };
}// namespace Moxaic::Vulkan
