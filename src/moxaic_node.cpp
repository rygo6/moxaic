#include "moxaic_node.hpp"

#include "moxaic_logging.hpp"

using namespace Moxaic;

static MXC_RESULT CreateProcess(STARTUPINFO &si, PROCESS_INFORMATION &pi)
{
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    ZeroMemory(&pi, sizeof(pi));

    char buf[256];

    snprintf(buf, sizeof(buf), "fabric.exe -child");
    MXC_LOG("Process Command", buf);

    if (!CreateProcess(NULL,   // No module name (use command line)
                       buf,        // Command line
                       NULL,           // Process handle not inheritable
                       NULL,           // Thread handle not inheritable
                       FALSE,          // Set handle inheritance to FALSE
                       0,              // No creation flags
                       NULL,           // Use parent's environment block
                       NULL,           // Use parent's starting directory
                       &si,            // Pointer to STARTUPINFO structure
                       &pi)           // Pointer to PROCESS_INFORMATION structure
            ) {
        MXC_LOG_ERROR("CreateProcess fail");
        return MXC_FAIL;
    }

    return MXC_SUCCESS;
}

static void CleanupProcess(const PROCESS_INFORMATION &pi)
{
    // TODO this is probably bad
    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

Node::Node(const VulkanDevice &device)
        : k_Device(device)
{

}

Node::~Node() = default;

MXC_RESULT Node::Init()
{
    m_NodeSemaphore.Init(false, Locality::External);


}

MXC_RESULT Node::InitFromImport(const ImportParam &param)
{

}