#pragma once

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class VulkanDevice;

    enum TextureLocality : char {
        LocalTexture,
        ExternalTexture,
    };

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

        bool Init(VkFormat format,
                  VkExtent3D extent,
                  VkImageUsageFlags usage,
                  VkImageAspectFlags aspectMask,
                  TextureLocality locality);

        void Cleanup();

    private:
        const VulkanDevice &k_Device;
        VkImage m_Image{VK_NULL_HANDLE};
        VkImageView m_ImageView{VK_NULL_HANDLE};
        VkDeviceMemory m_DeviceMemory{VK_NULL_HANDLE};
        VkExtent3D m_Extent{};
#ifdef WIN32
        HANDLE m_ExternalMemory{};
#endif
    };

    static inline const char* string_TextureLocality(TextureLocality input_value) {
        switch (input_value) {
            case LocalTexture:
                return "LocalTexture";
            case ExternalTexture:
                return "ExternalTexture";
            default:
                return "Unhandled TextureLocality";
        }
    }
}
