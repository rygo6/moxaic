#include "node.h"

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <stdio.h>
#include <stdlib.h>

//#define SOCKET_PATH "moxaic.server.socket"
#define SOCKET_PATH "C:\\temp\\moxaic_socket"
#define BUFFER_SIZE 128

static pthread_t serverThreadId;
static int       serverConnectionCount;

static void* runIPCServerConnection(void* arg) {
  printf("IPC Server Connection Thread Started\n");

  return NULL;
}

SOCKET clientSocket = INVALID_SOCKET;
SOCKET listenSocket = INVALID_SOCKET;

static void* runIPCServer(void* arg) {
  int         result = 0;
  char        sendBuffer[] = "af_unix from Windows to WSL!";
  int         sendResult = 0;
  SOCKADDR_UN address = {};
  WSADATA     wsaData = {};

  // Initialize Winsock
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    printf("WSAStartup failed with error: %d\n", result);
    goto Exit;
  }

  // Create a AF_UNIX stream server socket.
  listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenSocket == INVALID_SOCKET) {
    printf("socket failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }

  address.sun_family = AF_UNIX;
  strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1);

  // Bind the socket to the path.
  result = bind(listenSocket, (struct sockaddr*)&address, sizeof(address));
  if (result == SOCKET_ERROR) {
    printf("bind failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }

  // Listen to start accepting connections.
  result = listen(listenSocket, SOMAXCONN);
  if (result == SOCKET_ERROR) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }

  printf("Accepting connections on: '%s'\n", SOCKET_PATH);
  // Accept a connection.
  clientSocket = accept(listenSocket, NULL, NULL);
  if (clientSocket == INVALID_SOCKET) {
    printf("accept failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }
  printf("Accepted a connection.\n");

  // Send some data.
  sendResult = send(clientSocket, sendBuffer, (int)strlen(sendBuffer), 0);
  if (sendResult == SOCKET_ERROR) {
    printf("send failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }
  printf("Relayed %zu bytes: '%s'\n", strlen(sendBuffer), sendBuffer);

  // shutdown the connection.
  printf("Shutting down\n");
  result = shutdown(clientSocket, 0);
  if (result == SOCKET_ERROR) {
    printf("shutdown failed with error: %d\n", WSAGetLastError());
    goto Exit;
  }

Exit:
  // cleanup
  if (listenSocket != INVALID_SOCKET) {
    closesocket(listenSocket);
  }

  if (clientSocket != INVALID_SOCKET) {
    closesocket(clientSocket);
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
  if (clientSocket != INVALID_SOCKET) {
    closesocket(clientSocket);
  }
  DeleteFileA(SOCKET_PATH);
  WSACleanup();
}

void mxcConnectIPCNode() {

  int         result, len;
  char*       message = "Hello from client!";
  char buffer[BUFFER_SIZE];
  SOCKADDR_UN address = {};
  WSADATA     wsaData = {};

  // Initialize Winsock
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    perror("WSAStartup failed");
    goto ErrorExit;
  }

  // Create a UNIX domain socket
  clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (clientSocket == INVALID_SOCKET) {
    perror("Socket creation failed");
    goto ErrorExit;
  }

  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

  result = connect(clientSocket, (struct sockaddr*)&address, sizeof(address));
  if (result == SOCKET_ERROR) {
    perror("Connect failed");
    goto ErrorExit;
  }

  printf("Connected to server.\n");

  len = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
  if (len == SOCKET_ERROR) {
    perror("Recv failed");
    goto ErrorExit;
  }

  buffer[len] = '\0'; // Null-terminate the string
  printf("Received from server: %s\n", buffer);

//  result = send(clientSocket, message, (int)strlen(message), 0);
//  if (result == SOCKET_ERROR) {
//    perror("Send failed");
//    goto ErrorExit;
//  }

ErrorExit:
  closesocket(clientSocket);
  WSACleanup();
}


void mxcShutdownIPCNode() {
}

MxcCompNodeContextShared compNodeShared;

size_t               nodeCount = 0;
MxcNodeContext       nodes[MXC_NODE_CAPACITY] = {};
MxcNodeContextShared nodesShared[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}

void mxcRegisterNodeContextThread(const NodeHandle handle, const VkCommandBuffer cmd) {
  nodesShared[handle] = (MxcNodeContextShared){};
  nodesShared[handle].cmd = cmd;
  nodesShared[handle].active = true;
  nodesShared[handle].radius = 0.5;
  nodesShared[handle].type = nodes[handle].nodeType;
  nodesShared[handle].nodeTimeline = nodes[handle].nodeTimeline;
  nodesShared[handle].transform.rotation = QuatFromEuler(nodesShared[handle].transform.euler);
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    nodesShared[handle].framebufferColorImageViews[i] = nodes[handle].framebuffers[i].color.view;
    nodesShared[handle].framebufferNormalImageViews[i] = nodes[handle].framebuffers[i].normal.view;
    nodesShared[handle].framebufferGBufferImageViews[i] = nodes[handle].framebuffers[i].gBuffer.view;
    nodesShared[handle].framebufferColorImages[i] = nodes[handle].framebuffers[i].color.img;
    nodesShared[handle].framebufferNormalImages[i] = nodes[handle].framebuffers[i].normal.img;
    nodesShared[handle].framebufferGBufferImages[i] = nodes[handle].framebuffers[i].gBuffer.img;
  }
  nodeCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  mxcRunNodeContext(&nodes[handle]);
}

void mxcRequestNodeContextThread(const VkSemaphore compTimeline, void* (*runFunc)(const struct MxcNodeContext*), const void* pNode, NodeHandle* pNodeHandle) {
  NodeHandle      nodeHandle = 0;
  MxcNodeContext* pNodeContext = &nodes[nodeHandle];

  // create every time? or recycle? recycle probnably better to free resource
  mxcCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebuffers);
  vkmCreateTimeline(&pNodeContext->nodeTimeline);
  pNodeContext->compTimeline = compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_THREAD;
  pNodeContext->pNode = pNode;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->runFunc = runFunc;

  *pNodeHandle = nodeHandle;
}

void mxcRunNodeContext(const MxcNodeContext* pNodeContext) {
  switch (pNodeContext->nodeType) {
    case MXC_NODE_TYPE_THREAD:
      int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))pNodeContext->runFunc, pNodeContext);
      REQUIRE(result == 0, "Node thread creation failed!");
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      mxcCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebuffers);
      break;
    default: PANIC("Node type not available!");
  }
}

static const VkImageUsageFlags VKM_PASS_NODE_USAGES[] = {
    [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
};
void mxcCreateNodeRenderPass() {
  const VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .attachmentCount = 3,
      .pAttachments = (const VkAttachmentDescription2[]){
          [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
          [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
          [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX],
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
                  [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = {
                      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                      .attachment = VKM_PASS_ATTACHMENT_STD_COLOR_INDEX,
                      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  },
                  [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {
                      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                      .attachment = VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX,
                      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  },
              },
              .pDepthStencilAttachment = &(const VkAttachmentReference2){
                  .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                  .attachment = VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX,
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
  VKM_REQUIRE(vkCreateRenderPass2(context.device, &renderPassCreateInfo2, VKM_ALLOC, &context.nodeRenderPass));
}

void mxcCreateNodeFramebufferImport(const VkmLocality locality, const VkmNodeFramebuffer* pNodeFramebuffers, VkmFramebuffer* pFrameBuffers) {
  const VkExternalMemoryImageCreateInfo externalImageInfo = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
  };
  VkmTextureCreateInfo textureCreateInfo = {
      .imageCreateInfo = VKM_DEFAULT_TEXTURE_IMAGE_CREATE_INFO,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .locality = locality,
  };
  switch (locality) {
    default:
    case VKM_LOCALITY_CONTEXT:          break;
    case VKM_LOCALITY_PROCESS:          break;
    case VKM_LOCALITY_PROCESS_EXPORTED: textureCreateInfo.imageCreateInfo.pNext = &externalImageInfo; break;
  }
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {

    pFrameBuffers[i].color = pNodeFramebuffers[i].color;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].color.img, "ImportedColorFramebuffer");
    pFrameBuffers[i].normal = pNodeFramebuffers[i].normal;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].normal.img, "ImportedNormalFramebuffer");
    pFrameBuffers[i].gBuffer = pNodeFramebuffers[i].gBuffer;
    vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pNodeFramebuffers[i].gBuffer.img, "ImportedGBufferFramebuffer");

    textureCreateInfo.imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    textureCreateInfo.debugName = "ImportedDepthFramebuffer";
    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].depth);

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = context.nodeRenderPass,
        .attachmentCount = VKM_PASS_ATTACHMENT_STD_COUNT,
        .pAttachments = (const VkImageView[]){
            [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = pFrameBuffers[i].color.view,
            [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = pFrameBuffers[i].normal.view,
            [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = pFrameBuffers[i].depth.view,
        },
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .layers = 1,
    };
    VKM_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VKM_ALLOC, &pFrameBuffers[i].framebuffer));
    VKM_REQUIRE(vkCreateSemaphore(context.device, &(VkSemaphoreCreateInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}, VKM_ALLOC, &pFrameBuffers[i].renderCompleteSemaphore));
  }
}
void mxcCreateNodeFramebufferExport(const VkmLocality locality, VkmNodeFramebuffer* pNodeFramebuffers) {
  const VkExternalMemoryImageCreateInfo externalImageInfo = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
  };
  VkmTextureCreateInfo textureCreateInfo = {
      .imageCreateInfo = VKM_DEFAULT_TEXTURE_IMAGE_CREATE_INFO,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .locality = locality,
  };
  textureCreateInfo.imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};
  switch (locality) {
    default:
    case VKM_LOCALITY_CONTEXT:          break;
    case VKM_LOCALITY_PROCESS:          break;
    case VKM_LOCALITY_PROCESS_EXPORTED: textureCreateInfo.imageCreateInfo.pNext = &externalImageInfo; break;
  }
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    textureCreateInfo.debugName = "ExportedColorFramebuffer";
    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    vkmCreateTexture(&textureCreateInfo, &pNodeFramebuffers[i].color);

    textureCreateInfo.debugName = "ExportedNormalFramebuffer";
    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    vkmCreateTexture(&textureCreateInfo, &pNodeFramebuffers[i].normal);

    textureCreateInfo.debugName = "ExportedGBufferFramebuffer";
    textureCreateInfo.imageCreateInfo.format = VKM_G_BUFFER_FORMAT;
    textureCreateInfo.imageCreateInfo.usage = VKM_G_BUFFER_USAGE;
    textureCreateInfo.imageCreateInfo.mipLevels = VKM_G_BUFFER_LEVELS;
    vkmCreateTexture(&textureCreateInfo, &pNodeFramebuffers[i].gBuffer);
    textureCreateInfo.imageCreateInfo.mipLevels = 1;
  }
}