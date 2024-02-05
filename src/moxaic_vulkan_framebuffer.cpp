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
    MXC_CHK(normalTexture.InitFromImport(normalExternalHandle, extents));
    MXC_CHK(depthTexture.InitFromImport(depthExternalHandle, extents));
    MXC_CHK(gbufferTexture.InitFromImport(gBufferExternalHandle, extents));

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

void Framebuffer::TransitionAttachmentBuffers(const VkCommandBuffer commandBuffer,
                                              const Barrier2& src,
                                              const Barrier2& dst) const
{
    const StaticArray barriers{
      colorTexture.GetImageBarrier(src, dst),
      normalTexture.GetImageBarrier(src, dst),
      depthTexture.GetImageBarrier(src, dst),
      gbufferTexture.GetImageBarrier(src, dst),
    };
    const VkDependencyInfo dependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = barriers.size(),
      .pImageMemoryBarriers = barriers.data(),
    };
    VK_CHK_VOID(vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo));
}

// void Framebuffer::TransitionGBuffers(const VkCommandBuffer commandbuffer,
//                                      const Barrier& src,
//                                      const Barrier& dst) const
// {
//     const StaticArray acquireColorImageMemoryBarriers{
//       (VkImageMemoryBarrier){
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//         .srcAccessMask = src.colorAccessMask,
//         .dstAccessMask = dst.colorAccessMask,
//         .oldLayout = src.colorLayout,
//         .newLayout = dst.colorLayout,
//         .srcQueueFamilyIndex = Device->GetSrcQueue(src),
//         .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
//         .image = gbufferTexture.VkImageHandle,
//         .subresourceRange = gbufferTexture.GetSubresourceRange(),
//       }};
//     VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
//                                      src.colorStageMask,
//                                      dst.colorStageMask,
//                                      0,
//                                      0,
//                                      nullptr,
//                                      0,
//                                      nullptr,
//                                      acquireColorImageMemoryBarriers.size(),
//                                      acquireColorImageMemoryBarriers.data()));
// }
