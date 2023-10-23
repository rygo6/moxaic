#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"

//static void createTexture(const FbrVulkan *pVulkan,
//                          VkExtent2D extent,
//                          VkFormat format,
//                          VkImageTiling tiling,
//                          VkImageUsageFlags usage,
//                          VkMemoryPropertyFlags properties,
//                          FbrTexture *pTexture) {
//
//    const VkImageCreateInfo imageCreateInfo = {
//            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//            .imageType = VK_IMAGE_TYPE_2D,
//            .extent.width = extent.width,
//            .extent.height = extent.height,
//            .extent.depth = 1,
//            .mipLevels = 1,
//            .arrayLayers = 1,
//            .format = format,
//            .tiling = tiling,
//            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//            .usage = usage,
//            .samples = VK_SAMPLE_COUNT_1_BIT,
//            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//    };
//    FBR_VK_CHECK(vkCreateImage(pVulkan->device, &imageCreateInfo, FBR_ALLOCATOR, &pTexture->image));
//
//    VkMemoryRequirements memRequirements = {};
//    uint32_t memTypeIndex;
//    FBR_VK_CHECK(fbrImageMemoryTypeFromProperties(pVulkan,
//                                                  pTexture->image,
//                                                  properties,
//                                                  &memRequirements,
//                                                  &memTypeIndex));
//
//    VkMemoryAllocateInfo allocInfo = {
//            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//            .allocationSize = memRequirements.size,
//            .memoryTypeIndex = memTypeIndex,
//    };
//    FBR_VK_CHECK(vkAllocateMemory(pVulkan->device, &allocInfo, NULL, &pTexture->deviceMemory));
//
//    vkBindImageMemory(pVulkan->device, pTexture->image, pTexture->deviceMemory, 0);
//
//    pTexture->extent = extent;
//}

Moxaic::VulkanTexture::VulkanTexture(const VulkanDevice &device)
        : m_Device(device)
{}

Moxaic::VulkanTexture::~VulkanTexture() = default;

bool Moxaic::VulkanTexture::CreateFromImage(VkFormat format,
                                            VkExtent2D extent,
                                            VkImage image)
{

    return true;
}

bool Moxaic::VulkanTexture::CreateFromFile(bool external,
                                           const char *filename)
{

    return true;
}

bool Moxaic::VulkanTexture::Import(VkFormat format,
                                   VkExtent2D extent,
                                   VkImageUsageFlags usage,
                                   VkImageAspectFlags aspectMask,
                                   HANDLE externalMemory)
{

    return true;
}

bool Moxaic::VulkanTexture::Create(VkFormat format,
                                   VkExtent2D extent,
                                   VkImageUsageFlags usage,
                                   VkImageAspectFlags aspectMask,
                                   bool external)
{
//    if (external) {
//        createExternalTexture(pVulkan,
//                              extent,
//                              format,
//                              VK_IMAGE_TILING_OPTIMAL,
//                              usage,
//                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//                              pTexture);
//    } else {
//        createTexture(pVulkan,
//                      extent,
//                      format,
//                      VK_IMAGE_TILING_OPTIMAL,
//                      usage,
//                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//                      pTexture);
//    }
//    createTextureView(pVulkan, format, aspectMask, pTexture);
    return true;
}

void Moxaic::VulkanTexture::Destroy()
{

}
