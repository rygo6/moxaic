#include "node.h"
#include "test_node.h"

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <semaphore.h>

#define SOCKET_PATH "C:\\temp\\moxaic_socket"
#define BUFFER_SIZE 128

static struct {
  SOCKET          listenSocket;
  pthread_t       thread;
} ipcServer;

const static char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const static char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

#define ERR_CHECK(_command, _message)                 \
  {                                                   \
    int _result = _command;                           \
    if (__builtin_expect(!!(_result), 0)) {           \
      fprintf(stderr, "%s: %d\n", _message, _result); \
      goto Exit;                                      \
    }                                                 \
  }
#define WSA_ERR_CHECK(_command, _message)                       \
  {                                                             \
    int _result = _command;                                     \
    if (__builtin_expect(!!(_result), 0)) {                     \
      fprintf(stderr, "%s: %d\n", _message, WSAGetLastError()); \
      goto Exit;                                                \
    }                                                           \
  }

static void acceptIPCServer() {
  printf("Accepting connections on: '%s'\n", SOCKET_PATH);
  int  receiveLength, sendResult;

  SOCKET clientSocket = accept(ipcServer.listenSocket, NULL, NULL);
  WSA_ERR_CHECK(clientSocket == INVALID_SOCKET, "Accept failed");
  printf("Accepted Connection.\n");

  // Receive Node Ack Message
  {
    char buffer[sizeof(nodeIPCAckMessage)] = {};
    receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
    WSA_ERR_CHECK(receiveLength == SOCKET_ERROR, "Recv nodeIPCAckMessage failed");
    printf("Received node ack: %s size: %d\n", buffer, receiveLength);
    ERR_CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
  }

  // Send Server Ack message
  printf("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
  sendResult = send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
  WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send failed");

  // Receive Node Process Handle
  DWORD nodeProcessId = 0;
  receiveLength = recv(clientSocket, &nodeProcessId, sizeof(DWORD), 0);
  WSA_ERR_CHECK(receiveLength == SOCKET_ERROR, "Recv node process id failed");
  printf("Received node process id: %lu size: %d\n", nodeProcessId, receiveLength);
  ERR_CHECK(nodeProcessId == 0, "Invalid node process id");

  NodeHandle  processNodeHandle;
  mxcRequestNodeProcess(compNodeContext.compTimeline, &processNodeHandle);

  MxcNodeContext* pNodeContext = &nodeContexts[processNodeHandle];
  pNodeContext->processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcessId);
  printf("Node process hand duplicated: %p\n", pNodeContext->processHandle);
  ERR_CHECK(pNodeContext->processHandle == INVALID_HANDLE_VALUE, "Duplicate process handle failed");

  const HANDLE currentHandle = GetCurrentProcess();
  MxcImportParam importParam;
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    DuplicateHandle(currentHandle, pNodeContext->framebufferTextures[i].color.externalHandle, pNodeContext->processHandle, &importParam.framebufferHandles[i].color, 0, false, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(currentHandle, pNodeContext->framebufferTextures[i].color.externalHandle, pNodeContext->processHandle, &importParam.framebufferHandles[i].normal, 0, false, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(currentHandle, pNodeContext->framebufferTextures[i].color.externalHandle, pNodeContext->processHandle, &importParam.framebufferHandles[i].gbuffer, 0, false, DUPLICATE_SAME_ACCESS);
  }
  // need to export timelines
  //    DuplicateHandle(currentHandle, pNode->compTimeline, nodeProcessHandle, &importParam.compTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS);
  //    DuplicateHandle(currentHandle, pNode->nodeTimeline, nodeProcessHandle, &importParam.nodeTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS);

  nodesShared[processNodeHandle].active = true;

 Exit:
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
    WSA_ERR_CHECK(receiveLength == SOCKET_ERROR, "Recv compositor ack failed");
    printf("Received from server: %s size: %d\n", buffer, receiveLength);
    WSA_ERR_CHECK(strcmp(buffer, serverIPCAckMessage), "Unexpected compositor ack");
  }

  const DWORD currentProcessId = GetCurrentProcessId();
  printf("Sending process handle: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
  sendResult = send(clientSocket, &currentProcessId, sizeof(DWORD), 0);
  WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send process id failed");

  printf("Waiting to receive node import data.\n");
  MxcImportParam importParam;
  receiveLength = recv(clientSocket, &importParam, sizeof(MxcImportParam), 0);
  WSA_ERR_CHECK(receiveLength == SOCKET_ERROR, "Recv import data failed");

Exit:
  if (clientSocket != INVALID_SOCKET)
    closesocket(clientSocket);
  WSACleanup();
}
void mxcShutdownNodeIPC() {

}

MxcCompNodeContext compNodeContext;
MxcCompNodeShared compNodeShared;

NodeHandle    nodesAvailable[MXC_NODE_CAPACITY];
size_t        nodeCount = 0;
MxcNodeContext  nodeContexts[MXC_NODE_CAPACITY] = {};
MxcNodeShared nodesShared[MXC_NODE_CAPACITY] = {};
MxcNodeCompData nodeCompData[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}

void mxcRequestAndRunCompNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompNodeContext*)) {
  const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
  };
  MIDVK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &compNodeContext.pool));
  const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = compNodeContext.pool,
      .commandBufferCount = 1,
  };
  MIDVK_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &compNodeContext.cmd));
  vkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compNodeContext.cmd, "CompCmd");
  vkmCreateTimeline(VKM_LOCALITY_CONTEXT, &compNodeContext.compTimeline);
  vkmCreateSwap(surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compNodeContext.swap);

  compNodeShared = (MxcCompNodeShared){};

  int result = pthread_create(&compNodeContext.threadId, NULL, (void* (*)(void*))runFunc, &compNodeContext);
  REQUIRE(result == 0, "Comp Node thread creation failed!");
  printf("Request and Run CompNode Thread Success.");
}

void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pHandle) {
  NodeHandle      handle = 0;
  MxcNodeContext* pNodeContext = &nodeContexts[handle];

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
  mxcCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebufferTextures);
  vkmCreateTimeline(VKM_LOCALITY_CONTEXT, &pNodeContext->nodeTimeline);

  pNodeContext->compTimeline = compNodeContext.compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_THREAD;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->runFunc = runFunc;

  *pHandle = handle;

  printf("Request Node Thread Success. Handle: %d\n", handle);
}
void mxcRunNodeThread(NodeHandle handle) {
  nodesShared[handle] = (MxcNodeShared){};
  nodesShared[handle].active = true;
  nodesShared[handle].radius = 0.5;
  nodeCompData[handle].cmd = nodeContexts[handle].cmd;
  nodeCompData[handle].type = nodeContexts[handle].nodeType;
  nodeCompData[handle].nodeTimeline = nodeContexts[handle].nodeTimeline;
  nodeCompData[handle].transform.rotation = QuatFromEuler(nodeCompData[handle].transform.euler);
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    nodeCompData[handle].framebufferImages[i].color = nodeContexts[handle].framebufferTextures[i].color.image;
    nodeCompData[handle].framebufferImages[i].normal = nodeContexts[handle].framebufferTextures[i].normal.image;
    nodeCompData[handle].framebufferImages[i].gBuffer = nodeContexts[handle].framebufferTextures[i].gBuffer.image;
    nodeCompData[handle].framebufferViews[i].color = nodeContexts[handle].framebufferTextures[i].color.view;
    nodeCompData[handle].framebufferViews[i].normal = nodeContexts[handle].framebufferTextures[i].normal.view;
    nodeCompData[handle].framebufferViews[i].gBuffer = nodeContexts[handle].framebufferTextures[i].gBuffer.view;
  }
  nodeCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  mxcRunNode(&nodeContexts[handle]);
  printf("Run Node Thread Success. Handle: %d\n", handle);
}
void mxcReleaseNodeThread(NodeHandle handle) {
  PANIC("Not Implemented");
  printf("Release Node Thread Success. Handle: %d\n", handle);
}

void mxcRequestNodeProcess(const VkSemaphore compTimeline, NodeHandle* pNodeHandle) {
  NodeHandle      nodeHandle = 1;
  MxcNodeContext* pNodeContext = &nodeContexts[nodeHandle];

  mxcCreateNodeFramebufferExport(VKM_LOCALITY_INTERPROCESS_EXPORTED, pNodeContext->framebufferTextures);
  vkmCreateTimeline(VKM_LOCALITY_INTERPROCESS_EXPORTED, &pNodeContext->nodeTimeline);

  pNodeContext->compTimeline = compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_INTERPROCESS;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->processHandle = INVALID_HANDLE_VALUE;

  *pNodeHandle = nodeHandle;

  printf("Node Request Process Success. Handle: %d\n", nodeHandle);
}

void mxcRunNode(const MxcNodeContext* pNodeContext) {
  switch (pNodeContext->nodeType) {
    case MXC_NODE_TYPE_THREAD:
      int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))pNodeContext->runFunc, pNodeContext);
      REQUIRE(result == 0, "Node thread creation failed!");
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      mxcCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebufferTextures);
      break;
    default: PANIC("Node type not available!");
  }
}

static const VkImageUsageFlags VKM_PASS_NODE_USAGES[] = {
    [MIDVK_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    [MIDVK_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
};
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
              .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
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

void mxcCreateNodeFramebufferImport(const MidLocality locality, const MxcNodeFramebufferTexture* pNodeFramebuffers, MxcNodeFramebufferTexture* pFramebufferTextures) {
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    pFramebufferTextures[i].color = pNodeFramebuffers[i].color;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].color.image, "ImportedColorFramebuffer");
    pFramebufferTextures[i].normal = pNodeFramebuffers[i].normal;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].normal.image, "ImportedNormalFramebuffer");
    pFramebufferTextures[i].gBuffer = pNodeFramebuffers[i].gBuffer;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].gBuffer.image, "ImportedGBufferFramebuffer");
    // Depth is not shared over IPC. It goes in Gbuffer if needed.
    const VkmTextureCreateInfo depthCreateInfo = {
        .debugName = "ImportedDepthFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == VKM_LOCALITY_INTERPROCESS_EXPORTED ? &VKM_EXTERNAL_IMAGE_CREATE_INFO : NULL,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
            .extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_STD_DEPTH_INDEX],
        },
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .locality = locality,
    };
    vkmCreateTexture(&depthCreateInfo, &pFramebufferTextures[i].depth);
  }
}
void mxcCreateNodeFramebufferExport(const MidLocality locality, MxcNodeFramebufferTexture* pNodeFramebufferTextures) {
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    const VkmTextureCreateInfo colorCreateInfo = {
        .debugName = "ExportedColorFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == VKM_LOCALITY_INTERPROCESS_EXPORTED ? &VKM_EXTERNAL_IMAGE_CREATE_INFO : NULL,
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
    vkmCreateTexture(&colorCreateInfo, &pNodeFramebufferTextures[i].color);
    const VkmTextureCreateInfo normalCreateInfo = {
        .debugName = "ExportedNormalFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == VKM_LOCALITY_INTERPROCESS_EXPORTED ? &VKM_EXTERNAL_IMAGE_CREATE_INFO : NULL,
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
    vkmCreateTexture(&normalCreateInfo, &pNodeFramebufferTextures[i].normal);
    const VkmTextureCreateInfo depthCreateInfo = {
        .debugName = "ExportedGBufferFramebuffer",
        .imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == VKM_LOCALITY_INTERPROCESS_EXPORTED ? &VKM_EXTERNAL_IMAGE_CREATE_INFO : NULL,
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
    vkmCreateTexture(&depthCreateInfo, &pNodeFramebufferTextures[i].gBuffer);
  }
}