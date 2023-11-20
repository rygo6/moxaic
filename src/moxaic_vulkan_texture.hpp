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
        MXC_NO_VALUE_PASS(Texture);

        explicit Texture(const Device& device);
        virtual ~Texture();

        MXC_RESULT InitFromFile(const std::string& file,
                                Locality locality);

        MXC_RESULT InitFromImport(VkFormat format,
                                  VkExtent2D extents,
                                  VkImageUsageFlags usage,
                                  VkImageAspectFlags aspectMask,
                                  HANDLE externalMemory);

        MXC_RESULT Init(VkFormat format,
                        VkExtent2D extents,
                        VkImageUsageFlags usage,
                        VkImageAspectFlags aspectMask,
                        Locality locality);

        MXC_RESULT TransitionImmediateInitialToGraphicsRead() const;

        HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        const auto& vkImage() const { return m_VkImage; }
        const auto& vkImageView() const { return m_VkImageView; }

    private:
        const Device& k_Device;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};

        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        VkExtent2D m_Extents{};
        VkImageAspectFlags m_AspectMask{};

#ifdef WIN32
        HANDLE m_ExternalHandle{};
#endif
        bool TransitionImmediateInitialToTransferDst() const;
        bool TransitionImmediateTransferDstToGraphicsRead() const;

        MXC_RESULT InitImage(VkFormat format,
                             VkExtent2D extents,
                             VkImageUsageFlags usage,
                             Locality locality);

        MXC_RESULT InitImageView(VkFormat format,
                                 VkImageAspectFlags aspectMask);
    };
}
