/**********************************************************************************************
*
*   Vulkan Medium v0.0 - Vulkan boilerplate API
*
**********************************************************************************************/

#ifndef VULKAN_MEDIUM_H
#define VULKAN_MEDIUM_H

#include <vulkan/vulkan.h>

#if defined(__cplusplus)
extern "C" {// Prevents name mangling of functions
#endif

//------------------------------------------------------------------------------------
// Functions Declaration
//------------------------------------------------------------------------------------


// void glGenerateTextureMipmap(	GLuint texture);

void vkCmdPipelineImageBarrier2(
  VkCommandBuffer commandBuffer,
  uint32_t imageMemoryBarrierCount,
  const VkImageMemoryBarrier2* pImageMemoryBarriers);

#if defined(__cplusplus)
}
#endif

#endif//VULKAN_MEDIUM_H

/***********************************************************************************
*
*   VULKAN MEDIUM IMPLEMENTATION
*
************************************************************************************/

#if defined(VULKAN_MEDIUM_IMPLEMENTATION)

void vkCmdPipelineImageBarrier2(
  VkCommandBuffer commandBuffer,
  uint32_t imageMemoryBarrierCount,
  const VkImageMemoryBarrier2* pImageMemoryBarriers)
{
    const VkDependencyInfo toComputeDependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = imageMemoryBarrierCount,
      .pImageMemoryBarriers = pImageMemoryBarriers,
    };
    vkCmdPipelineBarrier2(commandBuffer, &toComputeDependencyInfo);
}

typedef struct VkFormatAspect
{
    VkFormat format;
    VkImageAspectFlags aspectMask;
} VkFormatAspect;

typedef struct VkTexture
{
    VkFormatAspect formatAspect;
    uint32_t levelCount;
    uint32_t layerCount;
    VkImage image;
    VkImageView view;
} VkTexture;

VkResult vkCreateImageView2D(
  VkDevice device,
  VkImage image,
  VkFormat format,
  VkImageAspectFlags aspectMask,
  const VkAllocationCallbacks* pAllocator,
  VkImageView* pView)
{
    const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .levelCount = 1,
        .layerCount = 1,
      },
    };
    return vkCreateImageView(device,
                             &imageViewCreateInfo,
                             pAllocator,
                             pView);
}

VkResult vkCreateMipImageView2D(
  VkDevice device,
  VkImage image,
  VkFormat format,
  VkImageAspectFlags aspectMask,
  int mipLevelCount,
  int mipLevel,
  const VkAllocationCallbacks* pAllocator,
  VkImageView* pView)
{
    const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .baseMipLevel = mipLevel,
        .levelCount = mipLevelCount,
        .layerCount = 1,
      },
    };
    return vkCreateImageView(device,
                             &imageViewCreateInfo,
                             pAllocator,
                             pView);
}

#endif
