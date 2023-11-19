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
    class CompositorNode
    {
    public:

        CompositorNode(const Vulkan::Device &device);
        virtual ~CompositorNode();

        MXC_RESULT Init();

        MXC_RESULT ExportOverIPC(const Vulkan::Semaphore &compositorSemaphore);

        inline auto &ipcToNode()
        {
            return m_IPCToNode;
        }

        inline auto &ipcFromNode()
        {
            return m_IPCFromNode;
        }

    private:
        const Vulkan::Device &k_Device;

        Transform m_Transform{};

        std::string m_Name{"default"};
        float m_DrawRadius{1.0f};

        Camera m_CompositingCamera{}; // Camera which compositor is using to reproject.

        InterProcessBuffer<Vulkan::GlobalDescriptor::Buffer> m_ExportedGlobalDescriptor{}; // Buffer which the node is currently using to render

        InterProcessProducer m_IPCToNode{};
        InterProcessReceiver m_IPCFromNode{};

        Vulkan::Semaphore m_ExportedNodeSemaphore{k_Device};

        std::array<Vulkan::Framebuffer, FramebufferCount> m_ExportedFramebuffers{k_Device, k_Device};

        STARTUPINFO m_Startupinfo{};
        PROCESS_INFORMATION m_ProcessInformation{};
    };

    class Node
    {
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

        Node(const Vulkan::Device &device);
        virtual ~Node();

        MXC_RESULT Init();
        MXC_RESULT InitImport(ImportParam &parameters);

        inline auto &globalDescriptor()
        {
            return m_ImportedGlobalDescriptor;
        }

        inline const auto &framebuffer(int index) const
        {
            return m_ImportedFramebuffers[index];
        }

        inline const auto &CompositorSemaphore() const
        {
            return m_ImportedCompositorSemaphore;
        }

        inline auto &NodeSemaphore()
        {
            return m_ImportedNodeSemaphore;
        }

        inline auto &ipcFromCompositor()
        {
            return m_IPCFromCompositor;
        }

        inline auto &ipcToCompositor()
        {
            return m_IPCFromCompositor;
        }

    private:
        const Vulkan::Device &k_Device;

        inline std::array<InterProcessFunc, InterProcessTargetFunc::Count> TargetFuncs()
        {
            return {
                    [this](void *pParameters) {
                        auto pImportParameters = (ImportParam *) pParameters;
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

        std::array<Vulkan::Framebuffer, FramebufferCount> m_ImportedFramebuffers{k_Device, k_Device};
    };
}
