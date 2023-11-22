#pragma once

#include "main.hpp"
#include "moxaic_transform.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_interprocess.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_semaphore.hpp"

#include <string>

#if WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class NodeReference
    {
    public:
        MXC_NO_VALUE_PASS(NodeReference);

        explicit NodeReference(const Vulkan::Device& device);
        virtual ~NodeReference();

        MXC_RESULT Init();

        MXC_RESULT ExportOverIPC(const Vulkan::Semaphore& compositorSemaphore);

        auto& GlobalDescriptor()
        {
            return m_ExportedGlobalDescriptor;
        }

        auto& Semaphore()
        {
            return m_ExportedNodeSemaphore;
        }

        const auto& framebuffer(const int index) const
        {
            return m_ExportedFramebuffers[index];
        }

        auto& ipcToNode()
        {
            return m_IPCToNode;
        }

        auto& ipcFromNode()
        {
            return m_IPCFromNode;
        }

    private:
        const Vulkan::Device& k_Device;

        Transform m_Transform{};

        std::string m_Name{"default"};
        float m_DrawRadius{1.0f};

        Camera m_CompositingCamera{}; // Camera which compositor is using to reproject.

        InterProcessBuffer<Vulkan::GlobalDescriptor::Buffer> m_ExportedGlobalDescriptor{};
        // Buffer which the node is currently using to render

        InterProcessProducer m_IPCToNode{};
        InterProcessReceiver m_IPCFromNode{};

        Vulkan::Semaphore m_ExportedNodeSemaphore{k_Device};

        StaticArray<Vulkan::Framebuffer, FramebufferCount> m_ExportedFramebuffers{
            Vulkan::Framebuffer(k_Device),
            Vulkan::Framebuffer(k_Device)
        };

        STARTUPINFO m_Startupinfo{};
        PROCESS_INFORMATION m_ProcessInformation{};
    };

    class Node
    {
        MXC_NO_VALUE_PASS(Node);

    public:
        struct ImportParam
        {
            uint32_t framebufferWidth;
            uint32_t framebufferHeight;
            HANDLE colorFramebuffer0ExternalHandle;
            HANDLE colorFramebuffer1ExternalHandle;
            HANDLE normalFramebuffer0ExternalHandle;
            HANDLE normalFramebuffer1ExternalHandle;
            HANDLE gBufferFramebuffer0ExternalHandle;
            HANDLE gBufferFramebuffer1ExternalHandle;
            HANDLE depthFramebuffer0ExternalHandle;
            HANDLE depthFramebuffer1ExternalHandle;
            HANDLE compositorSemaphoreExternalHandle;
            HANDLE nodeSemaphoreExternalHandle;
        };

        explicit Node(const Vulkan::Device& device);
        virtual ~Node();

        MXC_RESULT Init();
        MXC_RESULT InitImport(const ImportParam& parameters);

        const auto& globalDescriptor() const
        {
            return m_ImportedGlobalDescriptor;
        }

        const auto& framebuffer(const int index) const
        {
            return m_ImportedFramebuffers[index];
        }

        auto& CompositorSemaphore()
        {
            return m_ImportedCompositorSemaphore;
        }

        auto& NodeSemaphore()
        {
            return m_ImportedNodeSemaphore;
        }

        auto& ipcFromCompositor()
        {
            return m_IPCFromCompositor;
        }

        auto& ipcToCompositor()
        {
            return m_IPCFromCompositor;
        }

    private:
        const Vulkan::Device& k_Device;

        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> TargetFuncs()
        {
            return {
                [this](void* pParameters) {
                    const auto pImportParameters = static_cast<ImportParam *>(pParameters);
                    this->InitImport(*pImportParameters);
                }
            };
        }

        Transform m_Transform{};

        std::string m_Name{"default"};
        float m_DrawRadius{1.0f};

        InterProcessBuffer<Vulkan::GlobalDescriptor::Buffer> m_ImportedGlobalDescriptor{};

        InterProcessReceiver m_IPCFromCompositor{};
        InterProcessProducer m_IPCToCompositor{};

        Vulkan::Semaphore m_ImportedCompositorSemaphore{k_Device};
        Vulkan::Semaphore m_ImportedNodeSemaphore{k_Device};

        StaticArray<Vulkan::Framebuffer, FramebufferCount> m_ImportedFramebuffers{
            Vulkan::Framebuffer(k_Device),
            Vulkan::Framebuffer(k_Device)
        };
    };
}
