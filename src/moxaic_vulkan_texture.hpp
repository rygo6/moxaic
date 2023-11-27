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

        explicit Texture(const Device& device);
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
                        const Locality locality);

        MXC_RESULT TransitionInitialImmediate(const PipelineType pipelineType) const;

        void BlitTo(VkCommandBuffer commandBuffer, const Texture& dstTexture) const;

        HANDLE ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const;

        bool IsDepth() const
        {
            return m_Format == kDepthBufferFormat;
        }

        MXC_GET(VkImage);
        MXC_GET(VkImageView);

    private:
        const Device* const k_pDevice;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};

        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        VkExtent2D m_Extents{};
        VkImageAspectFlags m_AspectMask{};
        VkFormat m_Format{};

#ifdef WIN32
        HANDLE m_ExternalHandle{};
#endif

        MXC_RESULT TransitionImmediateInitialToTransferDst() const;
        MXC_RESULT TransitionImmediateTransferDstToGraphicsRead() const;

        MXC_RESULT InitImage(const VkFormat format,
                             const VkExtent2D extents,
                             const VkImageUsageFlags usage,
                             const Locality locality);
        MXC_RESULT InitImageView(const VkFormat& format,
                                 const VkImageAspectFlags& aspectMask);
    };
}// namespace Moxaic::Vulkan
