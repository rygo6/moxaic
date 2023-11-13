#include "moxaic_node.hpp"

#include "moxaic_logging.hpp"
#include "moxaic_window.hpp"

using namespace Moxaic;

const char g_TempSharedMemoryName[] = "FbrIPCRingBuffer";
const char g_TempSharedCamMemoryName[] = "FbrIPCCamera";

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

CompositorNode::CompositorNode(const VulkanDevice &device)
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
                                         Locality::External));

    m_IPCToNode.Init();
    m_ExportedGlobalDescriptor.Init();

    for (int i = 0; i < m_ExportedFramebuffers.size(); ++i) {
        m_ExportedFramebuffers[i].Init(Window::extents(),
                                       Locality::External);
    }

    StartProcess(m_Startupinfo, m_ProcessInformation);

    return MXC_SUCCESS;
}

Node::Node(const VulkanDevice &device)
        : k_Device(device) {}

Node::~Node() = default;

MXC_RESULT Node::Init()
{


    return MXC_SUCCESS;
}
