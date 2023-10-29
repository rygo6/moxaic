#pragma once

#include "moxaic_vulkan.hpp"

#include "vulkan/vulkan.h"
#include <vector>

namespace Moxaic
{
    class VulkanDevice;

    class VulkanTexture;

    class VulkanFramebuffer;

    class VulkanDescriptorBase
    {
    public:
        explicit VulkanDescriptorBase(const VulkanDevice &device);
        virtual ~VulkanDescriptorBase();

        [[nodiscard]] static auto vkLayout();
        [[nodiscard]] inline auto vkSet() const { return m_VkDescriptorSet; }
    protected:
        const VulkanDevice &k_Device;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        bool CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                       VkDescriptorSetLayout &outSetLayout);

        bool AllocateDescriptorSet(VkDescriptorSetLayout const &descriptorSetLayout,
                                   VkDescriptorSet &outDescriptorSet);

        void PushBinding(VkDescriptorSetLayoutBinding binding,
                         std::vector<VkDescriptorSetLayoutBinding> &bindings);

        void PushWrite(VkDescriptorImageInfo imageInfo,
                       std::vector<VkWriteDescriptorSet> &outWrites);

        void WritePushedDescriptors(const std::vector<VkWriteDescriptorSet> &writes);
    };

    class GlobalDescriptor : VulkanDescriptorBase
    {
    public:
//        bool Init(Camera);
        bool Init();
    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
    };

    class MaterialDescriptor : VulkanDescriptorBase
    {
    public:
        bool Init(VulkanTexture texture);
    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
    };

    class MeshNodeDescriptor : VulkanDescriptorBase
    {
    public:
        bool Init(VulkanFramebuffer framebuffer);
    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
    };

}
