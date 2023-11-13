#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

constinit VkImageUsageFlags k_ColorBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constinit VkImageUsageFlags k_NormalBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_GBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_DepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

Moxaic::VulkanFramebuffer::VulkanFramebuffer(const Moxaic::VulkanDevice &device)
        : k_Device(device)
{}

Moxaic::VulkanFramebuffer::~VulkanFramebuffer() = default;

bool Moxaic::VulkanFramebuffer::Init(const VkExtent2D extents,
                                     const Locality locality)
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
    VK_CHK(vkCreateFramebuffer(k_Device.vkDevice(), &framebufferCreateInfo, VK_ALLOC, &m_VkFramebuffer));
    const VkSemaphoreCreateInfo renderCompleteCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(), &renderCompleteCreateInfo, VK_ALLOC, &m_VkRenderCompleteSemaphore));
    m_Dimensions = extents;
    return true;
}
