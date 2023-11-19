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
        std::array<Vulkan::Framebuffer, FramebufferCount> m_Framebuffers{k_Device, k_Device};
        int m_FramebufferIndex{0};
        
        Vulkan::Swap m_Swap{k_Device};
        Vulkan::Semaphore m_Semaphore{k_Device};

        Vulkan::StandardPipeline m_StandardPipeline{k_Device};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{k_Device};
        Vulkan::MaterialDescriptor m_MaterialDescriptor{k_Device};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{k_Device};

        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{k_Device};
        Vulkan::Texture m_SphereTestTexture{k_Device};
        Transform m_SphereTestTransform{};

        // should node be here? maybe outside scene?
        CompositorNode m_CompositorNode{k_Device};
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

        Vulkan::StandardPipeline m_StandardPipeline{k_Device};
        Vulkan::GlobalDescriptor m_GlobalDescriptor{k_Device};
        Vulkan::MaterialDescriptor m_MaterialDescriptor{k_Device};
        Vulkan::ObjectDescriptor m_ObjectDescriptor{k_Device};

        Camera m_MainCamera{};

        Vulkan::Mesh m_SphereTestMesh{k_Device};
        Vulkan::Texture m_SphereTestTexture{k_Device};
        Transform m_SpherTestTransform{};
    };
}
