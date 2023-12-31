#pragma once

#include "moxaic_vulkan.hpp"

#include <string>
#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Texture
    {
    public:
        MXC_NO_VALUE_PASS(Texture);

        explicit Texture(const Vulkan::Device* const device);
        virtual ~Texture();

        MXC_RESULT InitFromFile(const std::string& file,
                                const Locality locality);
        MXC_RESULT InitFromImport(const VkFormat format,
                                  const VkExtent2D extents,
                                  const VkImageUsageFlags usage,
                                  const VkImageAspectFlags aspectMask,
                                  const HANDLE externalMemory);
        MXC_RESULT Init(const VkFormat format,
                        const VkExtent2D extents,
                        const VkImageUsageFlags usage,
                        const VkImageAspectFlags aspectMask,
                        const Locality locality,
                        const VkSampleCountFlagBits sampleCount);
        MXC_RESULT Init(const VkFormat format,
                         const VkExtent2D extents,
                         const VkImageUsageFlags usage,
                         const VkImageAspectFlags aspectMask,
                         const Locality locality);

        MXC_RESULT TransitionInitialImmediate(const PipelineType pipelineType) const;

        void BlitTo(VkCommandBuffer commandBuffer, const Texture& dstTexture) const;

        HANDLE ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const;

        bool IsDepth() const
        {
            return format == kDepthBufferFormat;
        }

        const auto& GetVkImage() const { return vkImage; }
        const auto& GetVkImageView() const { return vkImageView; }
        const auto GetExtents() const { return extents; }

    private:
        const Vulkan::Device* const Device;

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


        MXC_RESULT InitImage(const VkFormat format,
                             const VkExtent2D extents,
                             const VkImageUsageFlags usage,
                             const Locality locality);
        MXC_RESULT InitImage(const VkFormat format,
                             const VkExtent2D extents,
                             const VkImageUsageFlags usage,
                             const Locality locality,
                             const VkSampleCountFlagBits sampleCount);
        MXC_RESULT InitImageView(const VkFormat& format,
                                 const VkImageAspectFlags& aspectMask);
    };
}// namespace Moxaic::Vulkan
