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
    class SceneBase
    {
    public:
        SceneBase(const Vulkan::Device &device)
                : k_Device(device) {}
        virtual ~SceneBase() = default;
        virtual MXC_RESULT Init() = 0;
        virtual MXC_RESULT Loop(const uint32_t deltaTime) = 0;
    protected:
        const Vulkan::Device &k_Device;
    };

    class CompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t deltaTime) override;

    private:
        Vulkan::Framebuffer framebuffer{k_Device};
        Vulkan::Swap swap{k_Device};
        Vulkan::Semaphore semaphore{k_Device};
        Vulkan::Mesh mesh{k_Device};
        Vulkan::Texture texture{k_Device};

        Vulkan::StandardPipeline standardPipeline{k_Device};
        Vulkan::GlobalDescriptor globalDescriptor{k_Device};
        Vulkan::MaterialDescriptor materialDescriptor{k_Device};
        Vulkan::ObjectDescriptor objectDescriptor{k_Device};

        Camera camera{};

        Transform transform{};

        // should node be here? maybe outside scene?
        CompositorNode compositorNode{k_Device};
    };

    class NodeScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t deltaTime) override;

    private:
        // should node be here? maybe outside scene?
        Node node{k_Device};

        Vulkan::Mesh mesh{k_Device};
        Vulkan::Texture texture{k_Device};

        Vulkan::StandardPipeline standardPipeline{k_Device};
        Vulkan::GlobalDescriptor globalDescriptor{k_Device};
        Vulkan::MaterialDescriptor materialDescriptor{k_Device};
        Vulkan::ObjectDescriptor objectDescriptor{k_Device};

        Camera camera{};

        Transform transform{};
    };
}
