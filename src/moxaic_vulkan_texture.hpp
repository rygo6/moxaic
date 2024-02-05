#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include <cassert>
#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Texture final
    {
        MXC_NO_VALUE_PASS(Texture);

        const Device* const Device;

        VkFormat format;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspectMask;
        VkSampleCountFlagBits sampleCount;
        VkExtent2D extents;
        uint32_t mipLevels;
        Locality locality;

        VkDeviceMemory vkDeviceMemory{VK_NULL_HANDLE};
        VkImage vkImageHandle{VK_NULL_HANDLE};
        VkImageView vkImageViewHandle{VK_NULL_HANDLE};

        // VkAccessFlags accessMask;
        // VkImageLayout layout;
        // Queue queueFamily;

#ifdef WIN32
        HANDLE externalHandle{nullptr};
#endif

    public:
        const uint32_t& MipLevels{mipLevels};
        const VkExtent2D& Extents{extents};

        const VkImage& VkImageHandle{vkImageHandle};
        const VkImageView& VkImageViewHandle{vkImageViewHandle};

        struct Info
        {
            VkFormat format{VK_FORMAT_UNDEFINED};
            VkImageUsageFlags usage{};
            VkImageAspectFlags aspectMask{VK_IMAGE_ASPECT_NONE};
            VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
            VkExtent2D extents{};
            uint32_t mipLevels{1};
            Locality locality{Locality::Local};
        };

        explicit Texture(const Vulkan::Device* device);
        explicit Texture(const Vulkan::Device* device, const Info& info);
        ~Texture();

        MXC_RESULT InitFromFile(const char* file);
        MXC_RESULT InitFromImport(HANDLE externalHandle,
                                  const VkExtent2D& extents);
        MXC_RESULT Init(const VkExtent2D& extents);
        MXC_RESULT Init(const Info& args);

        MXC_RESULT TransitionInitialImmediate(PipelineType pipelineType) const;
        MXC_RESULT TransitionImmediate(const Barrier2& src, const Barrier2& dst) const;

        void BlitTo(VkCommandBuffer commandBuffer,
                    const Texture& dstTexture) const;
        void CopyViaBufferTo(VkCommandBuffer commandBuffer,
                             const Texture& dstTexture) const;

        VkImageMemoryBarrier2 GetImageBarrier(const Barrier2& src, const Barrier2& dst) const
        {
            return (VkImageMemoryBarrier2){
              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .srcStageMask = src.GetStageMask(aspectMask),
              .srcAccessMask = src.GetAccessMask(aspectMask),
              .dstStageMask = dst.GetStageMask(aspectMask),
              .dstAccessMask = dst.GetAccessMask(aspectMask),
              .oldLayout = src.layout,
              .newLayout = dst.layout,
              .srcQueueFamilyIndex = Device->GetSrcQueue(src),
              .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
              .image = vkImageHandle,
              .subresourceRange = GetSubresourceRange(),
            };
        }

        HANDLE ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const;

        constexpr VkImageSubresourceRange GetSubresourceRange() const
        {
            return {
              .aspectMask = aspectMask,
              .baseMipLevel = 0,
              .levelCount = mipLevels,
              .baseArrayLayer = 0,
              .layerCount = 1,
            };
        }

        bool IsDepth() const
        {
            return aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT;
        }

    private:
        MXC_RESULT TransitionImmediateInitialToTransferDst() const;
        MXC_RESULT TransitionImmediateTransferDstToGraphicsRead() const;

        MXC_RESULT InternalInit();

        // todo InitImage maybe go in VKDevice as CreateAllocateImage to reflect CreateAllocateBuffer
        MXC_RESULT InitImage();
        MXC_RESULT InitImageView();
    };
}// namespace Moxaic::Vulkan
