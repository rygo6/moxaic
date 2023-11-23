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

        explicit Texture(Device const& device);
        virtual ~Texture();

        MXC_RESULT InitFromFile(std::string const& file,
                                Locality const& locality);

        MXC_RESULT InitFromImport(VkFormat const& format,
                                  VkExtent2D const& extents,
                                  VkImageUsageFlags const& usage,
                                  VkImageAspectFlags const& aspectMask,
                                  const HANDLE& externalMemory);

        MXC_RESULT Init(VkFormat const& format,
                        VkExtent2D const& extents,
                        VkImageUsageFlags const& usage,
                        VkImageAspectFlags const& aspectMask,
                        Locality const& locality);

        MXC_RESULT TransitionImmediateInitialToGraphicsRead() const;

        HANDLE ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const;

        auto const& vkImage() const { return m_VkImage; }
        auto const& vkImageView() const { return m_VkImageView; }

    private:
        Device const& k_Device;

        VkImage m_VkImage{VK_NULL_HANDLE};
        VkImageView m_VkImageView{VK_NULL_HANDLE};

        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        VkExtent2D m_Extents{};
        VkImageAspectFlags m_AspectMask{};

#ifdef WIN32
        HANDLE m_ExternalHandle{};
#endif

        MXC_RESULT TransitionImmediateInitialToTransferDst() const;
        MXC_RESULT TransitionImmediateTransferDstToGraphicsRead() const;

        MXC_RESULT InitImage(VkFormat const& format,
                             VkExtent2D const& extents,
                             VkImageUsageFlags const& usage,
                             Locality const& locality);

        MXC_RESULT InitImageView(VkFormat const& format,
                                 VkImageAspectFlags const& aspectMask);
    };
}// namespace Moxaic::Vulkan
