#include "vulkan_descriptor.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"

#include <vector>

Moxaic::VulkanDescriptorBase::VulkanDescriptorBase(const VulkanDevice &device)
        : k_Device(device) {}


Moxaic::VulkanDescriptorBase::~VulkanDescriptorBase() = default;

void Moxaic::VulkanDescriptorBase::PushBinding(VkDescriptorSetLayoutBinding binding,
                                               std::vector<VkDescriptorSetLayoutBinding> &bindings)
{
    binding.binding = bindings.size();
    binding.descriptorCount == 0 ? 1 : binding.descriptorCount;
    bindings.push_back(binding);
}

bool Moxaic::VulkanDescriptorBase::CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                                             VkDescriptorSetLayout &outSetLayout)
{
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();
    VK_CHK(vkCreateDescriptorSetLayout(k_Device.vkDevice(),
                                       &layoutInfo,
                                       VK_ALLOC,
                                       &outSetLayout));
    return false;
}

bool Moxaic::VulkanDescriptorBase::AllocateDescriptorSet(const VkDescriptorSetLayout &descriptorSetLayout,
                                                         VkDescriptorSet &outDescriptorSet)
{
    const VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = k_Device.vkDescriptorPool(),
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout,
    };
    VK_CHK(vkAllocateDescriptorSets(k_Device.vkDevice(),
                                    &allocInfo,
                                    &outDescriptorSet));
    return true;
}

void Moxaic::VulkanDescriptorBase::PushWrite(VkDescriptorImageInfo imageInfo,
                                             std::vector<VkWriteDescriptorSet> &outWrites)
{
    SDL_assert_always(m_VkDescriptorSet != nullptr);
    VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_VkDescriptorSet,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo
    };
    write.dstBinding = outWrites.size();
    outWrites.push_back(write);
}

void Moxaic::VulkanDescriptorBase::WritePushedDescriptors(const std::vector<VkWriteDescriptorSet> &writes)
{
    vkUpdateDescriptorSets(k_Device.vkDevice(),
                           writes.size(),
                           writes.data(),
                           0,
                           nullptr);
}

bool Moxaic::GlobalDescriptor::Init()
{
    return false;
}

bool Moxaic::MaterialDescriptor::Init(VulkanTexture texture)
{
    if (s_VkDescriptorSetLayout == nullptr) {
        MXC_CHK(CreateDescriptorSetLayout(
                {
                        (VkDescriptorSetLayoutBinding) {
                                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        }
                },
                s_VkDescriptorSetLayout));
    }

    return false;
}

bool Moxaic::MeshNodeDescriptor::Init(VulkanFramebuffer framebuffer)
{
    if (s_VkDescriptorSetLayout == VK_NULL_HANDLE) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        PushBinding((VkDescriptorSetLayoutBinding) {
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT
        }, bindings);
        PushBinding((VkDescriptorSetLayoutBinding) {
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT
        }, bindings);
        PushBinding((VkDescriptorSetLayoutBinding) {
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT
        }, bindings);
        PushBinding((VkDescriptorSetLayoutBinding) {
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT
        }, bindings);
        MXC_CHK(CreateDescriptorSetLayout(bindings, s_VkDescriptorSetLayout));
    };

    MXC_CHK(AllocateDescriptorSet(s_VkDescriptorSetLayout, m_VkDescriptorSet));
    std::vector<VkWriteDescriptorSet> writes;
    PushWrite((VkDescriptorImageInfo) {
            .sampler = k_Device.vkLinearSampler(),
            .imageView = framebuffer.ColorTexture().vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    }, writes);
    PushWrite((VkDescriptorImageInfo) {
            .sampler = k_Device.vkLinearSampler(),
            .imageView = framebuffer.NormalTexture().vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    }, writes);
    PushWrite((VkDescriptorImageInfo) {
            .sampler = k_Device.vkLinearSampler(),
            .imageView = framebuffer.GBufferTexture().vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    }, writes);
    PushWrite((VkDescriptorImageInfo) {
            .sampler = k_Device.vkLinearSampler(),
            .imageView = framebuffer.ColorTexture().vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    }, writes);
    WritePushedDescriptors(writes);

    return true;
}