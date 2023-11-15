#pragma once

#include "moxaic_logging.hpp"

#include "moxaic_camera.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_swap.hpp"
#include "moxaic_vulkan_timeline_semaphore.hpp"
#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_node.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"

#include "moxaic_standard_pipeline.hpp"

namespace Moxaic
{
    class VulkanDevice;

    class Scene
    {
    public:
        Scene(const VulkanDevice &device);
        virtual ~Scene() = default;

        MXC_RESULT Init();
        MXC_RESULT Loop(const uint32_t deltaTime);

    private:
        const VulkanDevice &device;

        VulkanFramebuffer framebuffer{device};
        VulkanSwap swap{device};
        VulkanTimelineSemaphore timelineSemaphore{device};
        VulkanMesh mesh{device};
        VulkanTexture texture{device};

        CompositorNode compositorNode{device};

        StandardPipeline standardPipeline{device};
        GlobalDescriptor globalDescriptor{device};
        MaterialDescriptor materialDescriptor{device};
        ObjectDescriptor objectDescriptor{device};

        Camera camera{};

        Transform transform{};
    };
}
