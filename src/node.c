#include "node.h"

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <stdio.h>
#include <stdlib.h>

#define SOCKET_PATH "C:\\temp\\moxaic_socket"
#define BUFFER_SIZE 128

static pthread_t serverThreadId;
static SOCKET listenSocket = INVALID_SOCKET;
const static char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const static char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

static void* runIPCServerConnection(void* arg) {
  printf("IPC Server Connection Thread Started\n");

  return NULL;
}

static void* runIPCServer(void* arg) {
  int         result;
  char        buffer[BUFFER_SIZE];
  SOCKADDR_UN address = {};
  WSADATA     wsaData = {};

  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    printf("WSAStartup failed: %d\n", result);
    goto Exit;
  }

  listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenSocket == INVALID_SOCKET) {
    printf("Socket failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  address.sun_family = AF_UNIX;
  result = strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1);
  if (result != 0) {
    printf("Address copy failed: %d\n", result);
    goto Exit;
  }

  result = bind(listenSocket, (struct sockaddr*)&address, sizeof(address));
  if (result == SOCKET_ERROR) {
    printf("Bind failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  result = listen(listenSocket, SOMAXCONN);
  if (result == SOCKET_ERROR) {
    printf("Listen failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  while (isRunning) {
    printf("Accepting connections on: '%s'\n", SOCKET_PATH);
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
      printf("Accept failed: %d\n", WSAGetLastError());
      continue;
    }
    printf("Accepted Connection.\n");

    int len = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (len == SOCKET_ERROR) {
      printf("Recv failed: %d", WSAGetLastError());
      goto ClientExit;
    }

    buffer[len] = '\0';
    printf("Received from node: %s\n", buffer);
    if (strcmp(buffer, nodeIPCAckMessage)) {
      printf("Unexpected node message.\n");
      goto ClientExit;
    }

    int sendResult = send(clientSocket, serverIPCAckMessage, (int)strlen(serverIPCAckMessage), 0);
    if (sendResult == SOCKET_ERROR) {
      printf("Send failed: %d\n", WSAGetLastError());
      goto ClientExit;
    }
    printf("Sent ack message %zu bytes: '%s'\n", strlen(serverIPCAckMessage), serverIPCAckMessage);

    MxcImportParam importParam = {

    };

  ClientExit:
    closesocket(clientSocket);
  }

Exit:
  if (listenSocket != INVALID_SOCKET) {
    closesocket(listenSocket);
  }
  // Analogous to `unlink`
  DeleteFileA(SOCKET_PATH);
  WSACleanup();
  return NULL;
}

void mxcInitializeIPCServer() {
  int result = pthread_create(&serverThreadId, NULL, runIPCServer, NULL);
  REQUIRE(result == 0, "IPC server pipe creation Fail!");
}

void mxcShutdownIPCServer() {
  int cancelResult = pthread_cancel(serverThreadId);
  if (cancelResult != 0) perror("IPC server thread cancel Fail!");
  if (listenSocket != INVALID_SOCKET) {
    closesocket(listenSocket);
  }
  DeleteFileA(SOCKET_PATH);
  WSACleanup();
}

void mxcConnectNodeIPC() {
  int         result, len;
  char        buffer[BUFFER_SIZE];
  SOCKET clientSocket = INVALID_SOCKET;
  SOCKADDR_UN address = {};
  WSADATA     wsaData = {};

  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    printf("WSAStartup failed with error: %d\n", result);
    goto Exit;
  }

  clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (clientSocket == INVALID_SOCKET) {
    printf("Socket creation failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

  result = connect(clientSocket, (struct sockaddr*)&address, sizeof(address));
  if (result == SOCKET_ERROR) {
    printf("Connect failed: %d", WSAGetLastError());
    goto Exit;
  }
  printf("Connected to server.\n");

  result = send(clientSocket, nodeIPCAckMessage, (int)strlen(nodeIPCAckMessage), 0);
  if (result == SOCKET_ERROR) {
    printf("Send failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  len = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
  if (len == SOCKET_ERROR) {
    printf("Recv failed: %d\n", WSAGetLastError());
    goto Exit;
  }

  buffer[len] = '\0';
  printf("Received from server: %s\n", buffer);
  if (strcmp(buffer, serverIPCAckMessage)) {
    printf("Unexpected server message.\n");
    goto Exit;
  }

  printf("Waiting to receive node import data.\n");
  len = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
  if (len == SOCKET_ERROR) {
    printf("Recv failed: %d\n", WSAGetLastError());
    goto Exit;
  }

Exit:
  if (clientSocket != INVALID_SOCKET)
    closesocket(clientSocket);
  WSACleanup();
}

void mxcShutdownNodeIPC() {
}

MxcCompNodeShared compNodeShared;

NodeHandle    nodesAvailable[MXC_NODE_CAPACITY];
size_t        nodeCount = 0;
MxcNode       nodes[MXC_NODE_CAPACITY] = {};
MxcNodeShared nodesShared[MXC_NODE_CAPACITY] = {};
MxcNodeCompData nodeCompData[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}

void mxcRegisterNodeThread(NodeHandle handle) {
  nodesShared[handle] = (MxcNodeShared){};
  nodesShared[handle].active = true;
  nodesShared[handle].radius = 0.5;

  nodeCompData[handle].cmd = nodes[handle].cmd;
  nodeCompData[handle].type = nodes[handle].nodeType;
  nodeCompData[handle].nodeTimeline = nodes[handle].nodeTimeline;
  nodeCompData[handle].transform.rotation = QuatFromEuler(nodeCompData[handle].transform.euler);
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    nodeCompData[handle].framebufferImages[i].color = nodes[handle].framebufferTextures[i].color.image;
    nodeCompData[handle].framebufferImages[i].normal = nodes[handle].framebufferTextures[i].normal.image;
    nodeCompData[handle].framebufferImages[i].gBuffer = nodes[handle].framebufferTextures[i].gBuffer.image;
    nodeCompData[handle].framebufferViews[i].color = nodes[handle].framebufferTextures[i].color.view;
    nodeCompData[handle].framebufferViews[i].normal = nodes[handle].framebufferTextures[i].normal.view;
    nodeCompData[handle].framebufferViews[i].gBuffer = nodes[handle].framebufferTextures[i].gBuffer.view;
  }
  nodeCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  mxcRunNode(&nodes[handle]);
}

void mxcRequestNodeThread(const VkSemaphore compTimeline, void* (*runFunc)(const struct MxcNode*), const void* pNode, NodeHandle* pNodeHandle) {
  NodeHandle      nodeHandle = 0;
  MxcNode* pNodeContext = &nodes[nodeHandle];

  // Should I create every time? No. Should probably release so it doesn't take up memory
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
  vkmCreateTimeline(&pNodeContext->nodeTimeline);


  pNodeContext->compTimeline = compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_THREAD;
  pNodeContext->pNode = pNode;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->runFunc = runFunc;

  *pNodeHandle = nodeHandle;
}

void mxcRequestNodeProcess(const VkSemaphore compTimeline, void* (*runFunc)(const struct MxcNode*), const void* pNode, NodeHandle* pNodeHandle) {
  NodeHandle      nodeHandle = 0;
  MxcNode* pNodeContext = &nodes[nodeHandle];

  mxcCreateNodeFramebufferExport(VKM_LOCALITY_INTERPROCESS_EXPORTED, pNodeContext->framebufferTextures);
  vkmCreateTimeline(&pNodeContext->nodeTimeline);

  pNodeContext->compTimeline = compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_INTERPROCESS;
  pNodeContext->pNode = pNode;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->runFunc = runFunc;

  *pNodeHandle = nodeHandle;
}

void mxcRunNode(const MxcNode* pNodeContext) {
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