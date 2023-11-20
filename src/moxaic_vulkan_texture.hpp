#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <string>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    static inline const char* string_VkImageAspectFlagBits(VkImageAspectFlagBits input_value) {
        switch (input_value) {
            case VK_IMAGE_ASPECT_COLOR_BIT:
                return "COLOR_BIT";
            case VK_IMAGE_ASPECT_DEPTH_BIT:
                return "DEPTH_BIT";
            case VK_IMAGE_ASPECT_STENCIL_BIT:
                return "STENCIL_BIT";
            case VK_IMAGE_ASPECT_METADATA_BIT:
                return "METADATA_BIT";
            case VK_IMAGE_ASPECT_PLANE_0_BIT:
                return "PLANE_0_BIT";
            case VK_IMAGE_ASPECT_PLANE_1_BIT:
                return "PLANE_1_BIT";
            case VK_IMAGE_ASPECT_PLANE_2_BIT:
                return "PLANE_2_BIT";
            case VK_IMAGE_ASPECT_NONE:
                return "NONE";
            case VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT:
                return "MEMORY_PLANE_0_BIT_EXT";
            case VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT:
                return "MEMORY_PLANE_1_BIT_EXT";
            case VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT:
                return "MEMORY_PLANE_2_BIT_EXT";
            case VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT:
                return "MEMORY_PLANE_3_BIT_EXT";
            default:
                return "Unhandled VkImageAspectFlagBits";
        }
    }

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
