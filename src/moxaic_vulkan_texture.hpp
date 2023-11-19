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

        MXC_RESULT InitFromFile(const std::string file,
                                const Vulkan::Locality locality);

        MXC_RESULT InitFromImport(const VkFormat format,
                                  const VkExtent2D extent,
                                  const VkImageUsageFlags usage,
                                  const VkImageAspectFlags aspectMask,
                                  const HANDLE externalMemory);

        MXC_RESULT Init(const VkFormat format,
                        const VkExtent2D extents,
                        const VkImageUsageFlags usage,
                        const VkImageAspectFlags aspectMask,
                        const Vulkan::Locality locality);

        MXC_RESULT TransitionImmediateInitialToGraphicsRead();

        const HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        VkImage vkImage{VK_NULL_HANDLE};
        VkImageView vkImageView{VK_NULL_HANDLE};

    private:
        const Device &k_Device;

        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        VkExtent2D m_Extents{};
        VkImageAspectFlags m_AspectMask{};

#ifdef WIN32
        HANDLE m_ExternalHandle{};
#endif
        bool TransitionImmediateInitialToTransferDst();
        bool TransitionImmediateTransferDstToGraphicsRead();

        MXC_RESULT InitImage(const VkFormat format,
                             const VkExtent2D extents,
                             const VkImageUsageFlags usage,
                             const Locality locality);

        MXC_RESULT InitImageView(const VkFormat format,
                                 const VkImageAspectFlags aspectMask);
    };
}
