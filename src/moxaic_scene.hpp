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

#include "moxaic_standard_pipeline.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"

#include "moxaic_mesh_node_pipeline.hpp"
#include "moxaic_mesh_node_descriptor.hpp"

namespace Moxaic
{
    class SceneBase
    {
    public:
        MXC_NO_VALUE_PASS(SceneBase);

        explicit SceneBase(const Vulkan::Device& device)
            : k_Device(device) {}

        virtual ~SceneBase() = default;

        virtual MXC_RESULT Init() = 0;
        virtual MXC_RESULT Loop(const uint32_t deltaTime) = 0;

    protected:
        const Vulkan::Device& k_Device;
    };

    class CompositorScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t deltaTime) override;

    private:
        StaticArray<Vulkan::Framebuffer, FramebufferCount> m_Framebuffers{
            Vulkan::Framebuffer(k_Device),
            Vulkan::Framebuffer(k_Device)
        };
        int m_FramebufferIndex{0};

        Vulkan::Swap m_Swap{k_Device};
        Vulkan::Semaphore m_Semaphore{k_Device};

        Vulkan::StandardPipeline m_StandardPipeline{k_Device};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{k_Device};
        Vulkan::StandardMaterialDescriptor m_StandardMaterialDescriptor{k_Device};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{k_Device};

        Vulkan::MeshNodePipeline m_MeshNodePipeline{k_Device};
        StaticArray<Vulkan::MeshNodeDescriptor, FramebufferCount> m_MeshNodeDescriptor{
            Vulkan::MeshNodeDescriptor(k_Device),
            Vulkan::MeshNodeDescriptor(k_Device)
        };

        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{k_Device};
        Vulkan::Texture m_SphereTestTexture{k_Device};
        Transform m_SphereTestTransform{};

        // should node be here? maybe outside scene?
        NodeReference m_NodeReference{k_Device};
        uint64_t m_PriorNodeSemaphoreWaitValue{0};
        uint64_t m_NodeFramebufferIndex{0};
    };

    class NodeScene : public SceneBase
    {
    public:
        using SceneBase::SceneBase;

        MXC_RESULT Init() override;
        MXC_RESULT Loop(const uint32_t deltaTime) override;

    private:
        // should node be here? maybe outside scene?
        Node m_Node{k_Device};
        int m_FramebufferIndex{0};
        uint64_t m_CompositorSempahoreStep{30};

        Vulkan::Swap m_Swap{k_Device};

        Vulkan::StandardPipeline m_StandardPipeline{k_Device};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{k_Device};
        Vulkan::StandardMaterialDescriptor m_MaterialDescriptor{k_Device};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{k_Device};

        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{k_Device};
        Vulkan::Texture m_SphereTestTexture{k_Device};
        Transform m_SpherTestTransform{};
    };
}
