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
    class Scene
    {
    public:
        Scene(const Vulkan::Device &device);
        virtual ~Scene() = default;

        MXC_RESULT Init();
        MXC_RESULT Loop(const uint32_t deltaTime);

    private:
        const Vulkan::Device &device;

        Vulkan::Framebuffer framebuffer{device};
        Vulkan::Swap swap{device};
        Vulkan::Semaphore timelineSemaphore{device};
        Vulkan::Mesh mesh{device};
        Vulkan::Texture texture{device};

        CompositorNode compositorNode{device};

        Vulkan::StandardPipeline standardPipeline{device};
        Vulkan::GlobalDescriptor globalDescriptor{device};
        Vulkan::MaterialDescriptor materialDescriptor{device};
        Vulkan::ObjectDescriptor objectDescriptor{device};

        Camera camera{};

        Transform transform{};
    };
}
