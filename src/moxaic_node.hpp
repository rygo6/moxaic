#pragma once

#include "main.hpp"
#include "moxaic_camera.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_interprocess.hpp"
#include "moxaic_transform.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_semaphore.hpp"

#if WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class NodeReference
    {
        const Vulkan::Device* const Device;

    public:
        MXC_NO_VALUE_PASS(NodeReference);

        explicit NodeReference(const Vulkan::Device* pDevice);
        virtual ~NodeReference();

        MXC_RESULT Init(Vulkan::PipelineType pipelineType);
        MXC_RESULT ExportOverIPC(const Vulkan::Semaphore& compositorSemaphore);
        void SetZCondensedExportedGlobalDescriptorLocalBuffer(const Camera& camera);

        Moxaic::Transform transform{};
        // Buffer which the node is currently using to render
        InterProcessBuffer<Vulkan::GlobalDescriptor::UniformBuffer> exportedGlobalDescriptor{};
        Vulkan::Semaphore exportedSemaphore{Device};
        float drawRadius{1.0f};

        const auto& GetExportedFramebuffer(const int i) const { return exportedFramebuffers[i]; }

    private:

        const char* name{"default"};

        Camera compositingCamera{};// Camera which compositor is using to reproject.

        InterProcessProducer ipcToNode{};
        InterProcessReceiver ipcFromNode{};

        StaticArray<Vulkan::Framebuffer, FramebufferCount> exportedFramebuffers{
          Vulkan::Framebuffer(Device),
          Vulkan::Framebuffer(Device)};

        STARTUPINFO startupInfo{};
        PROCESS_INFORMATION processInformation{};
    };

    class Node
    {
    public:
        MXC_NO_VALUE_PASS(Node);

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

        explicit Node(const Vulkan::Device* const pDevice);
        virtual ~Node();

        MXC_RESULT Init();
        MXC_RESULT InitImport(const ImportParam& parameters);

        MXC_PTR_ACCESS(ImportedGlobalDescriptor);
        MXC_PTR_ACCESS(ImportedNodeSemaphore);
        MXC_PTR_ACCESS(ImportedCompositorSemaphore);

        const auto& framebuffer(const int index) const
        {
            return m_ImportedFramebuffers[index];
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
        const Vulkan::Device* const k_pDevice;

        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> TargetFuncs()
        {
            return {
              [this](void* pParameters) {
                  const auto pImportParameters = static_cast<ImportParam*>(pParameters);
                  this->InitImport(*pImportParameters);
              }};
        }

        Transform m_Transform{};

        const char* m_Name{"default"};
        float m_DrawRadius{1.0f};

        InterProcessBuffer<Vulkan::GlobalDescriptor::UniformBuffer> m_ImportedGlobalDescriptor{};

        InterProcessReceiver m_IPCFromCompositor{};
        InterProcessProducer m_IPCToCompositor{};

        Vulkan::Semaphore m_ImportedCompositorSemaphore{k_pDevice};
        Vulkan::Semaphore m_ImportedNodeSemaphore{k_pDevice};

        StaticArray<Vulkan::Framebuffer, FramebufferCount> m_ImportedFramebuffers{
          Vulkan::Framebuffer(k_pDevice),
          Vulkan::Framebuffer(k_pDevice)};
    };
}// namespace Moxaic
