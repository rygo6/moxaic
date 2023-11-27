#pragma once

#include "moxaic_logging.hpp"

#include "moxaic_camera.hpp"
#include "moxaic_node.hpp"
#include "moxaic_vulkan.hpp"
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

        explicit SceneBase(const Vulkan::Device& device)
            : k_pDevice(&device) {}

        virtual ~SceneBase() = default;

        virtual MXC_RESULT Init() = 0;
        virtual MXC_RESULT Loop(const uint32_t& deltaTime) = 0;

    protected:
        const Vulkan::Device* const k_pDevice;// do I really want this be a pointer?!
    };

    class CompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        StaticArray<Vulkan::Framebuffer, FramebufferCount> m_Framebuffers{
          Vulkan::Framebuffer(*k_pDevice),
          Vulkan::Framebuffer(*k_pDevice)};
        int m_FramebufferIndex{0};

        Vulkan::Swap m_Swap{*k_pDevice};
        Vulkan::Semaphore m_Semaphore{*k_pDevice};

        Vulkan::StandardPipeline m_StandardPipeline{*k_pDevice};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{*k_pDevice};
        Vulkan::StandardMaterialDescriptor m_StandardMaterialDescriptor{*k_pDevice};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{*k_pDevice};

        Vulkan::MeshNodePipeline m_MeshNodePipeline{*k_pDevice};
        StaticArray<Vulkan::MeshNodeDescriptor, FramebufferCount> m_MeshNodeDescriptor{
          Vulkan::MeshNodeDescriptor(*k_pDevice),
          Vulkan::MeshNodeDescriptor(*k_pDevice)};


        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{*k_pDevice};
        Vulkan::Texture m_SphereTestTexture{*k_pDevice};
        Transform m_SphereTestTransform{};

        // should node be here? maybe outside scene?
        NodeReference m_NodeReference{*k_pDevice};
        uint64_t m_PriorNodeSemaphoreWaitValue{0};
        uint64_t m_NodeFramebufferIndex{1};// yes this has to default to one to sync with child properly... this should probably be sent over ipc somehow
    };

    class ComputeCompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        Vulkan::Swap m_Swap{*k_pDevice};
        Vulkan::Semaphore m_Semaphore{*k_pDevice};

        Vulkan::ComputeNodePipeline m_ComputeNodePipeline{*k_pDevice};

        Vulkan::GlobalDescriptor m_GlobalDescriptor{*k_pDevice};
        Vulkan::ComputeNodeDescriptor m_ComputeNodeDescriptor{*k_pDevice};

        Camera m_MainCamera{};

        // should node be here? maybe outside scene?
        NodeReference m_NodeReference{*k_pDevice};
        uint64_t m_PriorNodeSemaphoreWaitValue{0};
        uint64_t m_NodeFramebufferIndex{1};// yes this has to default to one to sync with child properly... this should probably be sent over ipc somehow
    };

    class NodeScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t& deltaTime) override;

    private:
        // should node be here? maybe outside scene?
        Node m_Node{*k_pDevice};
        int m_FramebufferIndex{0};
        uint64_t m_CompositorSempahoreStep{30};

        Vulkan::Swap m_Swap{*k_pDevice};

        Vulkan::StandardPipeline m_StandardPipeline{*k_pDevice};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{*k_pDevice};
        Vulkan::StandardMaterialDescriptor m_MaterialDescriptor{*k_pDevice};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{*k_pDevice};

        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{*k_pDevice};
        Vulkan::Texture m_SphereTestTexture{*k_pDevice};
        Transform m_SpherTestTransform{};
    };
}// namespace Moxaic
