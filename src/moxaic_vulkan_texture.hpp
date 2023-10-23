#pragma once

#include <vulkan/vulkan.hpp>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class VulkanDevice;

    class VulkanTexture
    {
    public:
        VulkanTexture(const VulkanDevice &Device);

        virtual ~VulkanTexture();

        bool CreateFromImage(VkFormat format,
                             VkExtent2D extent,
                             VkImage image);

        bool CreateFromFile(bool external,
                            char const *filename);

        bool Import(VkFormat format,
                    VkExtent2D extent,
                    VkImageUsageFlags usage,
                    VkImageAspectFlags aspectMask,
                    HANDLE externalMemory);

        bool Create(VkFormat format,
                    VkExtent2D extent,
                    VkImageUsageFlags usage,
                    VkImageAspectFlags aspectMask,
                    bool external);

        void Destroy();

    private:
        const VulkanDevice &m_Device;
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory n_DeviceMemory;
        VkExtent2D m_Extent;
#ifdef WIN32
        HANDLE m_ExternalMemory;
#endif
    };
}
