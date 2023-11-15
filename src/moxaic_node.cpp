#include "moxaic_node.hpp"

#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"

using namespace Moxaic;

const char k_TempSharedProducerName[] = "IPCProducer";
const char k_TempSharedCamMemoryName[] = "IPCCamera";

static void TargetFuncImportCompositor(void *params)
{

}

static MXC_RESULT StartProcess(STARTUPINFO &si, PROCESS_INFORMATION &pi)
{
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char buf[256];

    snprintf(buf, sizeof(buf), "moxaic.exe -node");
    MXC_LOG("Process Command", buf);

    if (!CreateProcess(NULL, // No module name (use command line)
                       buf, // Command line
                       NULL,  // Process handle not inheritable
                       NULL, // Thread handle not inheritable
                       FALSE, // Set handle inheritance to FALSE
                       0, // No creation flags
                       NULL, // Use parent's environment block
                       NULL, // Use parent's starting directory
                       &si, // Pointer to STARTUPINFO structure
                       &pi)) { // Pointer to PROCESS_INFORMATION structure
        MXC_LOG_ERROR("CreateProcess fail");
        return MXC_FAIL;
    }

    return MXC_SUCCESS;
}

CompositorNode::CompositorNode(const Vulkan::Device &device)
        : k_Device(device) {}

CompositorNode::~CompositorNode()
{
    WaitForSingleObject(m_ProcessInformation.hProcess, INFINITE);
    CloseHandle(m_ProcessInformation.hProcess);
    CloseHandle(m_ProcessInformation.hThread);
}

MXC_RESULT CompositorNode::Init()
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

Node::Node(const Vulkan::Device &device)
        : k_Device(device) {}

Node::~Node() = default;

MXC_RESULT Node::Init()
{
//    auto lambda = [this](void* pParameters) {
//        auto pImportParameters = (ImportParam*)pParameters;
//        this->InitImport(*pImportParameters);
//    };
//    const std::array test{
//        lambda
//    };
    const std::array<InterProcessFunc, 1> targetFuncs{
            TargetFuncImportCompositor
    };
    m_IPCFromCompositor.Init(k_TempSharedProducerName, std::move(targetFuncs));
    m_ImportedGlobalDescriptor.Init(k_TempSharedCamMemoryName);
    return MXC_SUCCESS;
}

MXC_RESULT Node::InitImport(ImportParam &parameters)
{
    return MXC_SUCCESS;
}
