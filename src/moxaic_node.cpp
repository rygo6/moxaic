#include "moxaic_node.hpp"

#include "moxaic_logging.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_window.hpp"

using namespace Moxaic;

constexpr char k_TempSharedProducerName[] = "IPCProducer";
constexpr char k_TempSharedCamMemoryName[] = "IPCCamera";

static MXC_RESULT StartProcess(STARTUPINFO& si, PROCESS_INFORMATION& pi)
{
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char buf[256];

    snprintf(buf, sizeof(buf), "moxaic.exe -node");
    MXC_LOG("Process Command", buf);

    if (!CreateProcess(nullptr,
                       // No module name (use command line)
                       buf,
                       // Command line
                       nullptr,
                       // Process handle not inheritable
                       nullptr,
                       // Thread handle not inheritable
                       FALSE,
                       // Set handle inheritance to FALSE
                       0,
                       // No creation flags
                       nullptr,
                       // Use parent's environment block
                       nullptr,
                       // Use parent's starting directory
                       &si,
                       // Pointer to STARTUPINFO structure
                       &pi)) {
        // Pointer to PROCESS_INFORMATION structure
        MXC_LOG_ERROR("CreateProcess fail");
        return MXC_FAIL;
    }

    return MXC_SUCCESS;
}

NodeReference::NodeReference(Vulkan::Device const& device)
    : k_Device(device) {}

NodeReference::~NodeReference()
{
    WaitForSingleObject(m_ProcessInformation.hProcess, INFINITE);
    CloseHandle(m_ProcessInformation.hProcess);
    CloseHandle(m_ProcessInformation.hThread);
}

MXC_RESULT NodeReference::Init()
{
    MXC_CHK(m_ExportedNodeSemaphore.Init(false,
                                         Vulkan::Locality::External));

    m_IPCToNode.Init(k_TempSharedProducerName);
    m_ExportedGlobalDescriptor.Init(k_TempSharedCamMemoryName);

    for (int i = 0; i < m_ExportedFramebuffers.size(); ++i) {
        m_ExportedFramebuffers[i].Init(Window::extents(),
                                       Vulkan::Locality::External);
    }

    StartProcess(m_Startupinfo, m_ProcessInformation);

    return MXC_SUCCESS;
}

MXC_RESULT NodeReference::ExportOverIPC(Vulkan::Semaphore const& compositorSemaphore)
{
    auto const hProcess = m_ProcessInformation.hProcess;
    Node::ImportParam const importParam{
      .framebufferWidth = m_ExportedFramebuffers[0].extents().width,
      .framebufferHeight = m_ExportedFramebuffers[0].extents().height,
      .colorFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].colorTexture().ClonedExternalHandle(hProcess),
      .colorFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].colorTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].normalTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].normalTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].gBufferTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].gBufferTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].depthTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].depthTexture().ClonedExternalHandle(hProcess),
      .compositorSemaphoreExternalHandle = compositorSemaphore.ClonedExternalHandle(hProcess),
      .nodeSemaphoreExternalHandle = m_ExportedNodeSemaphore.ClonedExternalHandle(hProcess),
    };
    m_IPCToNode.Enque(InterProcessTargetFunc::ImportCompositor, &importParam);

    return MXC_SUCCESS;
}

Node::Node(Vulkan::Device const& device)
    : k_Device(device) {}

Node::~Node() = default;

MXC_RESULT Node::Init()
{
    StaticArray const targetFuncs{
      (InterProcessFunc)[this](void* pParameters){
        auto const pImportParameters = static_cast<ImportParam*>(pParameters);
    this->InitImport(*pImportParameters);
}
}
;
m_IPCFromCompositor.Init(k_TempSharedProducerName, std::move(targetFuncs));
return MXC_SUCCESS;
}

MXC_RESULT Node::InitImport(ImportParam const& parameters)
{
    MXC_LOG("Node Init Import");
    m_ImportedFramebuffers[0].InitFromImport({parameters.framebufferWidth, parameters.framebufferHeight},
                                             parameters.colorFramebuffer0ExternalHandle,
                                             parameters.normalFramebuffer0ExternalHandle,
                                             parameters.gBufferFramebuffer0ExternalHandle,
                                             parameters.depthFramebuffer0ExternalHandle);
    m_ImportedFramebuffers[1].InitFromImport({parameters.framebufferWidth, parameters.framebufferHeight},
                                             parameters.colorFramebuffer1ExternalHandle,
                                             parameters.normalFramebuffer1ExternalHandle,
                                             parameters.gBufferFramebuffer1ExternalHandle,
                                             parameters.depthFramebuffer1ExternalHandle);

    m_ImportedCompositorSemaphore.InitFromImport(true, parameters.compositorSemaphoreExternalHandle);
    m_ImportedNodeSemaphore.InitFromImport(false, parameters.nodeSemaphoreExternalHandle);

    m_ImportedGlobalDescriptor.InitFromImport(k_TempSharedCamMemoryName);

    return MXC_SUCCESS;
}
