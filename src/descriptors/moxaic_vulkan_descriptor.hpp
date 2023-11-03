#pragma once

#include "../moxaic_logging.hpp"
#include <vulkan/vulkan.h>

namespace Moxaic
{
    class VulkanDevice;
    class VulkanTexture;
    class VulkanFramebuffer;
    class Transform;

    template<typename T>
    class VulkanDescriptorBase
    {
    public:
        VulkanDescriptorBase(const VulkanDevice &device);
        virtual ~VulkanDescriptorBase();
        MXC_RESULT Init();

        inline VkDescriptorSetLayout vkLayout() const { return s_VkDescriptorSetLayout; }
        inline auto vkSet() const { return m_VkDescriptorSet; }
    protected:
        const VulkanDevice &k_Device;
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        virtual MXC_RESULT SetupDescriptorSetLayout() = 0;
        virtual MXC_RESULT SetupDescriptorSet() = 0;

        bool CreateDescriptorSetLayout();
        bool AllocateDescriptorSet();
        void PushBinding(VkDescriptorSetLayoutBinding binding);
        void PushWrite(VkDescriptorImageInfo imageInfo);
        void PushWrite(VkDescriptorBufferInfo bufferInfo);
        void WritePushedDescriptors();
    };
}
