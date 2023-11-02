#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_uniform.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

#include <glm/glm.hpp>

namespace Moxaic
{
    class VulkanDevice;
    class VulkanTexture;
    class VulkanFramebuffer;
    class Camera;
    class Transform;

    class VulkanDescriptorBase
    {
    public:
        VulkanDescriptorBase(const VulkanDevice &device);
        virtual ~VulkanDescriptorBase();
        MXC_RESULT Init();

        inline virtual VkDescriptorSetLayout vkLayout() const = 0;
        inline auto vkSet() const { return m_VkDescriptorSet; }
    protected:
        const VulkanDevice &k_Device;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        virtual MXC_RESULT SetupDescriptorSetLayout() = 0;
        virtual MXC_RESULT SetupDescriptorSet() = 0;

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


    class GlobalDescriptor : public VulkanDescriptorBase
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        void Update(Camera &camera, VkExtent2D dimensions);

        struct Buffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 invView;
            glm::mat4 invProj;
            uint32_t width;
            uint32_t height;
        };

        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};

        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };


    class MaterialDescriptor : public VulkanDescriptorBase
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
//        void Update(VulkanTexture texture);

        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }

    private:

        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };

    class ObjectDescriptor : public VulkanDescriptorBase
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
//        void Update(Transform transform);

        struct Buffer
        {
            glm::mat4 model;
        };

        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};

        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };


    class MeshNodeDescriptor : public VulkanDescriptorBase
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
//        bool Init(VulkanFramebuffer framebuffer);

        inline VkDescriptorSetLayout vkLayout() const override { return s_VkDescriptorSetLayout; }

    private:

        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };

}
