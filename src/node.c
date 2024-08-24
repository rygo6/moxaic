#include "node.h"
#include "test_node.h"

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <afunix.h>
#include <stdio.h>

#include <pthread.h>
#include <assert.h>

#define SOCKET_PATH "C:\\temp\\moxaic_socket"

static struct {
  SOCKET          listenSocket;
  pthread_t       thread;
} ipcServer;
const static char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const static char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

// Checks error code. Expects 0 for success.
#define ERR_CHECK(_command, _message)                        \
  {                                                          \
    int _result = (_command);                                \
    if (__builtin_expect(!!(_result), 0)) {                  \
      fprintf(stderr, "%s: %ld\n", _message, GetLastError()); \
      goto Exit;                                             \
    }                                                        \
  }
#define WSA_ERR_CHECK(_command, _message)                       \
  {                                                             \
    int _result = (_command);                                   \
    if (__builtin_expect(!!(_result), 0)) {                     \
      fprintf(stderr, "%s: %d\n", _message, WSAGetLastError()); \
      goto Exit;                                                \
    }                                                           \
  }

static void acceptIPCServer() {
  printf("Accepting connections on: '%s'\n", SOCKET_PATH);
  int  receiveLength, sendResult;
  MxcNodeContext* pProcessNodeContext = NULL;
  MxcNodeShared* pProcessNodeShared = NULL;

  SOCKET clientSocket = accept(ipcServer.listenSocket, NULL, NULL);
  WSA_ERR_CHECK(clientSocket == INVALID_SOCKET, "Accept failed");
  printf("Accepted Connection.\n");

  // Receive Node Ack Message
  {
    char buffer[sizeof(nodeIPCAckMessage)] = {};
    receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
    WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv nodeIPCAckMessage failed");
    printf("Received node ack: %s Size: %d\n", buffer, receiveLength);
    ERR_CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
  }

  // Send Server Ack message
  printf("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
  sendResult = send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
  WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send failed");

  // Receive Node Process Handle
  DWORD nodeProcessId = 0;
  receiveLength = recv(clientSocket, &nodeProcessId, sizeof(DWORD), 0);
  WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node process id failed");
  printf("Received node process id: %lu Size: %d\n", nodeProcessId, receiveLength);
  ERR_CHECK(nodeProcessId == 0, "Invalid node process id");

  // Create external handles
  HANDLE nodeProcessHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcessId);
  ERR_CHECK(nodeProcessHandle == INVALID_HANDLE_VALUE, "Duplicate process handle failed");
  HANDLE externalNodeMemoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
  ERR_CHECK(externalNodeMemoryHandle == NULL, "Could not create file mapping object");
  MxcExternalNodeMemory* pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
  ERR_CHECK(pExternalNodeMemory == NULL, "Could not map view of file");

  // Retrieve node handles
  NodeHandle  processNodeHandle;
  mxcRequestNodeProcessExport(compositorNodeContext.compTimeline, &processNodeHandle);
  // todo this all needs to go in mxcRequestNodeProcessExport so that it increments nodeCount after allis created
  pProcessNodeContext = &nodeContexts[processNodeHandle];
  pProcessNodeContext->processId = nodeProcessId;
  pProcessNodeContext->processHandle = nodeProcessHandle;
  pProcessNodeContext->externalMemoryHandle = externalNodeMemoryHandle;
  pProcessNodeContext->pExternalMemory = pExternalNodeMemory;
  pProcessNodeContext->pNodeShared = &pExternalNodeMemory->nodeShared;

  { // Send Import Params
    const HANDLE currentHandle = GetCurrentProcess();
    MxcImportParam* pImportParam = &pExternalNodeMemory->importParam;
    for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
      DuplicateHandle(currentHandle, GetMemoryExternalHandle(pProcessNodeContext->framebufferTextures[i].color.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].color, 0, false, DUPLICATE_SAME_ACCESS);
      DuplicateHandle(currentHandle, GetMemoryExternalHandle(pProcessNodeContext->framebufferTextures[i].normal.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].normal, 0, false, DUPLICATE_SAME_ACCESS);
      DuplicateHandle(currentHandle, GetMemoryExternalHandle(pProcessNodeContext->framebufferTextures[i].gbuffer.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].gbuffer, 0, false, DUPLICATE_SAME_ACCESS);
    }
    DuplicateHandle(currentHandle, GetSemaphoreExternalHandle(pProcessNodeContext->nodeTimeline), nodeProcessHandle, &pImportParam->nodeTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(currentHandle, GetSemaphoreExternalHandle(compositorNodeContext.compTimeline), nodeProcessHandle, &pImportParam->compTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS);

    HANDLE duplicatedExternalNodeMemoryHandle;
    DuplicateHandle(currentHandle, pProcessNodeContext->externalMemoryHandle, nodeProcessHandle, &duplicatedExternalNodeMemoryHandle, 0, false, DUPLICATE_SAME_ACCESS);

    printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
    sendResult = send(clientSocket, &duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0);
    WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send failed");
  }

  MxcNodeShared* pNodeShared = &pExternalNodeMemory->nodeShared;
  *pNodeShared = (MxcNodeShared){};
  pNodeShared->radius = 0.5;
  pNodeShared->compCycleSkip = 16;

  //todo move into mxcRequestNodeProcessExport
  nodeCount++;
  threadNodesShared[threadNodesCount] = pNodeShared;
  threadNodesCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);

  printf("Process Node Export Success.\n");
  goto ExitSuccess;

 Exit:
  if (pProcessNodeContext != NULL && pProcessNodeContext->pExternalMemory != NULL)
    UnmapViewOfFile(pExternalNodeMemory);
  if (pProcessNodeContext != NULL && pProcessNodeContext->externalMemoryHandle != NULL)
    CloseHandle(pProcessNodeContext->externalMemoryHandle);
 ExitSuccess:
  if (clientSocket != INVALID_SOCKET)
    closesocket(clientSocket);
}

static void* runIPCServer(void* arg) {
  SOCKADDR_UN address = {.sun_family = AF_UNIX};
  WSADATA     wsaData = {};

  // Unlink/delete sock file in case it was left from before
  unlink(SOCKET_PATH);

  ERR_CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");
  ERR_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");

  ipcServer.listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  WSA_ERR_CHECK(ipcServer.listenSocket == INVALID_SOCKET, "Socket failed");
  WSA_ERR_CHECK(bind(ipcServer.listenSocket, (struct sockaddr*)&address, sizeof(address)), "Bind failed");
  WSA_ERR_CHECK(listen(ipcServer.listenSocket, SOMAXCONN), "Listen failed");

  while (isRunning) {
    acceptIPCServer();
  }

Exit:
  if (ipcServer.listenSocket != INVALID_SOCKET) {
    closesocket(ipcServer.listenSocket);
  }
  unlink(SOCKET_PATH);
  WSACleanup();
  return NULL;
}
void mxcInitializeIPCServer() {
  SYSTEM_INFO systemInfo;
  GetSystemInfo(&systemInfo);
  printf("Allocation granularity: %lu\n", systemInfo.dwAllocationGranularity);

  ipcServer.listenSocket = INVALID_SOCKET;
  REQUIRE_ERR(pthread_create(&ipcServer.thread, NULL, runIPCServer, NULL), "IPC server pipe creation Fail!");
}
void mxcShutdownIPCServer() {
  if (ipcServer.listenSocket != INVALID_SOCKET) {
    closesocket(ipcServer.listenSocket);
  }
  unlink(SOCKET_PATH);
  WSACleanup();
}

void mxcConnectNodeIPC() {
  printf("Connecting on: '%s'\n", SOCKET_PATH);
  int  receiveLength, sendResult;
  SOCKET clientSocket = INVALID_SOCKET;

  SOCKADDR_UN address = {.sun_family = AF_UNIX};
  ERR_CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");

  WSADATA wsaData = {};
  ERR_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");

  clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  WSA_ERR_CHECK(clientSocket == INVALID_SOCKET, "Socket creation failed");

  WSA_ERR_CHECK(connect(clientSocket, (struct sockaddr*)&address, sizeof(address)), "Connect failed");
  printf("Connected to server.\n");

  printf("Sending node ack: %s size: %llu\n", nodeIPCAckMessage, strlen(nodeIPCAckMessage));
  sendResult = send(clientSocket, nodeIPCAckMessage, (int)strlen(nodeIPCAckMessage), 0);
  WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send node ack failed");

  {
    char buffer[sizeof(serverIPCAckMessage)] = {};
    receiveLength = recv(clientSocket, buffer, sizeof(serverIPCAckMessage), 0);
    WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv compositor ack failed");
    printf("Received from server: %s size: %d\n", buffer, receiveLength);
    WSA_ERR_CHECK(strcmp(buffer, serverIPCAckMessage), "Unexpected compositor ack");
  }

  const DWORD currentProcessId = GetCurrentProcessId();
  printf("Sending process handle: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
  sendResult = send(clientSocket, &currentProcessId, sizeof(DWORD), 0);
  WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send process id failed");

  printf("Waiting to receive externalNodeMemoryHandle.\n");
  HANDLE externalNodeMemoryHandle;
  receiveLength = recv(clientSocket, &externalNodeMemoryHandle, sizeof(HANDLE), 0);
  WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv externalNodeMemoryHandle failed");
  printf("Received externalNodeMemoryHandle: %p Size: %d\n", externalNodeMemoryHandle, receiveLength);

  MxcExternalNodeMemory* pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
  ERR_CHECK(pExternalNodeMemory == NULL, "Map pExternalNodeMemory failed");

  NodeHandle nodeHandle;
  // dont need to pass import param separately
  mxcRequestNodeProcessImport(externalNodeMemoryHandle, pExternalNodeMemory, &pExternalNodeMemory->importParam, &nodeHandle);

Exit:
  if (clientSocket != INVALID_SOCKET)
    closesocket(clientSocket);
  WSACleanup();
}
void mxcShutdownNodeIPC() {

}

MxcCompositorNodeContext compositorNodeContext = {};

size_t                nodeCount = 0;
MxcNodeContext        nodeContexts[MXC_NODE_CAPACITY] = {};
MxcNodeShared         nodesShared[MXC_NODE_CAPACITY] = {};
MxcNodeCompositorData nodeCompositorData[MXC_NODE_CAPACITY] = {};
NodeHandle            nodesAvailable[MXC_NODE_CAPACITY] = {};

size_t         threadNodesCount = 0;
MxcNodeShared* threadNodesShared[MXC_NODE_CAPACITY] = {};

size_t         interprocessNodesCount = 0;
MxcNodeShared* interprocessNodesShared[MXC_NODE_CAPACITY] = {};


size_t                     submitNodeQueueStart = 0;
size_t                     submitNodeQueueEnd = 0;
MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY] = {};
void mxcRequestAndRunCompNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompositorNodeContext*)) {
  const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
  };
  MIDVK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &compositorNodeContext.pool));
  const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = compositorNodeContext.pool,
      .commandBufferCount = 1,
  };
  MIDVK_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &compositorNodeContext.cmd));
  vkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compositorNodeContext.cmd, "CompCmd");
  const MidVkSemaphoreCreateInfo semaphoreCreateInfo =  {
      .debugName = "CompTimelineSemaphore",
      .locality = MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
  };
  midvkCreateSemaphore(&semaphoreCreateInfo, &compositorNodeContext.compTimeline);
  vkmCreateSwap(surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorNodeContext.swap);

  int result = pthread_create(&compositorNodeContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorNodeContext);
  REQUIRE(result == 0, "Comp Node thread creation failed!");
  printf("Request and Run CompNode Thread Success.");
}


void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pNodeHandle) {
  const NodeHandle handle = nodeCount;

  MxcNodeContext* pNodeContext = &nodeContexts[handle];
  *pNodeContext = (MxcNodeContext){};
  pNodeContext->compTimeline = compositorNodeContext.compTimeline;
  pNodeContext->type = MXC_NODE_TYPE_THREAD;

  // mayube have the thread allocate this and submit it once ready to get rid of < 1 check
  // then could also get rid of pNodeContext->pNodeShared?
  MxcNodeShared* pNodeShared = &nodesShared[handle];
  *pNodeShared = (MxcNodeShared){};
  pNodeContext->pNodeShared = pNodeShared;
  pNodeShared->radius = 0.5;
  pNodeShared->compCycleSkip = 16;

  const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
  };
  MIDVK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
  const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pNodeContext->pool,
      .commandBufferCount = 1,
  };
  MIDVK_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
  vkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");
  mxcCreateNodeFramebuffer(MID_LOCALITY_CONTEXT, pNodeContext->framebufferTextures);
  const MidVkSemaphoreCreateInfo semaphoreCreateInfo =  {
      .debugName = "NodeTimelineSemaphore",
      .locality = MID_LOCALITY_CONTEXT,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
  };
  midvkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->nodeTimeline);

  MxcNodeCompositorData* pNodeCompositorData = &nodeCompositorData[handle];
  *pNodeCompositorData = (MxcNodeCompositorData){};
  pNodeCompositorData->transform.rotation = QuatFromEuler(pNodeCompositorData->transform.euler);
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    pNodeCompositorData->framebufferImages[i].color = nodeContexts[handle].framebufferTextures[i].color.image;
    pNodeCompositorData->framebufferImages[i].normal = nodeContexts[handle].framebufferTextures[i].normal.image;
    pNodeCompositorData->framebufferImages[i].gBuffer = nodeContexts[handle].framebufferTextures[i].gbuffer.image;
    pNodeCompositorData->framebufferImages[i].colorView = nodeContexts[handle].framebufferTextures[i].color.view;
    pNodeCompositorData->framebufferImages[i].normalView = nodeContexts[handle].framebufferTextures[i].normal.view;
    pNodeCompositorData->framebufferImages[i].gBufferView = nodeContexts[handle].framebufferTextures[i].gbuffer.view;
  }

  *pNodeHandle = handle;
  nodeCount++;

  threadNodesShared[threadNodesCount] = pNodeShared;
  threadNodesCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);

  int result = pthread_create(&nodeContexts[handle].threadId, NULL, (void* (*)(void*))runFunc, pNodeContext);
  REQUIRE(result == 0, "Node thread creation failed!");

  printf("Request Node Thread Success. Handle: %d\n", handle);
}
void mxcReleaseNodeThread(NodeHandle handle) {
  PANIC("Not Implemented");
  printf("Release Node Thread Success. Handle: %d\n", handle);
}

void mxcRequestNodeProcessExport(const VkSemaphore compTimeline, NodeHandle* pNodeHandle) {
  *pNodeHandle = nodeCount;

  MxcNodeContext* pNodeContext = &nodeContexts[*pNodeHandle];
  *pNodeContext = (MxcNodeContext){};
  pNodeContext->compTimeline = compTimeline;
  pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS;
  pNodeContext->processHandle = INVALID_HANDLE_VALUE;

  mxcCreateNodeFramebuffer(MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE, pNodeContext->framebufferTextures);
  const MidVkSemaphoreCreateInfo semaphoreCreateInfo =  {
      .debugName = "NodeTimelineSemaphoreExport",
      .locality = MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
  };
  midvkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->nodeTimeline);

//  MxcNodeShared* pNodeShared = &nodesShared[*pNodeHandle];
//  *pNodeShared = (MxcNodeShared){};
//  pNodeShared->radius = 0.5;
//  pNodeShared->compCycleSkip = 16;

  MxcNodeCompositorData* pNodeCompData = &nodeCompositorData[*pNodeHandle];
  *pNodeCompData = (MxcNodeCompositorData){};
  pNodeCompData->transform.rotation = QuatFromEuler(nodeCompositorData[*pNodeHandle].transform.euler);
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    pNodeCompData->framebufferImages[i].color = pNodeContext->framebufferTextures[i].color.image;
    pNodeCompData->framebufferImages[i].normal = pNodeContext->framebufferTextures[i].normal.image;
    pNodeCompData->framebufferImages[i].gBuffer = pNodeContext->framebufferTextures[i].gbuffer.image;
    pNodeCompData->framebufferImages[i].colorView = pNodeContext->framebufferTextures[i].color.view;
    pNodeCompData->framebufferImages[i].normalView = pNodeContext->framebufferTextures[i].normal.view;
    pNodeCompData->framebufferImages[i].gBufferView = pNodeContext->framebufferTextures[i].gbuffer.view;
  }
//  nodeCount++; // pull framebuffer create into here so can increment this here
  __atomic_thread_fence(__ATOMIC_RELEASE);
  printf("Node Request Process Export Success. Handle: %d\n", *pNodeHandle);
}
void mxcRequestNodeProcessImport(const HANDLE externalMemoryHandle, MxcExternalNodeMemory* pExternalMemory, const MxcImportParam* pImportParam, NodeHandle* pNodeHandle) {
  *pNodeHandle = nodeCount;

  // switch to MxcNodeProcessImportContext? maybe not
  MxcNodeContext* pNodeContext = &nodeContexts[*pNodeHandle];
  *pNodeContext = (MxcNodeContext){};
  pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS;
  pNodeContext->externalMemoryHandle = externalMemoryHandle;
  pNodeContext->pExternalMemory = pExternalMemory;
  pNodeContext->pNodeShared = &pExternalMemory->nodeShared;

  const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
  };
  MIDVK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
  const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pNodeContext->pool,
      .commandBufferCount = 1,
  };
  MIDVK_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
  vkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

  MxcNodeFramebufferTexture* pFramebufferTextures = pNodeContext->framebufferTextures;
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    const VkmTextureCreateInfo colorCreateInfo = {
        .debugName = "ImportedColorFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext =  MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
        .externalHandle = pImportParam->framebufferHandles[i].color,
    };
    midvkCreateTexture(&colorCreateInfo, &pFramebufferTextures[i].color);
    const VkmTextureCreateInfo normalCreateInfo = {
        .debugName = "ImportedNormalFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
        .externalHandle = pImportParam->framebufferHandles[i].normal,
    };
    midvkCreateTexture(&normalCreateInfo, &pFramebufferTextures[i].normal);
    const VkmTextureCreateInfo gbufferCreateInfo = {
        .debugName = "ImportedGBufferFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MXC_NODE_GBUFFER_FORMAT,
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = MXC_NODE_GBUFFER_LEVELS,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MXC_NODE_GBUFFER_USAGE,
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
        .externalHandle = pImportParam->framebufferHandles[i].gbuffer,
    };
    midvkCreateTexture(&gbufferCreateInfo, &pFramebufferTextures[i].gbuffer);
    // Depth is not shared over IPC.
    const VkmTextureCreateInfo depthCreateInfo = {
        .debugName = "ImportedDepthFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .locality = MID_LOCALITY_CONTEXT,
    };
    midvkCreateTexture(&depthCreateInfo, &pFramebufferTextures[i].depth);
  }
  const MidVkSemaphoreCreateInfo compTimelineCreateInfo =  {
      .debugName = "CompositorTimelineSemaphoreImport",
      .locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .externalHandle = pImportParam->compTimelineHandle,
  };
  midvkCreateSemaphore(&compTimelineCreateInfo, &pNodeContext->compTimeline);
  const MidVkSemaphoreCreateInfo nodeTimelineCreateInfo =  {
      .debugName = "NodeTimelineSemaphoreImport",
      .locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .externalHandle = pImportParam->nodeTimelineHandle,
  };
  midvkCreateSemaphore(&nodeTimelineCreateInfo, &pNodeContext->nodeTimeline);

  // do I want this on an imported node? Maybe would be nice to keep the ability for something to run it's own compositor for debug
//  MxcNodeCompositorData* pNodeCompData = &nodeCompositorData[*pNodeHandle];
//  *pNodeCompData = (MxcNodeCompositorData){};
//  pNodeCompData->cmd = pNodeContext->cmd;
//  pNodeCompData->nodeTimeline = pNodeContext->nodeTimeline;
//  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
//    pNodeCompData->framebufferImages[i].color = pNodeContext->framebufferTextures[i].color.image;
//    pNodeCompData->framebufferImages[i].normal = pNodeContext->framebufferTextures[i].normal.image;
//    pNodeCompData->framebufferImages[i].gBuffer = pNodeContext->framebufferTextures[i].gbuffer.image;
//    pNodeCompData->framebufferImages[i].colorView = pNodeContext->framebufferTextures[i].color.view;
//    pNodeCompData->framebufferImages[i].normalView = pNodeContext->framebufferTextures[i].normal.view;
//    pNodeCompData->framebufferImages[i].gBufferView = pNodeContext->framebufferTextures[i].gbuffer.view;
//  }

  nodeCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext);
  REQUIRE(result == 0, "Node Process Import thread creation failed!");
  printf("Node Request Process Import Success. Handle: %d\n", *pNodeHandle);
}

void mxcCreateNodeRenderPass() {
  const VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .attachmentCount = 3,
      .pAttachments = (const VkAttachmentDescription2[]){
          [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
          [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
          [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
      },
      .subpassCount = 1,
      .pSubpasses = (const VkSubpassDescription2[]){
          {
              .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
              .colorAttachmentCount = 2,
              .pColorAttachments = (const VkAttachmentReference2[]){
                  [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = {
                      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                      .attachment = MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX,
                      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  },
                  [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {
                      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                      .attachment = MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX,
                      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  },
              },
              .pDepthStencilAttachment = &(const VkAttachmentReference2){
                  .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                  .attachment = MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX,
                  .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              },
          },
      },
      .dependencyCount = 2,
      .pDependencies = (const VkSubpassDependency2[]){
          // from here https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#combined-graphicspresent-queue
          {
              .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
              .srcSubpass = VK_SUBPASS_EXTERNAL,
              .dstSubpass = 0,
              .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
              .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .dependencyFlags = 0,
          },
          {
              .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
              .srcSubpass = 0,
              .dstSubpass = VK_SUBPASS_EXTERNAL,
              .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
              .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
              .dependencyFlags = 0,
          },
      },
  };
  MIDVK_REQUIRE(vkCreateRenderPass2(context.device, &renderPassCreateInfo2, MIDVK_ALLOC, &context.nodeRenderPass));
  vkmSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)context.nodeRenderPass, "NodeRenderPass");
}

void mxcCreateNodeFramebuffer(const MidLocality locality, MxcNodeFramebufferTexture* pNodeFramebufferTextures) {
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    const VkmTextureCreateInfo colorCreateInfo = {
        .debugName = "ExportedColorFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = locality,
    };
    midvkCreateTexture(&colorCreateInfo, &pNodeFramebufferTextures[i].color);
    const VkmTextureCreateInfo normalCreateInfo = {
        .debugName = "ExportedNormalFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = locality,
    };
    midvkCreateTexture(&normalCreateInfo, &pNodeFramebufferTextures[i].normal);
    const VkmTextureCreateInfo gbufferCreateInfo = {
        .debugName = "ExportedGBufferFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MXC_NODE_GBUFFER_FORMAT,
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = MXC_NODE_GBUFFER_LEVELS,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MXC_NODE_GBUFFER_USAGE,
        },
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .locality = locality,
    };
    midvkCreateTexture(&gbufferCreateInfo, &pNodeFramebufferTextures[i].gbuffer);

    // Depth is not shared over IPC.
    if (locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY) {
      // we need to transition these out of undefined initially because the transition in the other process won't update layout to avoid initial validation error on transition
      VkCommandBuffer cmd = MidVKBeginImmediateTransferCommandBuffer();
      const VkImageMemoryBarrier2 interProcessBarriers[] = {
          VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].color.image),
          VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].normal.image),
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].gbuffer.image, 0, MXC_NODE_GBUFFER_LEVELS),
      };
      MIDVK_DEVICE_FUNC(CmdPipelineBarrier2);
      CmdPipelineImageBarriers2(cmd, COUNT(interProcessBarriers), interProcessBarriers);
      MidVKEndImmediateTransferCommandBuffer(cmd);
      continue;
    }
    const VkmTextureCreateInfo depthCreateInfo = {
        .debugName = "ImportedDepthFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .locality = MID_LOCALITY_CONTEXT,
    };
    midvkCreateTexture(&depthCreateInfo, &pNodeFramebufferTextures[i].depth);
  }
}