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
            HANDLE parentSemaphoreExternalHandle;
            HANDLE childSemaphoreExternalHandle;
        };

        Node(const VulkanDevice &device);
        virtual ~Node();

        MXC_RESULT Init();
        MXC_RESULT InitFromImport(const ImportParam &param);

    private:
        const VulkanDevice &k_Device;

        Transform m_Transform;

        std::string m_Name;
        float m_DrawRadius;

        InterProcessRingBuffer m_ProducerRingBuffer;
//        InterProcessRingBuffer m_ReceiverRingBuffer;

        VulkanTimelineSemaphore m_CompositorSemaphore{k_Device};
        VulkanTimelineSemaphore m_NodeSemaphore{k_Device};

        VulkanFramebuffer m_Framebuffers[k_FramebufferCount]{k_Device,
                                                             k_Device};

        // Camera which compositor is using to reproject.
        Camera m_CompositingCamera;
        // Buffer which the node is currently using to render
        InterProcessBuffer<GlobalDescriptor::Buffer> m_GlobalDescriptorBuffer;

        STARTUPINFO m_Startupinfo;
        PROCESS_INFORMATION m_ProcessInformation;
    };
}
