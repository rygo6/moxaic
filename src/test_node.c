#include "globals.h"
#include "renderer.h"
#include "window.h"

#include <vulkan/vk_enum_string_helper.h>

#define CACHE_ALIGN __attribute((aligned(64)))

static struct {
  Transform      cameraTransform;
  GlobalSetState globalSetState;

  VkCommandBuffer cmd;
  VkRenderPass    pass;

  int           framebufferIndex;
  VkFramebuffer framebuffers[VK_SWAP_COUNT];

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  GlobalSetState* pGlobalSetMapped;
  VkDescriptorSet globalSet;
  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  uint32_t sphereIndexCount;
  VkBuffer sphereIndexBuffer, sphereVertexBuffer;

  VkDevice device;

  VkImage frameBufferColorImages[VK_SWAP_COUNT];

  Swap     swap;
  VkImage  swapImages[VK_SWAP_COUNT];

  Timeline timeline;
  VkQueue graphicsQueue;

} CACHE_ALIGN node;

void mxcTestNodeUpdate() {
  mxUpdateWindowInput();

  if (mvkProcessInput(&node.cameraTransform)) {
    mvkUpdateGlobalSetView(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  }
  vkmResetBeginCommandBuffer(node.cmd);
  vkmBeginPass(node.cmd, node.pass, node.framebuffers[node.framebufferIndex]);

  vkCmdBindPipeline(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipeline);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, MVK_PIPE_SET_BINDING_STANDARD_GLOBAL, 1, &node.globalSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, MVK_PIPE_SET_BINDING_STANDARD_MATERIAL, 1, &node.checkerMaterialSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, MVK_PIPE_SET_BINDING_STANDARD_OBJECT, 1, &node.sphereObjectSet, 0, NULL);

  vkCmdBindVertexBuffers(node.cmd, 0, 1, (VkBuffer[]){node.sphereVertexBuffer}, (VkDeviceSize[]){0});
  vkCmdBindIndexBuffer(node.cmd, node.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(node.cmd, node.sphereIndexCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(node.cmd);

  vkAcquireNextImageKHR(node.device, node.swap.chain, UINT64_MAX, node.swap.acquireSemaphore, VK_NULL_HANDLE, &node.swap.index);

  vkmBlit(node.cmd, node.frameBufferColorImages[node.framebufferIndex], node.swapImages[node.swap.index]);

  vkEndCommandBuffer(node.cmd);

  vkmSubmitPresentCommandBuffer(node.cmd, node.graphicsQueue, &node.swap, &node.timeline);

  TimelineWait(node.device, &node.timeline);
}

static struct {
  Framebuffer framebuffers[VK_SWAP_COUNT];
  Texture     checkerTexture;
  Mesh        sphereMesh;

  VkDeviceMemory globalSetMemory;
  VkBuffer       globalSetBuffer;

  VkmStandardObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory          sphereObjectSetMemory;
  VkBuffer                sphereObjectSetBuffer;

  Transform              sphereTransform;
  VkmStandardObjectSetState sphereObjectState;

} nodeInit;

void mxcTestNodeInit(const VkmContext* pContext) {
  CreateFramebuffers(VK_SWAP_COUNT, nodeInit.framebuffers);

  CreateSphereMeshBuffers(0.5f, 32, 32, &nodeInit.sphereMesh);

  AllocateDescriptorSet(&pContext->globalSetLayout, &node.globalSet);
  CreateAllocateBindMapBuffer(MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(GlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.globalSetMemory, &nodeInit.globalSetBuffer, (void**)&node.pGlobalSetMapped);

  AllocateDescriptorSet(&pContext->materialSetLayout, &node.checkerMaterialSet);
  CreateTextureFromFile("textures/uvgrid.jpg", &nodeInit.checkerTexture);

  AllocateDescriptorSet(&pContext->objectSetLayout, &node.sphereObjectSet);
  CreateAllocateBindMapBuffer(MEMORY_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.sphereObjectSetMemory, &nodeInit.sphereObjectSetBuffer, (void**)&nodeInit.pSphereObjectSetMapped);

  const VkWriteDescriptorSet writeSets[] = {
      SET_WRITE_GLOBAL(node.globalSet, nodeInit.globalSetBuffer),
      SET_WRITE_STANDARD_MATERIAL(node.checkerMaterialSet, nodeInit.checkerTexture.imageView, pContext->linearSampler),
      SET_WRITE_STANDARD_OBJECT(node.sphereObjectSet, nodeInit.sphereObjectSetBuffer),
  };
  vkUpdateDescriptorSets(pContext->device, COUNT(writeSets), writeSets, 0, NULL);
  UpdateGlobalSet(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  UpdateObjectSet(&nodeInit.sphereTransform, &nodeInit.sphereObjectState, nodeInit.pSphereObjectSetMapped);

  node.cmd = pContext->graphicsCommandBuffer;
  node.pass = pContext->renderPass;

  node.standardPipelineLayout = pContext->standardPipelineLayout;
  node.standardPipeline = pContext->standardPipeline;

  for (int i = 0; i < VK_SWAP_COUNT; ++i) {
    node.framebuffers[i] = nodeInit.framebuffers[i].framebuffer;
    node.frameBufferColorImages[i] = nodeInit.framebuffers[i].colorImage;
    node.swapImages[i] = pContext->swapImages[i];
  }

  node.sphereIndexCount = nodeInit.sphereMesh.indexCount;
  node.sphereIndexBuffer = nodeInit.sphereMesh.indexBuffer;
  node.sphereVertexBuffer = nodeInit.sphereMesh.vertexBuffer;

  node.device = pContext->device;

  node.swap = pContext->swap;

  node.timeline = pContext->timeline;
  node.graphicsQueue = pContext->graphicsQueue;
}