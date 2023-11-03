#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"
#include "moxaic_mesh_node_descriptor.hpp"

#include "../moxaic_vulkan.hpp"
#include "../moxaic_vulkan_device.hpp"

#include <vector>

thread_local std::vector<VkDescriptorSetLayoutBinding> g_Bindings;
thread_local std::vector<VkWriteDescriptorSet> g_Writes;

template<typename T>
Moxaic::VulkanDescriptorBase<T>::VulkanDescriptorBase(const VulkanDevice &device)
        : k_Device(device) {}

template<typename T>
Moxaic::VulkanDescriptorBase<T>::~VulkanDescriptorBase()
{
    vkFreeDescriptorSets(k_Device.vkDevice(),
                         k_Device.vkDescriptorPool(),
                         1,
                         &m_VkDescriptorSet);
}

template<typename T>
MXC_RESULT Moxaic::VulkanDescriptorBase<T>::Init()
{
    if (vkLayout() == VK_NULL_HANDLE) {
        MXC_CHK(SetupDescriptorSetLayout());
    };
    MXC_CHK(AllocateDescriptorSet());
    MXC_CHK(SetupDescriptorSet());
    return MXC_SUCCESS;
}

template<typename T>
void Moxaic::VulkanDescriptorBase<T>::PushBinding(VkDescriptorSetLayoutBinding binding)
{
    binding.binding = g_Bindings.size();
    binding.descriptorCount = 1;
    g_Bindings.push_back(binding);
}

template<typename T>
MXC_RESULT Moxaic::VulkanDescriptorBase<T>::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = g_Bindings.size();
    layoutInfo.pBindings = g_Bindings.data();
    VK_CHK(vkCreateDescriptorSetLayout(k_Device.vkDevice(),
                                       &layoutInfo,
                                       VK_ALLOC,
                                       &s_VkDescriptorSetLayout));
    g_Bindings.clear();
    return MXC_SUCCESS;
}

template<typename T>
bool Moxaic::VulkanDescriptorBase<T>::AllocateDescriptorSet()
{
    const VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = k_Device.vkDescriptorPool(),
            .descriptorSetCount = 1,
            .pSetLayouts = &s_VkDescriptorSetLayout,
    };
    VK_CHK(vkAllocateDescriptorSets(k_Device.vkDevice(),
                                    &allocInfo,
                                    &m_VkDescriptorSet));
    return true;
}

template<typename T>
void Moxaic::VulkanDescriptorBase<T>::PushWrite(VkDescriptorBufferInfo bufferInfo)
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
    write.dstBinding = g_Writes.size();
    write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
    g_Writes.push_back(write);
}

template<typename T>
void Moxaic::VulkanDescriptorBase<T>::PushWrite(VkDescriptorImageInfo imageInfo)
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
    write.dstBinding = g_Writes.size();
    write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
    g_Writes.push_back(write);
}

template<typename T>
void Moxaic::VulkanDescriptorBase<T>::WritePushedDescriptors()
{
    VK_CHK_VOID(vkUpdateDescriptorSets(k_Device.vkDevice(),
                                       g_Writes.size(),
                                       g_Writes.data(),
                                       0,
                                       nullptr));
    g_Writes.clear();
}

template
class Moxaic::VulkanDescriptorBase<Moxaic::GlobalDescriptor>;

template
class Moxaic::VulkanDescriptorBase<Moxaic::MaterialDescriptor>;


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