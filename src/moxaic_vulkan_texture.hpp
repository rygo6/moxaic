#pragma once

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
        VulkanTexture(const VulkanDevice &Device);

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

        bool Init(VkFormat format,
                  VkExtent3D extent,
                  VkImageUsageFlags usage,
                  VkImageAspectFlags aspectMask);

        bool InitExternal(VkFormat format,
                          VkExtent3D extent,
                          VkImageUsageFlags usage,
                          VkImageAspectFlags aspectMask);

        void Cleanup();

    private:
        const VulkanDevice &m_Device;
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory m_DeviceMemory;
        VkExtent3D m_Extent;
#ifdef WIN32
        HANDLE m_ExternalMemory;
#endif


    };
}
