#pragma once

#include "moxaic_camera.hpp"
#include "moxaic_node.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_mesh.hpp"
#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_swap.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"
#include "moxaic_standard_pipeline.hpp"

#include "moxaic_compute_node_descriptor.hpp"
#include "moxaic_compute_node_pipeline.hpp"
#include "moxaic_mesh_node_descriptor.hpp"
#include "moxaic_mesh_node_pipeline.hpp"

namespace Moxaic
{
    class SceneBase
    {
    public:
        MXC_NO_VALUE_PASS(SceneBase);

        explicit SceneBase(const Vulkan::Device* const device)
            : Device(device) {}

        virtual ~SceneBase() = default;

        virtual MXC_RESULT Init() = 0;
        virtual MXC_RESULT Loop(const uint32_t& deltaTime) = 0;

    protected:
        const Vulkan::Device* const Device;
    };

    class CompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        StaticArray<Vulkan::Framebuffer, FramebufferCount> framebuffers{
          Vulkan::Framebuffer(Device),
          Vulkan::Framebuffer(Device)};
        int framebufferIndex{0};

        Vulkan::Swap swap{Device};
        Vulkan::Semaphore semaphore{Device};

        Vulkan::StandardPipeline standardPipeline{Device};
        Vulkan::GlobalDescriptor globalDescriptor{Device};
        Vulkan::StandardMaterialDescriptor standardMaterialDescriptor{Device};
        Vulkan::ObjectDescriptor objectDescriptor{Device};

        Vulkan::MeshNodePipeline meshNodePipeline{Device};
        StaticArray<Vulkan::MeshNodeDescriptor, FramebufferCount> meshNodeDescriptor{
          Vulkan::MeshNodeDescriptor(Device),
          Vulkan::MeshNodeDescriptor(Device)};


        Camera mainCamera{};

        Vulkan::Mesh sphereTestMesh{Device};
        Vulkan::Texture sphereTestTexture{Device};
        Transform sphereTestTransform{};

        // should node be here? maybe outside scene?
        NodeReference nodeReference{Device};
        uint64_t priorNodeSemaphoreWaitValue{0};
        uint64_t nodeFramebufferIndex{1};// yes this has to default to one to sync with child properly... this should probably be sent over ipc somehow
    };

    class ComputeCompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        Vulkan::Swap swap{Device};
        Vulkan::Semaphore semaphore{Device};

        Vulkan::ComputeNodePipeline computeNodePrePipeline{Device};
        Vulkan::ComputeNodePipeline computeNodePipeline{Device};
        Vulkan::ComputeNodePipeline computeNodePostPipeline{Device};

        Vulkan::GlobalDescriptor globalDescriptor{Device};
        Vulkan::ComputeNodeDescriptor computeNodeDescriptor{Device};

        Vulkan::Texture outputAtomicTexture{Device};
        Vulkan::Texture outputAveragedAtomicTexture{Device};

        Camera mainCamera{};

        // should node be here? maybe outside scene?
        NodeReference nodeReference{Device};
        uint64_t priorNodeSemaphoreWaitValue{0};
        uint64_t nodeFramebufferIndex{1};// yes this has to default to one to sync with child properly... this should probably be sent over ipc somehow
    };

    class NodeScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        // should node be here? maybe outside scene?
        Node node{Device};
        int framebufferIndex{0};
        uint64_t compositorSempahoreStep{30};

        Vulkan::Swap swap{Device};

        Vulkan::StandardPipeline standardPipeline{Device};
        Vulkan::GlobalDescriptor globalDescriptor{Device};
        Vulkan::StandardMaterialDescriptor materialDescriptor{Device};
        Vulkan::ObjectDescriptor objectDescriptor{Device};

        Camera mainCamera{};

        Vulkan::Mesh sphereTestMesh{Device};
        Vulkan::Texture sphereTestTexture{Device};
        Transform spherTestTransform{};
    };
}
