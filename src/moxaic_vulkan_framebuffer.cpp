#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

using namespace Moxaic;

constinit VkImageUsageFlags k_ColorBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constinit VkImageUsageFlags k_NormalBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_GBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_DepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

Vulkan::Framebuffer::Framebuffer(const Vulkan::Device &device)
        : k_Device(device) {}

Vulkan::Framebuffer::~Framebuffer() = default;

bool Vulkan::Framebuffer::Init(const VkExtent2D extents,
                               const Vulkan::Locality locality)
{
    MXC_CHK(m_ColorTexture.Init(k_ColorBufferFormat,
                                extents,
                                k_ColorBufferUsage,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                locality));
    MXC_CHK(m_ColorTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_NormalTexture.Init(k_NormalBufferFormat,
                                 extents,
                                 k_NormalBufferUsage,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(m_NormalTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_GBufferTexture.Init(k_GBufferFormat,
                                  extents,
                                  k_GBufferUsage,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(m_GBufferTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_DepthTexture.Init(k_DepthBufferFormat,
                                extents,
                                k_DepthBufferUsage,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                locality));
    MXC_CHK(m_DepthTexture.TransitionImmediateInitialToGraphicsRead());
    const std::array attachments{
            m_ColorTexture.vkImageView(),
            m_NormalTexture.vkImageView(),
            m_GBufferTexture.vkImageView(),
            m_DepthTexture.vkImageView(),
    };
    const VkFramebufferCreateInfo framebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = k_Device.vkRenderPass(),
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = extents.width,
            .height = extents.height,
            .layers = 1,
    };
    VK_CHK(vkCreateFramebuffer(k_Device.vkDevice(),
                               &framebufferCreateInfo,
                               VK_ALLOC,
                               &m_VkFramebuffer));
    MXC_CHK(InitSemaphore());
    m_Extents = extents;
    return true;
}

MXC_RESULT Vulkan::Framebuffer::InitFromImport(const VkExtent2D extents,
                                               const HANDLE colorExternalHandle,
                                               const HANDLE normalExternalHandle,
                                               const HANDLE gBufferExternalHandle,
                                               const HANDLE depthExternalHandle)
{
    MXC_CHK(m_ColorTexture.InitFromImport(k_ColorBufferFormat,
                                          extents,
                                          k_ColorBufferUsage,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          colorExternalHandle));
    MXC_CHK(m_ColorTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_NormalTexture.InitFromImport(k_NormalBufferFormat,
                                           extents,
                                           k_NormalBufferUsage,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           normalExternalHandle));
    MXC_CHK(m_NormalTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_GBufferTexture.InitFromImport(k_GBufferFormat,
                                            extents,
                                            k_GBufferUsage,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            gBufferExternalHandle));
    MXC_CHK(m_GBufferTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_DepthTexture.InitFromImport(k_DepthBufferFormat,
                                          extents,
                                          k_DepthBufferUsage,
                                          VK_IMAGE_ASPECT_DEPTH_BIT,
                                          depthExternalHandle));
    MXC_CHK(m_DepthTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(InitSemaphore());
    m_Extents = extents;
    return MXC_SUCCESS;
}

MXC_RESULT Vulkan::Framebuffer::InitSemaphore()
{
    const VkSemaphoreCreateInfo renderCompleteCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(),
                             &renderCompleteCreateInfo,
                             VK_ALLOC,
                             &m_VkRenderCompleteSemaphore));
    return MXC_SUCCESS;
}
