#include "moxaic_vulkan_descriptor.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_uniform.hpp"

#include "moxaic_camera.hpp"

#include <vector>

Moxaic::VulkanDescriptorBase::VulkanDescriptorBase(const VulkanDevice &device)
        : k_Device(device) {}


Moxaic::VulkanDescriptorBase::~VulkanDescriptorBase()
{
    vkFreeDescriptorSets(k_Device.vkDevice(),
                         k_Device.vkDescriptorPool(),
                         1,
                         &m_VkDescriptorSet);
}

MXC_RESULT Moxaic::VulkanDescriptorBase::Init()
{
    if (vkLayout() == VK_NULL_HANDLE) {
        MXC_CHK(SetupDescriptorSetLayout());
    };
    MXC_CHK(SetupDescriptorSet());
    return MXC_SUCCESS;
}

void Moxaic::VulkanDescriptorBase::PushBinding(VkDescriptorSetLayoutBinding binding,
                                               std::vector<VkDescriptorSetLayoutBinding> &bindings)
{
    binding.binding = bindings.size();
    binding.descriptorCount = 1;
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
    return true;
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

void Moxaic::VulkanDescriptorBase::PushWrite(VkDescriptorBufferInfo bufferInfo,
                                             std::vector<VkWriteDescriptorSet> &outWrites)
{
    SDL_assert_always(m_VkDescriptorSet != nullptr);
    VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_VkDescriptorSet,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo
    };
    write.dstBinding = outWrites.size();
    write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
    outWrites.push_back(write);
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
    write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
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

/// GlobalDescriptor
MXC_RESULT Moxaic::GlobalDescriptor::SetupDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    PushBinding((VkDescriptorSetLayoutBinding) {
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                          VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                          VK_SHADER_STAGE_COMPUTE_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT |
                          VK_SHADER_STAGE_MESH_BIT_EXT |
                          VK_SHADER_STAGE_TASK_BIT_EXT,
    }, bindings);
    MXC_CHK(CreateDescriptorSetLayout(bindings, s_VkDescriptorSetLayout));
    return MXC_SUCCESS;
}
MXC_RESULT Moxaic::GlobalDescriptor::SetupDescriptorSet()
{
    MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           Locality::Local));
    MXC_CHK(AllocateDescriptorSet(s_VkDescriptorSetLayout, m_VkDescriptorSet));
    std::vector<VkWriteDescriptorSet> writes;
    PushWrite((VkDescriptorBufferInfo) {
            .buffer = m_Uniform.vkBuffer(),
            .range = sizeof(Buffer),
    }, writes);
    WritePushedDescriptors(writes);
    return MXC_SUCCESS;
}
void Moxaic::GlobalDescriptor::Update(Camera &camera, VkExtent2D dimensions)
{
    m_Uniform.Mapped().width = dimensions.width;
    m_Uniform.Mapped().height = dimensions.height;
    m_Uniform.Mapped().proj = camera.projection();
    m_Uniform.Mapped().invProj = camera.inverseProjection();
    m_Uniform.Mapped().view = camera.view();
    m_Uniform.Mapped().invView = camera.inverseView();
}

/// MaterialDescriptor
MXC_RESULT Moxaic::MaterialDescriptor::SetupDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    PushBinding((VkDescriptorSetLayoutBinding) {
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    }, bindings);

    MXC_CHK(CreateDescriptorSetLayout(bindings, s_VkDescriptorSetLayout));
    return MXC_SUCCESS;
}
MXC_RESULT Moxaic::MaterialDescriptor::SetupDescriptorSet()
{
    return MXC_SUCCESS;
}


//bool Moxaic::MeshNodeDescriptor::Init(VulkanFramebuffer framebuffer)
//{
//    if (s_VkDescriptorSetLayout == VK_NULL_HANDLE) {
//        std::vector<VkDescriptorSetLayoutBinding> bindings;
//        PushBinding((VkDescriptorSetLayoutBinding) {
//                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
//                              VK_SHADER_STAGE_TASK_BIT_EXT |
//                              VK_SHADER_STAGE_MESH_BIT_EXT
//        }, bindings);
//        PushBinding((VkDescriptorSetLayoutBinding) {
//                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
//                              VK_SHADER_STAGE_TASK_BIT_EXT |
//                              VK_SHADER_STAGE_MESH_BIT_EXT
//        }, bindings);
//        PushBinding((VkDescriptorSetLayoutBinding) {
//                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
//                              VK_SHADER_STAGE_TASK_BIT_EXT |
//                              VK_SHADER_STAGE_MESH_BIT_EXT
//        }, bindings);
//        PushBinding((VkDescriptorSetLayoutBinding) {
//                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
//                              VK_SHADER_STAGE_TASK_BIT_EXT |
//                              VK_SHADER_STAGE_MESH_BIT_EXT
//        }, bindings);
//        MXC_CHK(CreateDescriptorSetLayout(bindings, s_VkDescriptorSetLayout));
//    };
//
//    MXC_CHK(AllocateDescriptorSet(s_VkDescriptorSetLayout, m_VkDescriptorSet));
//    std::vector<VkWriteDescriptorSet> writes;
//    PushWrite((VkDescriptorImageInfo) {
//            .sampler = k_Device.vkLinearSampler(),
//            .imageView = framebuffer.colorTexture().vkImageView(),
//            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//    }, writes);
//    PushWrite((VkDescriptorImageInfo) {
//            .sampler = k_Device.vkLinearSampler(),
//            .imageView = framebuffer.normalTexture().vkImageView(),
//            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//    }, writes);
//    PushWrite((VkDescriptorImageInfo) {
//            .sampler = k_Device.vkLinearSampler(),
//            .imageView = framebuffer.gBufferTexture().vkImageView(),
//            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//    }, writes);
//    PushWrite((VkDescriptorImageInfo) {
//            .sampler = k_Device.vkLinearSampler(),
//            .imageView = framebuffer.colorTexture().vkImageView(),
//            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//    }, writes);
//    WritePushedDescriptors(writes);
//
//    return true;
//}