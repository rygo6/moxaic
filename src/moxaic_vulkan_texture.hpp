#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <string>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Texture
    {
    public:
        explicit Texture(const Device &device);
        virtual ~Texture();

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

        const HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        inline auto vkImage() const { return m_VkImage; }
        inline auto vkImageView() const { return m_VkImageView; }
        inline auto vkDeviceMemory() const { return m_VkDeviceMemory; }

    private:
        const Device &k_Device;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};

        VkExtent2D m_Dimensions{};
        VkImageAspectFlags m_AspectMask{};

#ifdef WIN32
        HANDLE m_ExternalHandle{};
#endif
        bool TransitionImmediateInitialToTransferDst();
        bool TransitionImmediateTransferDstToGraphicsRead();
    };
}
