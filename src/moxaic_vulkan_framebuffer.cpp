#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

constinit VkImageUsageFlags k_ColorBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constinit VkImageUsageFlags k_NormalBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_GBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_DepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

static bool initialLayoutTransition(const Moxaic::VulkanTexture &vulkanTexture, VkImageAspectFlags aspectMask)
{
    return vulkanTexture.TransitionImageLayoutImmediate(VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                        0, 0,
                                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                        aspectMask);
}

Moxaic::VulkanFramebuffer::VulkanFramebuffer(const Moxaic::VulkanDevice &device)
        : k_Device(device)
        , m_ColorTexture(std::make_unique<VulkanTexture>(device))
        , m_NormalTexture(std::make_unique<VulkanTexture>(device))
        , m_GBufferTexture(std::make_unique<VulkanTexture>(device))
        , m_DepthTexture(std::make_unique<VulkanTexture>(device))
{}

Moxaic::VulkanFramebuffer::~VulkanFramebuffer() = default;

bool Moxaic::VulkanFramebuffer::Init(const VkExtent2D &extent,
                                     const BufferLocality &locality)
{
    VkExtent3D extent3D = {extent.width, extent.height, 1};
    MXC_CHK(m_ColorTexture->Init(k_ColorBufferFormat,
                                 extent3D,
                                 k_ColorBufferUsage,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(initialLayoutTransition(*m_ColorTexture, VK_IMAGE_ASPECT_COLOR_BIT));

    MXC_CHK(m_NormalTexture->Init(k_NormalBufferFormat,
                                  extent3D,
                                  k_NormalBufferUsage,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(initialLayoutTransition(*m_NormalTexture, VK_IMAGE_ASPECT_COLOR_BIT));

    MXC_CHK(m_GBufferTexture->Init(k_GBufferFormat,
                                   extent3D,
                                   k_GBufferUsage,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   locality));
    MXC_CHK(initialLayoutTransition(*m_GBufferTexture, VK_IMAGE_ASPECT_COLOR_BIT));

    MXC_CHK(m_DepthTexture->Init(k_DepthBufferFormat,
                                 extent3D,
                                 k_DepthBufferUsage,
                                 VK_IMAGE_ASPECT_DEPTH_BIT,
                                 locality));
    MXC_CHK(initialLayoutTransition(*m_DepthTexture, VK_IMAGE_ASPECT_DEPTH_BIT));

    const std::array attachments{
            m_ColorTexture->vkImageView(),
            m_NormalTexture->vkImageView(),
            m_GBufferTexture->vkImageView(),
            m_DepthTexture->vkImageView(),
    };
    const VkFramebufferCreateInfo framebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = k_Device.vkRenderPass(),
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
    };
    VK_CHK(k_Device.VkCreateFramebuffer(framebufferCreateInfo, m_Framebuffer));

    const VkSemaphoreCreateInfo renderCompleteCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    VK_CHK(k_Device.VkCreateSemaphore(renderCompleteCreateInfo, m_RenderCompleteSemaphore));

    return true;
}


