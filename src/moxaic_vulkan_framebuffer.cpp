#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Framebuffer::Framebuffer(const Vulkan::Device* const device,
                         const Vulkan::Locality locality)
    : Device(device),
      Locality(locality)
{
}

Framebuffer::~Framebuffer()
{
    assert(vkFramebuffer != nullptr);
    assert(vkRenderCompleteSemaphore != nullptr);
    vkDestroyFramebuffer(Device->VkDeviceHandle, vkFramebuffer, VK_ALLOC);
    vkDestroySemaphore(Device->VkDeviceHandle, vkRenderCompleteSemaphore, VK_ALLOC);
}

MXC_RESULT Framebuffer::Init(const PipelineType pipelineType,
                             const VkExtent2D extents)
{
    this->extents = extents;

    MXC_CHK(colorTexture.Init(extents));
    MXC_CHK(colorTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(normalTexture.Init(extents));
    MXC_CHK(normalTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(depthTexture.Init(extents));
    MXC_CHK(depthTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(gbufferTexture.Init(extents));
    MXC_CHK(gbufferTexture.TransitionInitialImmediate(pipelineType));


    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());

    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitFromImport(const PipelineType pipelineType,
                                       const VkExtent2D extents,
                                       const HANDLE colorExternalHandle,
                                       const HANDLE normalExternalHandle,
                                       const HANDLE gBufferExternalHandle,
                                       const HANDLE depthExternalHandle)
{
    this->extents = extents;

    MXC_CHK(colorTexture.InitFromImport(colorExternalHandle, extents));
    MXC_CHK(colorTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(normalTexture.InitFromImport(normalExternalHandle, extents));
    MXC_CHK(normalTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(gbufferTexture.InitFromImport(gBufferExternalHandle, extents));
    MXC_CHK(gbufferTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(depthTexture.InitFromImport(depthExternalHandle, extents));
    MXC_CHK(depthTexture.TransitionInitialImmediate(pipelineType));

    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());

    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitFramebuffer()
{
    const StaticArray attachments{
      colorTexture.VkImageViewHandle,
      normalTexture.VkImageViewHandle,
      // gbufferTexture.VKImageView,
      depthTexture.VkImageViewHandle,
    };
    const VkFramebufferCreateInfo framebufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderPass = Device->GetVkRenderPass(),
      .attachmentCount = attachments.size(),
      .pAttachments = attachments.data(),
      .width = extents.width,
      .height = extents.height,
      .layers = 1,
    };
    VK_CHK(vkCreateFramebuffer(Device->VkDeviceHandle,
                               &framebufferCreateInfo,
                               VK_ALLOC,
                               &vkFramebuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitSemaphore()
{
    constexpr VkSemaphoreCreateInfo renderCompleteCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(Device->VkDeviceHandle,
                             &renderCompleteCreateInfo,
                             VK_ALLOC,
                             &vkRenderCompleteSemaphore));
    return MXC_SUCCESS;
}

void Framebuffer::TransitionAttachmentBuffers(const VkCommandBuffer commandbuffer,
                                              const Barrier& src,
                                              const Barrier& dst) const
{
    const StaticArray acquireColorImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = colorTexture.VkImageHandle,
        .subresourceRange = colorTexture.GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = normalTexture.VkImageHandle,
        .subresourceRange = normalTexture.GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = gbufferTexture.VkImageHandle,
        .subresourceRange = gbufferTexture.GetSubresourceRange(),
      }};
    VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
                                     src.colorStageMask,
                                     dst.colorStageMask,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     acquireColorImageMemoryBarriers.size(),
                                     acquireColorImageMemoryBarriers.data()));
    const StaticArray acquireDepthImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.depthAccessMask,
        .dstAccessMask = dst.depthAccessMask,
        .oldLayout = src.depthLayout,
        .newLayout = dst.depthLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = depthTexture.VkImageHandle,
        .subresourceRange = depthTexture.GetSubresourceRange(),
      }};
    VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
                                     src.depthStageMask,
                                     dst.depthStageMask,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     acquireDepthImageMemoryBarriers.size(),
                                     acquireDepthImageMemoryBarriers.data()));
}


void Framebuffer::TransitionGBuffers(const VkCommandBuffer commandbuffer,
                                     const Barrier& src,
                                     const Barrier& dst) const
{
    const StaticArray acquireColorImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = gbufferTexture.VkImageHandle,
        .subresourceRange = gbufferTexture.GetSubresourceRange(),
      }};
    VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
                                     src.colorStageMask,
                                     dst.colorStageMask,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     acquireColorImageMemoryBarriers.size(),
                                     acquireColorImageMemoryBarriers.data()));
}
