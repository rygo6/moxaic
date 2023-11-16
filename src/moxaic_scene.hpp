#pragma once

#include "moxaic_logging.hpp"

#include "moxaic_camera.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_swap.hpp"
#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_node.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"

#include "moxaic_standard_pipeline.hpp"

namespace Moxaic
{
    class CompositorScene
    {
    public:
        CompositorScene(const Vulkan::Device &device);
        virtual ~CompositorScene() = default;

        MXC_RESULT Init();
        MXC_RESULT Loop(const uint32_t deltaTime);

    private:
        const Vulkan::Device &device;

        Vulkan::Framebuffer framebuffer{device};
        Vulkan::Swap swap{device};
        Vulkan::Semaphore semaphore{device};
        Vulkan::Mesh mesh{device};
        Vulkan::Texture texture{device};

        Vulkan::StandardPipeline standardPipeline{device};
        Vulkan::GlobalDescriptor globalDescriptor{device};
        Vulkan::MaterialDescriptor materialDescriptor{device};
        Vulkan::ObjectDescriptor objectDescriptor{device};

        Camera camera{};

        Transform transform{};

        // should node be here? maybe outside scene?
        CompositorNode compositorNode{device};
    };

    class NodeScene
    {
    public:
        NodeScene(const Vulkan::Device &device);
        virtual ~NodeScene() = default;

        MXC_RESULT Init();
        MXC_RESULT Loop(const uint32_t deltaTime);

    private:
        const Vulkan::Device &device;

        // should node be here? maybe outside scene?
        Node node{device};

        Vulkan::Mesh mesh{device};
        Vulkan::Texture texture{device};

        Vulkan::StandardPipeline standardPipeline{device};
        Vulkan::GlobalDescriptor globalDescriptor{device};
        Vulkan::MaterialDescriptor materialDescriptor{device};
        Vulkan::ObjectDescriptor objectDescriptor{device};

        Camera camera{};

        Transform transform{};
    };
}
