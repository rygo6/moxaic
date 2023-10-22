#pragma once

#include <vulkan/vulkan.hpp>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class VulkanTexture {
        VulkanTexture();
        virtual ~VulkanTexture();

    private:
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory n_DeviceMemory;
        VkExtent2D m_Extent;
#ifdef WIN32
        HANDLE m_ExternalMemory;
#endif
    };
}
