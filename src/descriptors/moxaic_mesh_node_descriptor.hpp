#pragma once

#include "moxaic_vulkan_descriptor.hpp"

namespace Moxaic
{
    class MeshNodeDescriptor : public VulkanDescriptorBase<MeshNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
//        bool Init(VulkanFramebuffer framebuffer);

    private:

        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };

}
