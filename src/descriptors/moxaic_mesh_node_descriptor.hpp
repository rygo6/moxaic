#pragma once

#include "moxaic_vulkan_descriptor.hpp"

namespace Moxaic::Vulkan
{
    class MeshNodeDescriptor : public VulkanDescriptorBase<MeshNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        MXC_RESULT Init();

    private:

    };
}
