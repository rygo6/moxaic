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
        MXC_RESULT AllocateBindImageImport(VkMemoryPropertyFlags properties,
                                           VkImage image,
                                           VkExternalMemoryHandleTypeFlagBits externalHandleType,
                                           HANDLE externalHandle,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImageExport(VkMemoryPropertyFlags properties,
                                           VkImage image,
                                           VkExternalMemoryHandleTypeFlags externalHandleType,
                                           VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT AllocateBindImage(VkMemoryPropertyFlags properties,
                                     VkImage image,
                                     VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateAndPopulateStagingBuffer(const void* srcData,
                                       VkDeviceSize bufferSize,
                                       VkBuffer* pStagingBuffer,
                                       VkDeviceMemory* pStagingBufferMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties,
                                            VkDeviceSize bufferSize,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory) const;
        MXC_RESULT CreateAllocateBindBuffer(VkBufferUsageFlags usage,
                                            VkMemoryPropertyFlags properties,
                                            VkDeviceSize bufferSize,
                                            Vulkan::Locality locality,
                                            VkBuffer* pBuffer,
                                            VkDeviceMemory* pDeviceMemory,
                                            HANDLE* pExternalMemory) const;
        MXC_RESULT CreateAllocateBindPopulateBufferViaStaging(const void* srcData,
                                                              VkBufferUsageFlagBits usage,
                                                              VkDeviceSize bufferSize,
                                                              VkBuffer* pBuffer,
                                                              VkDeviceMemory* pBufferMemory) const;
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
        MXC_RESULT TransitionImageLayoutImmediate(VkImage image,
                                                  const Barrier& src,
                                                  const Barrier& dst,
                                                  VkImageAspectFlags aspectMask) const;
        MXC_RESULT BeginImmediateCommandBuffer(VkCommandBuffer* pCommandBuffer) const; // todo change to return commandbuffer
        MXC_RESULT EndImmediateCommandBuffer(VkCommandBuffer commandBuffer) const;
        VkCommandBuffer BeginGraphicsCommandBuffer() const;
        void BeginRenderPass(const Framebuffer& framebuffer,
                             const VkClearColorValue& backgroundColor) const;
        MXC_RESULT SubmitGraphicsQueueAndPresent(const Swap& swap,
                                                 uint32_t swapIndex,
                                                 Semaphore* pTimelineSemaphore) const;
        MXC_RESULT SubmitGraphicsQueue(Semaphore* pTimelineSemaphore) const;
        VkCommandBuffer BeginComputeCommandBuffer() const;
        bool SubmitComputeQueue(Semaphore* pTimelineSemaphore) const;
        MXC_RESULT SubmitComputeQueueAndPresent(VkCommandBuffer commandBuffer,
                                                const Swap& swap,
                                                uint32_t swapIndex,
                                                Semaphore* pTimelineSemaphore) const;

        void ResetTimestamps() const;
        void WriteTimestamp(VkCommandBuffer commandbuffer,
                            VkPipelineStageFlagBits pipelineStage,
                            uint32_t query) const;
        StaticArray<double, QueryPoolCount> GetTimestamps() const;

        uint32_t GetSrcQueue(const Barrier src) const
        {
            switch (src.queueFamilyIndex) {
                case Queue::None:
                    return 0;
                case Queue::Graphics:
                    return graphicsQueueFamilyIndex;
                case Queue::Compute:
                    return computeQueueFamilyIndex;
                case Queue::FamilyExternal:
                    return VK_QUEUE_FAMILY_EXTERNAL;
                case Queue::Ignore:
                    return VK_QUEUE_FAMILY_IGNORED;
                default:
                    SDL_assert(false && "Unknown Queue Type");
                    return -1;
            }
        }

        uint32_t GetDstQueue(const Barrier src, const Barrier dst) const
        {
            // This logic is needed because if src queue is ignored, then both must be ignored
            if (src.queueFamilyIndex == Queue::Ignore) {
                return VK_QUEUE_FAMILY_IGNORED;
            }

            return GetSrcQueue(dst);
        }

        // vkHandles return by ref so they dan be used in designated initializers
        const auto& GetVkDevice() const { return vkDevice; }
        const auto& GetVkPhysicalDevice() const { return vkPhysicalDevice; }
        const auto& GetVkRenderPass() const { return vkRenderPass; }
        const auto& GetVkDescriptorPool() const { return vkDescriptorPool; }
        const auto& GetVkLinearSampler() const { return vkLinearSampler; }

        const auto GetGraphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex; }

    private:
        VkDevice vkDevice{VK_NULL_HANDLE};

        VkPhysicalDevice vkPhysicalDevice{VK_NULL_HANDLE};

        VkQueue vkGraphicsQueue{VK_NULL_HANDLE};
        VkQueue vkComputeQueue{VK_NULL_HANDLE};

        VkRenderPass vkRenderPass{VK_NULL_HANDLE};

        VkDescriptorPool vkDescriptorPool{VK_NULL_HANDLE};
        VkQueryPool vkQueryPool{VK_NULL_HANDLE};
        VkCommandPool vkGraphicsCommandPool{VK_NULL_HANDLE};
        VkCommandPool vkComputeCommandPool{VK_NULL_HANDLE};

        VkCommandBuffer vkGraphicsCommandBuffer{VK_NULL_HANDLE};
        VkCommandBuffer vkComputeCommandBuffer{VK_NULL_HANDLE};

        VkSampler vkLinearSampler{VK_NULL_HANDLE};
        VkSampler vkNearestSampler{VK_NULL_HANDLE};
        VkSampler vkMinSampler{VK_NULL_HANDLE};

        uint32_t graphicsQueueFamilyIndex{};
        uint32_t computeQueueFamilyIndex{};

        VkPhysicalDeviceProperties2 physicalDeviceProperties{};
        VkPhysicalDeviceMeshShaderPropertiesEXT pysicalDeviceMeshShaderProperties{};
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties{};

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
                                         uint32_t swapIndex,
                                         const VkPipelineStageFlags& waitDstStageMask,
                                         Semaphore* pTimelineSemaphore) const;
        MXC_RESULT SubmitQueue(const VkCommandBuffer& commandBuffer,
                               const VkQueue& queue,
                               const VkPipelineStageFlags& waitDstStageMask,
                               Semaphore* pTimelineSemaphore) const;
    };
}
