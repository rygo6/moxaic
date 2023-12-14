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
    : k_pDevice(pDevice) {}

NodeReference::~NodeReference()
{
    WaitForSingleObject(m_ProcessInformation.hProcess, INFINITE);
    CloseHandle(m_ProcessInformation.hProcess);
    CloseHandle(m_ProcessInformation.hThread);
}

MXC_RESULT NodeReference::Init(const Vulkan::PipelineType pipelineType)
{
    MXC_LOG("Init NodeReference");
    MXC_CHK(m_ExportedSemaphore.Init(false,
                                     Vulkan::Locality::External));

    m_IPCToNode.Init(k_TempSharedProducerName);
    m_ExportedGlobalDescriptor.Init(k_TempSharedCamMemoryName);

    for (int i = 0; i < m_ExportedFramebuffers.size(); ++i) {
        m_ExportedFramebuffers[i].Init(pipelineType,
                                       Window::extents(),
                                       Vulkan::Locality::External);
    }

    StartProcess(m_Startupinfo, m_ProcessInformation);

    return MXC_SUCCESS;
}

MXC_RESULT NodeReference::ExportOverIPC(const Vulkan::Semaphore& compositorSemaphore)
{
    const auto hProcess = m_ProcessInformation.hProcess;
    const Node::ImportParam importParam{
      .framebufferWidth = m_ExportedFramebuffers[0].GetExtents().width,
      .framebufferHeight = m_ExportedFramebuffers[0].GetExtents().height,
      .colorFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].GetColorTexture().ClonedExternalHandle(hProcess),
      .colorFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].GetColorTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].GetNormalTexture().ClonedExternalHandle(hProcess),
      .normalFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].GetNormalTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].GetGBufferTexture().ClonedExternalHandle(hProcess),
      .gBufferFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].GetGBufferTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer0ExternalHandle = m_ExportedFramebuffers[0].GetDepthTexture().ClonedExternalHandle(hProcess),
      .depthFramebuffer1ExternalHandle = m_ExportedFramebuffers[1].GetDepthTexture().ClonedExternalHandle(hProcess),
      .compositorSemaphoreExternalHandle = compositorSemaphore.ClonedExternalHandle(hProcess),
      .nodeSemaphoreExternalHandle = m_ExportedSemaphore.ClonedExternalHandle(hProcess),
    };
    m_IPCToNode.Enque(InterProcessTargetFunc::ImportCompositor, &importParam);

    return MXC_SUCCESS;
}

void NodeReference::SetZCondensedExportedGlobalDescriptorLocalBuffer(const Camera& camera)
{
    // Condense nearz/farz to draw radius of node
    const auto viewPosition = camera.view() * glm::vec4(m_Transform.position_, 1);
    const float viewDistanceToCenter = -viewPosition.z;
    const float offset = m_DrawRadius * 0.5f;
    const float farZ = viewDistanceToCenter + offset;
    float nearZ = viewDistanceToCenter - offset;
    if (nearZ < MXC_CAMERA_MIN_Z) {
        nearZ = MXC_CAMERA_MIN_Z;
    }
    auto& localBuffer = m_ExportedGlobalDescriptor.localBuffer_;
    localBuffer.view = camera.view();
    localBuffer.proj = glm::perspective(camera.fieldOfView(),
                                        camera.aspectRatio(),
                                        nearZ,
                                        farZ);
    localBuffer.viewProj = localBuffer.proj * localBuffer.view;
    localBuffer.invView = camera.inverseView();
    localBuffer.invProj = glm::inverse(localBuffer.proj);
    localBuffer.invViewProj = localBuffer.invView * localBuffer.invProj;
}

Node::Node(const Vulkan::Device* const pDevice)
    : k_pDevice(pDevice) {}

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
    m_ImportedNodeSemaphore.InitFromImport(false, parameters.nodeSemaphoreExternalHandle);

    m_ImportedGlobalDescriptor.InitFromImport(k_TempSharedCamMemoryName);

    return MXC_SUCCESS;
}
