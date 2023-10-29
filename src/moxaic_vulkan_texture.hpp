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

    private:
        const VulkanDevice &k_Device;
        VkExtent3D m_Extent{};

#ifdef WIN32
        HANDLE m_ExternalMemory{};
#endif


    public:
#define VK_HANDLES \
        VK_HANDLE(,Image) \
        VK_HANDLE(,ImageView) \
        VK_HANDLE(,DeviceMemory)

#define VK_HANDLE(p, h) VK_GETTERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
    private:
#define VK_HANDLE(p, h) VK_MEMBERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
#undef VK_HANDLES
    };
}
