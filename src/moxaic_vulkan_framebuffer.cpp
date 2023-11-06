#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

constinit VkImageUsageFlags k_ColorBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constinit VkImageUsageFlags k_NormalBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_GBufferUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constinit VkImageUsageFlags k_DepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

Moxaic::VulkanFramebuffer::VulkanFramebuffer(const Moxaic::VulkanDevice &device)
        : k_Device(device)
//        , m_ColorTexture(std::make_unique<VulkanTexture>(device))
//        , m_NormalTexture(std::make_unique<VulkanTexture>(device))
//        , m_GBufferTexture(std::make_unique<VulkanTexture>(device))
//        , m_DepthTexture(std::make_unique<VulkanTexture>(device))
{}

Moxaic::VulkanFramebuffer::~VulkanFramebuffer() = default;

bool Moxaic::VulkanFramebuffer::Init(const VkExtent2D &dimensions,
                                     const Locality &locality)
{
    VkExtent3D extent3D = {dimensions.width, dimensions.height, 1};
    MXC_CHK(m_ColorTexture.Init(k_ColorBufferFormat,
                                extent3D,
                                k_ColorBufferUsage,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                locality));
    MXC_CHK(m_ColorTexture.InitialReadTransition());
    MXC_CHK(m_NormalTexture.Init(k_NormalBufferFormat,
                                 extent3D,
                                 k_NormalBufferUsage,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(m_NormalTexture.InitialReadTransition());
    MXC_CHK(m_GBufferTexture.Init(k_GBufferFormat,
                                  extent3D,
                                  k_GBufferUsage,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(m_GBufferTexture.InitialReadTransition());
    MXC_CHK(m_DepthTexture.Init(k_DepthBufferFormat,
                                extent3D,
                                k_DepthBufferUsage,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                locality));
    MXC_CHK(m_DepthTexture.InitialReadTransition());
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
            .width = dimensions.width,
            .height = dimensions.height,
            .layers = 1,
    };
    VK_CHK(vkCreateFramebuffer(k_Device.vkDevice(), &framebufferCreateInfo, VK_ALLOC, &m_VkFramebuffer));
    const VkSemaphoreCreateInfo renderCompleteCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(), &renderCompleteCreateInfo, VK_ALLOC, &m_VkRenderCompleteSemaphore));
    m_Dimensions = dimensions;
    return true;
}
