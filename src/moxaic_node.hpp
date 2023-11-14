#pragma once

#include "main.hpp"
#include "moxaic_transform.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_interprocess.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_timeline_semaphore.hpp"

#include <string>

#if WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class CompositorNode
    {
    public:

        CompositorNode(const VulkanDevice &device);
        virtual ~CompositorNode();

        MXC_RESULT Init();

    private:
        const VulkanDevice &k_Device;

        Transform m_Transform{};

        std::string m_Name{"default"};
        float m_DrawRadius{1.0f};

        Camera m_CompositingCamera{}; // Camera which compositor is using to reproject.

        InterProcessBuffer<GlobalDescriptor::Buffer> m_ExportedGlobalDescriptor{}; // Buffer which the node is currently using to render

        InterProcessProducer m_IPCToNode{};
        InterProcessReceiver m_IPCFromNode{};

        VulkanTimelineSemaphore m_ExportedNodeSemaphore{k_Device};

        std::array<VulkanFramebuffer, k_FramebufferCount> m_ExportedFramebuffers{k_Device, k_Device};

        STARTUPINFO m_Startupinfo{};
        PROCESS_INFORMATION m_ProcessInformation{};
    };

    class Node
    {
    public:
        struct ImportParam
        {
            uint16_t framebufferWidth;
            uint16_t framebufferHeight;
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

        Node(const VulkanDevice &device);
        virtual ~Node();

        MXC_RESULT Init();
        MXC_RESULT InitImport(ImportParam& parameters);

    private:
        const VulkanDevice &k_Device;

        Transform m_Transform{};

        std::string m_Name{"default"};
        float m_DrawRadius{1.0f};

        InterProcessBuffer<GlobalDescriptor::Buffer> m_ImportedGlobalDescriptor{};

        InterProcessReceiver m_IPCFromCompositor{};
        InterProcessProducer m_IPCToCompositor{};

        VulkanTimelineSemaphore m_ImportedCompositorSemaphore{k_Device};
        VulkanTimelineSemaphore m_ImportedNodeSemaphore{k_Device};

        std::array<VulkanFramebuffer, k_FramebufferCount> m_ImportedFramebuffers{k_Device, k_Device};
    };
}
