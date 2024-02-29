// #define MXC_DISABLE_LOG

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

NodeReference::NodeReference(const Vulkan::Device* const pDevice)
    : Device(pDevice) {}

NodeReference::~NodeReference()
{
    WaitForSingleObject(processInformation.hProcess, INFINITE);
    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
}

MXC_RESULT NodeReference::Init(const Vulkan::PipelineType pipelineType)
{
    MXC_LOG("Init NodeReference");
    MXC_CHK(exportedSemaphore.Init(false,
                                   Vulkan::Locality::External));

    ipcToNode.Init(k_TempSharedProducerName);
    exportedGlobalDescriptor.Init(k_TempSharedCamMemoryName);

    for (uint32_t i = 0; i < exportedFramebuffers.size(); ++i) {
        exportedFramebuffers[i].Init(pipelineType,
                                     Window::GetExtents());
    }

    StartProcess(startupInfo, processInformation);

    return MXC_SUCCESS;
}

MXC_RESULT NodeReference::ExportOverIPC(const Vulkan::Semaphore& compositorSemaphore)
{
    const auto hProcess = processInformation.hProcess;
    const Node::ImportParam importParam{
      .framebufferWidth = exportedFramebuffers[0].GetExtents().width,
      .framebufferHeight = exportedFramebuffers[0].GetExtents().height,
      .colorFramebuffer0ExternalHandle = exportedFramebuffers[0].GetColorTexture().ClonedExternalHandle(hProcess),
      .colorFramebuffer1ExternalHandle = exportedFramebuffers[1].GetColorTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer0ExternalHandle = exportedFramebuffers[0].GetNormalTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer1ExternalHandle = exportedFramebuffers[1].GetNormalTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer0ExternalHandle = exportedFramebuffers[0].GetGBufferTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer1ExternalHandle = exportedFramebuffers[1].GetGBufferTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer0ExternalHandle = exportedFramebuffers[0].GetDepthTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer1ExternalHandle = exportedFramebuffers[1].GetDepthTexture().ClonedExternalHandle(hProcess),
      .compositorSemaphoreExternalHandle = compositorSemaphore.ClonedExternalHandle(hProcess),
      .nodeSemaphoreExternalHandle = exportedSemaphore.ClonedExternalHandle(hProcess),
    };
    ipcToNode.Enque(InterProcessTargetFunc::ImportCompositor, &importParam);

    return MXC_SUCCESS;
}

void NodeReference::SetZCondensedExportedGlobalDescriptorLocalBuffer(const Camera& camera)
{
    // Condense nearz/farz to draw radius of node
    const auto viewPosition = camera.GetView() * glm::vec4(transform.position, 1);
    const float viewDistanceToCenter = -viewPosition.z;
    const float offset = drawRadius * 0.5f;
    const float farZ = viewDistanceToCenter + offset;
    float nearZ = viewDistanceToCenter - offset;
    if (nearZ < MXC_CAMERA_MIN_Z) {
        nearZ = MXC_CAMERA_MIN_Z;
    }
    auto& localBuffer = exportedGlobalDescriptor.localBuffer;
    localBuffer.view = camera.GetView();
    localBuffer.proj = Camera::ReversePerspective(camera.fieldOfView,
                                                  camera.aspectRatio,
                                                  nearZ,
                                                  farZ);
    localBuffer.viewProj = localBuffer.proj * localBuffer.view;
    localBuffer.invView = camera.GetInverseView();
    localBuffer.invProj = glm::inverse(localBuffer.proj);
    localBuffer.invViewProj = localBuffer.invView * localBuffer.invProj;
}

Node::Node(const Vulkan::Device* const pDevice)
    : Device(pDevice) {}

Node::~Node() = default;

MXC_RESULT Node::Init()
{
    const InterProcessFunc importCompositorFunc = [this](void* pParameters) {
        const auto pImportParameters = static_cast<ImportParam*>(pParameters);
        this->InitImport(*pImportParameters);
    };
    const StaticArray targetFuncs{
      importCompositorFunc,
    };
    m_IPCFromCompositor.Init(k_TempSharedProducerName, std::move(targetFuncs));
    return MXC_SUCCESS;
}

MXC_RESULT Node::InitImport(const ImportParam& parameters)
{
    MXC_LOG("Node Init Import");
    m_ImportedFramebuffers[0].InitFromImport(Vulkan::CompositorPipelineType,// TODO this pipelinetpye needs to come over IPC probably
                                             (VkExtent2D){parameters.framebufferWidth,
                                                          parameters.framebufferHeight},
                                             parameters.colorFramebuffer0ExternalHandle,
                                             parameters.normalFramebuffer0ExternalHandle,
                                             parameters.gBufferFramebuffer0ExternalHandle,
                                             parameters.depthFramebuffer0ExternalHandle);
    m_ImportedFramebuffers[1].InitFromImport(Vulkan::CompositorPipelineType,
                                             (VkExtent2D){parameters.framebufferWidth,
                                                          parameters.framebufferHeight},
                                             parameters.colorFramebuffer1ExternalHandle,
                                             parameters.normalFramebuffer1ExternalHandle,
                                             parameters.gBufferFramebuffer1ExternalHandle,
                                             parameters.depthFramebuffer1ExternalHandle);

    m_ImportedCompositorSemaphore.InitFromImport(true, parameters.compositorSemaphoreExternalHandle);

    // this not being readonly means the node can corrupt it on crash, should this work different!?
    m_ImportedNodeSemaphore.InitFromImport(false, parameters.nodeSemaphoreExternalHandle);

    m_ImportedGlobalDescriptor.InitFromImport(k_TempSharedCamMemoryName);

    return MXC_SUCCESS;
}
