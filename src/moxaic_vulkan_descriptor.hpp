#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_uniform.hpp"

#include <vulkan/vulkan.h>
#include <vector>

#include <glm/glm.hpp>

namespace Moxaic
{
    class VulkanDevice;
    class VulkanTexture;
    class VulkanFramebuffer;
    class Camera;

    class VulkanDescriptorBase
    {
    public:
        VulkanDescriptorBase(const VulkanDevice &device);
        virtual ~VulkanDescriptorBase();

        inline virtual VkDescriptorSetLayout vkLayout() const = 0;
        inline auto vkSet() const { return m_VkDescriptorSet; }
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

        void PushWrite(VkDescriptorBufferInfo bufferInfo,
                       std::vector<VkWriteDescriptorSet> &outWrites);

        void WritePushedDescriptors(const std::vector<VkWriteDescriptorSet> &writes);
    };


    class GlobalDescriptor : VulkanDescriptorBase
    {
    public:
        GlobalDescriptor(const VulkanDevice &device);
        bool Init();
        void Update(Camera &camera, VkExtent2D dimensions);

        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }

        struct Buffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 invView;
            glm::mat4 invProj;
            uint32_t width;
            uint32_t height;
        };

    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VulkanUniform<Buffer> m_Uniform{k_Device};
    };


    class MaterialDescriptor : VulkanDescriptorBase
    {
    public:
        bool Init(VulkanTexture texture);
        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }
    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
    };


    class MeshNodeDescriptor : VulkanDescriptorBase
    {
    public:
        bool Init(VulkanFramebuffer framebuffer);
        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }
    private:
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
    };

}
