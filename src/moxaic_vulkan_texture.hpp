#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Texture final
    {
    public:
        MXC_NO_VALUE_PASS(Texture);

        explicit Texture(const Device* device);
        ~Texture();

        MXC_RESULT InitFromFile(const char* file,
                                Locality locality);
        MXC_RESULT InitFromImport(VkFormat format,
                                  VkExtent2D extents,
                                  VkImageUsageFlags usage,
                                  VkImageAspectFlags aspectMask,
                                  HANDLE externalMemory);
        MXC_RESULT Init(VkFormat format,
                        VkExtent2D extents,
                        VkImageUsageFlags usage,
                        VkImageAspectFlags aspectMask,
                        Locality locality,
                        VkSampleCountFlagBits sampleCount);
        MXC_RESULT Init(VkFormat format,
                        VkExtent2D extents,
                        VkImageUsageFlags usage,
                        VkImageAspectFlags aspectMask,
                        Locality locality);

        MXC_RESULT TransitionInitialImmediate(PipelineType pipelineType) const;

        void BlitTo(VkCommandBuffer commandBuffer, const Texture& dstTexture) const;

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

        const auto& GetVkImage() const { return vkImage; }
        const auto& GetVkImageView() const { return vkImageView; }

        const auto GetExtents() const { return extents; }
        const auto GetMipLevels() const { return mipLevels; }

    private:
        const Device* const Device;

        uint32_t mipLevels{1};

        VkImage vkImage{VK_NULL_HANDLE};
        VkImageView vkImageView{VK_NULL_HANDLE};

        VkDeviceMemory vkDeviceMemory{VK_NULL_HANDLE};
        VkExtent2D extents{};
        VkImageAspectFlags aspectMask{};
        VkFormat format{};

#ifdef WIN32
        HANDLE externalHandle{};
#endif

        MXC_RESULT TransitionImmediateInitialToTransferDst() const;
        MXC_RESULT TransitionImmediateTransferDstToGraphicsRead() const;

        MXC_RESULT InitImage(VkFormat format,
                             VkExtent2D extents,
                             VkImageUsageFlags usage,
                             Locality locality);
        MXC_RESULT InitImage(VkFormat format,
                             VkExtent2D extents,
                             VkImageUsageFlags usage,
                             Locality locality,
                             VkSampleCountFlagBits sampleCount);
        MXC_RESULT InitImageView(VkFormat format,
                                 VkImageAspectFlags aspectMask);
    };
}
