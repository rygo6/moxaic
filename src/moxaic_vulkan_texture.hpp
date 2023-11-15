#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <string>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class VulkanDevice;

    class VulkanTexture
    {
    public:
        explicit VulkanTexture(const VulkanDevice &device);
        virtual ~VulkanTexture();

        MXC_RESULT InitFromImage(VkFormat format,
                                 VkExtent2D extent,
                                 VkImage image);

        MXC_RESULT InitFromFile(const std::string file,
                                const Vulkan::Locality locality);

        MXC_RESULT InitFromImport(VkFormat format,
                                  VkExtent2D extent,
                                  VkImageUsageFlags usage,
                                  VkImageAspectFlags aspectMask,
                                  HANDLE externalMemory);

        MXC_RESULT Init(const VkFormat format,
                        const VkExtent2D extents,
                        const VkImageUsageFlags usage,
                        const VkImageAspectFlags aspectMask,
                        const Vulkan::Locality locality);

        MXC_RESULT TransitionImmediateInitialToGraphicsRead();

        inline auto vkImage() const { return m_VkImage; }
        inline auto vkImageView() const { return m_VkImageView; }
        inline auto vkDeviceMemory() const { return m_VkDeviceMemory; }

    private:
        const VulkanDevice &k_Device;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};

        VkExtent2D m_Dimensions{};
        VkImageAspectFlags m_AspectMask{};

#ifdef WIN32
        HANDLE m_ExternalMemory{};
#endif
        bool TransitionImmediateInitialToTransferDst();
        bool TransitionImmediateTransferDstToGraphicsRead();
    };
}
