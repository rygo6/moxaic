#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>

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

        bool InitFromImage(VkFormat format,
                           VkExtent2D extent,
                           VkImage image);

        bool InitFromFile(bool external,
                          char const *filename);

        bool InitFromImport(VkFormat format,
                            VkExtent2D extent,
                            VkImageUsageFlags usage,
                            VkImageAspectFlags aspectMask,
                            HANDLE externalMemory);

        bool Init(const VkFormat &format,
                  const VkExtent3D &extent,
                  const VkImageUsageFlags &usage,
                  const VkImageAspectFlags &aspectMask,
                  const BufferLocality &locality);

        void Cleanup();

        bool TransitionImageLayoutImmediate(VkImageLayout oldLayout,
                                            VkImageLayout newLayout,
                                            VkAccessFlags srcAccessMask,
                                            VkAccessFlags dstAccessMask,
                                            VkPipelineStageFlags srcStageMask,
                                            VkPipelineStageFlags dstStageMask,
                                            VkImageAspectFlags aspectMask) const;

        inline auto vkImage() const { return m_VkImage; }
        inline auto vkImageView() const { return m_VkImageView; }
        inline auto vkDeviceMemory() const { return m_VkDeviceMemory; }

    private:
        const VulkanDevice &k_Device;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};

        VkExtent3D m_Extent{};

#ifdef WIN32
        HANDLE m_ExternalMemory{};
#endif
    };
}
