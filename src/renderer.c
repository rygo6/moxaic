#include "renderer.h"
#include "globals.h"
#include "stb_image.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// single instance for whole application
static VkmInstance instance = {};
// but process could have different contexts managed by different threads
_Thread_local VkmContext context = {};

//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

static void ReadFile(const char* pPath, size_t* pFileLength, char** ppFileContents) {
  FILE* file = fopen(pPath, "rb");
  REQUIRE(file != NULL, "File can't be opened!");
  fseek(file, 0, SEEK_END);
  *pFileLength = ftell(file);
  rewind(file);
  *ppFileContents = malloc(*pFileLength * sizeof(char));
  const size_t readCount = fread(*ppFileContents, *pFileLength, 1, file);
  REQUIRE(readCount > 0, "Failed to read file!");
  fclose(file);
}

static VkBool32 DebugUtilsCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  switch (messageSeverity) {
    default:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("%s\n", pCallbackData->pMessage); return VK_FALSE;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   PANIC(pCallbackData->pMessage); return VK_FALSE;
  }
  return 0;
}

static VkCommandBuffer BeginImmediateCommandBuffer(const VkCommandPool commandPool) {
  VkCommandBuffer                   commandBuffer;
  const VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &allocateInfo, &commandBuffer));
  const VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VKM_REQUIRE(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  return commandBuffer;
}
static void EndImmediateCommandBuffer(const VkCommandPool commandPool, const VkQueue graphicsQueue, VkCommandBuffer commandBuffer) {
  VKM_REQUIRE(vkEndCommandBuffer(commandBuffer));
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  VKM_REQUIRE(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  VKM_REQUIRE(vkQueueWaitIdle(graphicsQueue));
  vkFreeCommandBuffers(context.device, commandPool, 1, &commandBuffer);
}

//----------------------------------------------------------------------------------
// Pipelines
//----------------------------------------------------------------------------------

#define COLOR_WRITE_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

static VkShaderModule CreateShaderModule(const char* pShaderPath) {
  size_t codeSize;
  char*  pCode;
  ReadFile(pShaderPath, &codeSize, &pCode);
  const VkShaderModuleCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = codeSize,
      .pCode = (uint32_t*)pCode,
  };
  VkShaderModule shaderModule;
  VKM_REQUIRE(vkCreateShaderModule(context.device, &createInfo, VKM_ALLOC, &shaderModule));
  free(pCode);
  return shaderModule;
}

static const VkFormat VKM_PASS_STD_FORMATS[] = {
    [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_FORMAT_R8G8B8A8_UNORM,
    [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_FORMAT_D32_SFLOAT,
};
static const VkImageUsageFlags VKM_PASS_STD_USAGES[] = {
    [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
};

// Standard Pipeline
#define VKM_PIPE_VERTEX_BINDING_STD_INDEX 0
enum VkmPipeVertexAttributeStandardIndices {
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_POSITION_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_NORMAL_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_UV_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_COUNT,
};
static void CreateStandardPipelineLayout(VkmStandardPipe* pStandardPipeline) {
  VkDescriptorSetLayout pSetLayouts[VKM_PIPE_SET_STD_INDEX_COUNT];
  pSetLayouts[VKM_PIPE_SET_STD_GLOBAL_INDEX] = pStandardPipeline->globalSetLayout;
  pSetLayouts[VKM_PIPE_SET_STD_MATERIAL_INDEX] = pStandardPipeline->materialSetLayout;
  pSetLayouts[VKM_PIPE_SET_STD_OBJECT_INDEX] = pStandardPipeline->objectSetLayout;
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = VKM_PIPE_SET_STD_INDEX_COUNT,
      .pSetLayouts = pSetLayouts,
  };
  VKM_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VKM_ALLOC, &pStandardPipeline->pipelineLayout));
}

static void CreateStandardPipeline(const VkRenderPass renderPass, VkmStandardPipe* pStandardPipeline) {
  const VkShaderModule vertShader = CreateShaderModule("./shaders/basic_material.vert.spv");
  const VkShaderModule fragShader = CreateShaderModule("./shaders/basic_material.frag.spv");

  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &(VkPipelineRobustnessCreateInfoEXT){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
          .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
          .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT,
          //            .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT,
      },
      .stageCount = 2,
      .pStages = (VkPipelineShaderStageCreateInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_VERTEX_BIT,
              .module = vertShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
              .module = fragShader,
              .pName = "main",
          },
      },
      .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          .vertexBindingDescriptionCount = 1,
          .pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
              {
                  .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,
                  .stride = sizeof(VkmVertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
          .vertexAttributeDescriptionCount = 3,
          .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
              {
                  .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_POSITION_INDEX,
                  .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(VkmVertex, position),
              },
              {
                  .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_NORMAL_INDEX,
                  .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(VkmVertex, normal),
              },
              {
                  .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_UV_INDEX,
                  .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(VkmVertex, uv),
              },
          },
      },
      .pInputAssemblyState = &(const VkPipelineInputAssemblyStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE,
      },
      .pViewportState = &(const VkPipelineViewportStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          .viewportCount = 1,
          .scissorCount = 1,
      },
      .pRasterizationState = &(const VkPipelineRasterizationStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .frontFace = VK_FRONT_FACE_CLOCKWISE,
          .lineWidth = 1.0f,
      },
      .pMultisampleState = &(const VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &(const VkPipelineDepthStencilStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
          .maxDepthBounds = 1.0f,
      },
      .pColorBlendState = &(const VkPipelineColorBlendStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOp = VK_LOGIC_OP_COPY,
          .attachmentCount = 2,
          .pAttachments = (const VkPipelineColorBlendAttachmentState[]){
              {/* Color */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
              {/* Normal */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
          },
          .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
      },
      .pDynamicState = &(const VkPipelineDynamicStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = 2,
          .pDynamicStates = (const VkDynamicState[]){
              VK_DYNAMIC_STATE_VIEWPORT,
              VK_DYNAMIC_STATE_SCISSOR,
          },
      },
      .layout = pStandardPipeline->pipelineLayout,
      .renderPass = renderPass,
  };
  VKM_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VKM_ALLOC, &pStandardPipeline->pipeline));
  vkDestroyShaderModule(context.device, fragShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, vertShader, VKM_ALLOC);
}

//----------------------------------------------------------------------------------
// Descriptors
//----------------------------------------------------------------------------------

static void CreateGlobalSetLayout(VkmStandardPipe* pStandardPipeline) {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BINDING_STD_GLOBAL_BUFFER,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                        VK_SHADER_STAGE_COMPUTE_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT |
                        VK_SHADER_STAGE_MESH_BIT_EXT |
                        VK_SHADER_STAGE_TASK_BIT_EXT,
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &pStandardPipeline->globalSetLayout));
}
static void CreateStandardMaterialSetLayout(VkmStandardPipe* pStandardPipeline) {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BINDING_STD_MATERIAL_TEXTURE,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &pStandardPipeline->materialSetLayout));
}
static void CreateStandardObjectSetLayout(VkmStandardPipe* pStandardPipeline) {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BINDING_STD_OBJECT_BUFFER,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &pStandardPipeline->objectSetLayout));
}

void VkmAllocateDescriptorSet(const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet) {
  const VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = pSetLayout,
  };
  VKM_REQUIRE(vkAllocateDescriptorSets(context.device, &allocateInfo, pSet));
}

//----------------------------------------------------------------------------------
// Memory
//----------------------------------------------------------------------------------

static uint32_t FindMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties* pPhysicalDeviceMemoryProperties, const VkMemoryRequirements* pMemoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags) {
  for (uint32_t i = 0; i < pPhysicalDeviceMemoryProperties->memoryTypeCount; i++) {
    bool hasTypeBits = pMemoryRequirements->memoryTypeBits & 1 << i;
    bool hasPropertyFlags = (pPhysicalDeviceMemoryProperties->memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;
    if (hasTypeBits && hasPropertyFlags) {
      return i;
    }
  }
  int index = 0;
  while (memoryPropertyFlags) {
    if (memoryPropertyFlags & 1) {
      printf(" %s ", string_VkMemoryPropertyFlagBits(1U << index));
    }
    ++index;
    memoryPropertyFlags >>= 1;
  }
  PANIC("Failed to find memory with properties!");
}
static void AllocateMemory(const VkMemoryRequirements* pMemoryRequirements, const VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceMemory* pDeviceMemory) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memoryProperties);
  const uint32_t             memoryTypeIndex = FindMemoryTypeIndex(&memoryProperties, pMemoryRequirements, memoryPropertyFlags);
  const VkMemoryAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = pMemoryRequirements->size,
      .memoryTypeIndex = memoryTypeIndex,
  };
  VKM_REQUIRE(vkAllocateMemory(context.device, &allocateInfo, VKM_ALLOC, pDeviceMemory));
}
static void CreateAllocateBindBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = usage,
  };
  VKM_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, NULL, pBuffer));
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(context.device, *pBuffer, &memoryRequirements);
  AllocateMemory(&memoryRequirements, memoryPropertyFlags, pDeviceMemory);
  VKM_REQUIRE(vkBindBufferMemory(context.device, *pBuffer, *pDeviceMemory, 0));
}
void VkmCreateAllocateBindMapBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer, void** ppMapped) {
  CreateAllocateBindBuffer(memoryPropertyFlags, bufferSize, usage, pDeviceMemory, pBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pDeviceMemory, 0, bufferSize, 0, ppMapped));
}
static void CreateStagingBuffer(const void* srcData, const VkDeviceSize bufferSize, VkDeviceMemory* pStagingBufferMemory, VkBuffer* pStagingBuffer) {
  void* dstData;
  CreateAllocateBindBuffer(VKM_MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, pStagingBufferMemory, pStagingBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pStagingBufferMemory, 0, bufferSize, 0, &dstData));
  memcpy(dstData, srcData, bufferSize);
  vkUnmapMemory(context.device, *pStagingBufferMemory);
}
static void PopulateBufferViaStaging(const VkmCommandContext* pCommand, const void* srcData, const VkDeviceSize bufferSize, const VkBuffer buffer) {
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(srcData, bufferSize, &stagingBufferMemory, &stagingBuffer);
  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(pCommand->pool);
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.size = bufferSize});
  EndImmediateCommandBuffer(pCommand->pool, pCommand->queue, commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VKM_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VKM_ALLOC);
}

//----------------------------------------------------------------------------------
// Images
//----------------------------------------------------------------------------------

#define DEFAULT_IMAGE_CREATE_INFO                 \
  (VkImageCreateInfo) {                           \
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, \
    .imageType = VK_IMAGE_TYPE_2D,                \
    .format = VK_FORMAT_B8G8R8A8_SRGB,            \
    .mipLevels = 1,                               \
    .arrayLayers = 1,                             \
    .samples = VK_SAMPLE_COUNT_1_BIT,             \
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT           \
  }
static void CreateImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImage image, const VkImageAspectFlags aspectMask, VkImageView* pImageView) {
  const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = (VkImageViewType)pImageCreateInfo->imageType,
      .format = pImageCreateInfo->format,
      .subresourceRange = {
          .aspectMask = aspectMask,
          .levelCount = pImageCreateInfo->mipLevels,
          .layerCount = pImageCreateInfo->arrayLayers,
      },
  };
  VKM_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VKM_ALLOC, pImageView));
}
static void CreateAllocateBindImage(const VkImageCreateInfo* pImageCreateInfo, VkDeviceMemory* pMemory, VkImage* pImage) {
  VKM_REQUIRE(vkCreateImage(context.device, pImageCreateInfo, VKM_ALLOC, pImage));
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, *pImage, &memRequirements);
  AllocateMemory(&memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pMemory);
  VKM_REQUIRE(vkBindImageMemory(context.device, *pImage, *pMemory, 0));
}
static void CreateAllocateBindImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImageAspectFlags aspectMask, VkDeviceMemory* pMemory, VkImage* pImage, VkImageView* pImageView) {
  CreateAllocateBindImage(pImageCreateInfo, pMemory, pImage);
  CreateImageView(pImageCreateInfo, *pImage, aspectMask, pImageView);
}

void VkmCreateTextureFromFile(const VkmCommandContext* pCommand, const char* pPath, VkmTexture* pTexture) {
  int      texChannels, width, height;
  stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
  REQUIRE(width > 0 && height > 0, "Image height or width is equal to zero.")
  const VkDeviceSize imageBufferSize = width * height * 4;
  pTexture->extent = (VkExtent3D){width, height, 1};

  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(pImagePixels, imageBufferSize, &stagingBufferMemory, &stagingBuffer);
  stbi_image_free(pImagePixels);

  VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
  imageCreateInfo.extent = pTexture->extent;
  imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pTexture->imageMemory, &pTexture->image, &pTexture->imageView);

  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(pCommand->pool);
  vkmCommandPipelineImageBarrier(commandBuffer, 1, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image));
  const VkBufferImageCopy region = {
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkmCommandPipelineImageBarrier(commandBuffer, 1, &VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_SHADER_READ_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image));
  EndImmediateCommandBuffer(pCommand->pool, pCommand->queue, commandBuffer);
}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------

typedef enum VkmSupport {
  VKM_SUPPORT_OPTIONAL,
  VKM_SUPPORT_YES,
  VKM_SUPPORT_NO,
  VKM_SUPPORT_COUNT,
} VkmSupport;
static const char* string_Support[] = {
    [VKM_SUPPORT_OPTIONAL] = "SUPPORT_OPTIONAL",
    [VKM_SUPPORT_YES] = "SUPPORT_YES",
    [VKM_SUPPORT_NO] = "SUPPORT_NO",
};
typedef struct VkmQueueInfo {
  VkmSupport   graphics;
  VkmSupport   compute;
  VkmSupport   transfer;
  VkmSupport   globalPriority;
  VkSurfaceKHR presentSurface;
} VkmQueueInfo;
#define QUEUE_DESC_GRAPHICS(surface, global_priority) (VkmQueueInfo) { .graphics = VKM_SUPPORT_YES, .compute = VKM_SUPPORT_YES, .transfer = VKM_SUPPORT_YES, .globalPriority = global_priority, .presentSurface = surface }
#define QUEUE_DESC_COMPUTE(surface, global_priority) (VkmQueueInfo) { .graphics = VKM_SUPPORT_NO, .compute = VKM_SUPPORT_YES, .transfer = VKM_SUPPORT_YES, .globalPriority = global_priority, .presentSurface = surface }
#define QUEUE_DESC_TRANSFER (VkmQueueInfo) { .graphics = VKM_SUPPORT_NO, .compute = VKM_SUPPORT_NO, .transfer = VKM_SUPPORT_YES }
static uint32_t FindQueueIndex(const VkPhysicalDevice physicalDevice, const VkmQueueInfo* pQueueDesc) {
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties2                 queueFamilyProperties[queueFamilyCount] = {};
  VkQueueFamilyGlobalPriorityPropertiesEXT queueFamilyGlobalPriorityProperties[queueFamilyCount] = {};
  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    queueFamilyGlobalPriorityProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT;
    queueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    queueFamilyProperties[i].pNext = &queueFamilyGlobalPriorityProperties[i];
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties);

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    bool graphicsSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    bool computeSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    bool transferSupport = queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
    bool globalPrioritySupport = queueFamilyGlobalPriorityProperties[i].priorityCount > 0;

    VkBool32 presentSupport = VK_FALSE;
    if (pQueueDesc->presentSurface != NULL) vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, pQueueDesc->presentSurface, &presentSupport);

    if (pQueueDesc->graphics == VKM_SUPPORT_YES && !graphicsSupport) continue;
    if (pQueueDesc->graphics == VKM_SUPPORT_NO && graphicsSupport) continue;
    if (pQueueDesc->compute == VKM_SUPPORT_YES && !computeSupport) continue;
    if (pQueueDesc->compute == VKM_SUPPORT_NO && computeSupport) continue;
    if (pQueueDesc->transfer == VKM_SUPPORT_YES && !transferSupport) continue;
    if (pQueueDesc->transfer == VKM_SUPPORT_NO && transferSupport) continue;
    if (pQueueDesc->globalPriority == VKM_SUPPORT_YES && !globalPrioritySupport) continue;
    if (pQueueDesc->globalPriority == VKM_SUPPORT_NO && globalPrioritySupport) continue;
    if (pQueueDesc->presentSurface != NULL && !presentSupport) continue;

    return i;
  }

  fprintf(stderr, "graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
          string_Support[pQueueDesc->graphics],
          string_Support[pQueueDesc->compute],
          string_Support[pQueueDesc->transfer],
          string_Support[pQueueDesc->globalPriority],
          pQueueDesc->presentSurface == NULL ? "No" : "Yes");
  PANIC("Can't find queue family");
  return -1;
}
static void CreateCommandContext(const VkDevice device, VkmCommandContext* pCommand) {
  vkGetDeviceQueue(device, pCommand->queueFamilyIndex, 0, &pCommand->queue);
  REQUIRE((pCommand->queue != NULL), "graphicsQueue not found!");
  const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = pCommand->queueFamilyIndex,
  };
  VKM_REQUIRE(vkCreateCommandPool(device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &pCommand->pool));
  const VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pCommand->pool,
      .commandBufferCount = 1,
  };
  VKM_REQUIRE(vkAllocateCommandBuffers(device, &graphicsCommandBufferAllocateInfo, &pCommand->buffer));
}

void vkmCreateInstance(const bool createSurface) {
  {
    const char* ppEnabledLayerNames[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const char* ppEnabledInstanceExtensionNames[] = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        WindowExtensionName,
    };
    const VkInstanceCreateInfo instanceCreationInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &(const VkApplicationInfo){
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Moxaic",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Vulkan",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VKM_VERSION,
        },
        .enabledLayerCount = COUNT(ppEnabledLayerNames),
        .ppEnabledLayerNames = ppEnabledLayerNames,
        .enabledExtensionCount = COUNT(ppEnabledInstanceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
    };
    VKM_REQUIRE(vkCreateInstance(&instanceCreationInfo, VKM_ALLOC, &instance.instance));
    printf("Instance Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }
  {
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = {};
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT messageType = {};
    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = DebugUtilsCallback,
    };
    VKM_PFN_LOAD(instance.instance, vkCreateDebugUtilsMessengerEXT);
    VKM_REQUIRE(vkCreateDebugUtilsMessengerEXT(instance.instance, &debugUtilsMessengerCreateInfo, VKM_ALLOC, &instance.debugUtilsMessenger));
  }
  if (createSurface) {
    VKM_REQUIRE(vkmCreateSurface(instance.instance, VKM_ALLOC, &instance.surface));
  }
}

void vkmCreateContext(const VkmContextCreateInfo* pCreateInfo) {
  REQUIRE(instance.instance != NULL, "Trying to create Context when Instance has not been created!");
  {  // PhysicalDevice
    uint32_t deviceCount = 0;
    VKM_REQUIRE(vkEnumeratePhysicalDevices(instance.instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    VKM_REQUIRE(vkEnumeratePhysicalDevices(instance.instance, &deviceCount, devices));
    context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    REQUIRE(physicalDeviceProperties.properties.apiVersion >= VKM_VERSION, "Insufficinet Vulkan API Version");
  }
  const int commandCount = pCreateInfo->createGraphicsCommand + pCreateInfo->createComputeCommand + pCreateInfo->createTransferCommand;
  if (pCreateInfo->createGraphicsCommand)
    context.graphicsCommand.queueFamilyIndex = FindQueueIndex(context.physicalDevice, &QUEUE_DESC_GRAPHICS(instance.surface, VKM_SUPPORT_OPTIONAL));
  if (pCreateInfo->createComputeCommand)
    context.computeCommand.queueFamilyIndex = FindQueueIndex(context.physicalDevice, &QUEUE_DESC_COMPUTE(instance.surface, VKM_SUPPORT_OPTIONAL));
  if (pCreateInfo->createTransferCommand)
    context.transferCommand.queueFamilyIndex = FindQueueIndex(context.physicalDevice, &QUEUE_DESC_TRANSFER);
  {  // Device
    const VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
        .pNext = NULL,
        .globalPriorityQuery = VK_TRUE,
    };
    const VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = (void*)&physicalDeviceGlobalPriorityQueryFeatures,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
        .nullDescriptor = VK_TRUE,
    };
    const VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = (void*)&physicalDeviceRobustness2Features,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = (void*)&physicalDeviceMeshShaderFeatures,
        .synchronization2 = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = (void*)&physicalDeviceVulkan13Features,
        .samplerFilterMinmax = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
    };
    const VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = (void*)&physicalDeviceVulkan12Features,
    };
    const VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = (void*)&physicalDeviceVulkan11Features,
        .features = {
            .robustBufferAccess = VK_TRUE,
        },
    };
    const char* ppEnabledDeviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME,
        VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        ExternalMemoryExtensionName,
        ExternalSemaphoreExtensionName,
        ExternalFenceExtensionName,
    };
    const VkDeviceQueueGlobalPriorityCreateInfoEXT deviceQueueGlobalPriorityCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
        .globalPriority = pCreateInfo->highPriority ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT,
    };
    VkDeviceQueueCreateInfo deviceQueueCreateInfos[commandCount];
    if (pCreateInfo->createGraphicsCommand) {
      deviceQueueCreateInfos[0] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = &deviceQueueGlobalPriorityCreateInfo,
          .queueFamilyIndex = context.graphicsCommand.queueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &(const float){pCreateInfo->highPriority ? 1.0f : 0.0f},
      };
    }
    if (pCreateInfo->createComputeCommand) {
      deviceQueueCreateInfos[1] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = &deviceQueueGlobalPriorityCreateInfo,
          .queueFamilyIndex = context.computeCommand.queueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &(const float){pCreateInfo->highPriority ? 1.0f : 0.0f},
      };
    }
    if (pCreateInfo->createTransferCommand) {
      deviceQueueCreateInfos[2] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = &deviceQueueGlobalPriorityCreateInfo,
          .queueFamilyIndex = context.transferCommand.queueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &(const float){0},
      };
    }
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = commandCount,
        .pQueueCreateInfos = deviceQueueCreateInfos,
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    VKM_REQUIRE(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VKM_ALLOC, &context.device));
  }
  if (pCreateInfo->createGraphicsCommand)
    CreateCommandContext(context.device, &context.graphicsCommand);
  if (pCreateInfo->createComputeCommand)
    CreateCommandContext(context.device, &context.computeCommand);
  if (pCreateInfo->createTransferCommand)
    CreateCommandContext(context.device, &context.transferCommand);
  {  // Semaphore
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineSemaphoreTypeCreateInfo,
    };
    VKM_REQUIRE(vkCreateSemaphore(context.device, &timelineSemaphoreCreateInfo, VKM_ALLOC, &context.timeline.semaphore));
    context.timeline.waitValue = 0;
  }
  {  // Pools
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 30,
        .poolSizeCount = 3,
        .pPoolSizes = (const VkDescriptorPoolSize[]){
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
        },
    };
    VKM_REQUIRE(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, VKM_ALLOC, &context.descriptorPool));
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    VKM_REQUIRE(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VKM_ALLOC, &context.timeQueryPool));
  }
}

void vkmForkContext(const VkmContextCreateInfo* pCreateInfo) {


}

void VkmCreateStandardRenderPass(VkRenderPass* pRenderPass) {
  REQUIRE(context.device != NULL, "Trying to create RenderPass when Context has not been created!");
#define DEFAULT_ATTACHMENT_DESCRIPTION                \
  .samples = VK_SAMPLE_COUNT_1_BIT,                   \
  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              \
  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,            \
  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   \
  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, \
  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         \
  .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
  const VkRenderPassCreateInfo renderPassCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 3,
      .pAttachments = (const VkAttachmentDescription[]){
          {.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX], DEFAULT_ATTACHMENT_DESCRIPTION},
          {.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX], DEFAULT_ATTACHMENT_DESCRIPTION},
          {.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX], DEFAULT_ATTACHMENT_DESCRIPTION},
      },
      .subpassCount = 1,
      .pSubpasses = &(const VkSubpassDescription){
          .colorAttachmentCount = 2,
          .pColorAttachments = (const VkAttachmentReference[]){
              {
                  .attachment = VKM_PASS_ATTACHMENT_STD_COLOR_INDEX,
                  .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
              },
              {
                  .attachment = VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX,
                  .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
              },
          },
          .pDepthStencilAttachment = &(const VkAttachmentReference){
              .attachment = VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX,
              .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
          },
      },
  };
  VKM_REQUIRE(vkCreateRenderPass(context.device, &renderPassCreateInfo, VKM_ALLOC, pRenderPass));
#undef DEFAULT_ATTACHMENT_DESCRIPTION
}

void VkmContextCreateSampler(const VkmSamplerDesc* pSamplerDesc, VkSampler* pSampler) {
  REQUIRE(context.device != NULL, "Trying to create Sampler when Context has not been created!");
  const VkSamplerCreateInfo minSamplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = &(const VkSamplerReductionModeCreateInfo){
          .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
          .reductionMode = pSamplerDesc->reductionMode,
      },
      .magFilter = pSamplerDesc->filter,
      .minFilter = pSamplerDesc->filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .maxLod = 16.0,
      .addressModeU = pSamplerDesc->addressMode,
      .addressModeV = pSamplerDesc->addressMode,
      .addressModeW = pSamplerDesc->addressMode,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK};
  VKM_REQUIRE(vkCreateSampler(context.device, &minSamplerCreateInfo, VKM_ALLOC, pSampler));
}

void VkmContextCreateSwap(VkmSwap* pSwap) {
  REQUIRE(context.device != NULL, "Trying to create Swap when Context has not been created!");
  REQUIRE(instance.surface != NULL, "Trying to create Swap when Instance was not made with a Surface!");
  const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = instance.surface,
      .minImageCount = VKM_SWAP_COUNT,
      .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
      .imageExtent = {DEFAULT_WIDTH, DEFAULT_HEIGHT},
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
  };
  VKM_REQUIRE(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, VKM_ALLOC, &pSwap->chain));
  uint32_t swapCount;
  VKM_REQUIRE(vkGetSwapchainImagesKHR(context.device, pSwap->chain, &swapCount, NULL));
  REQUIRE(swapCount == VKM_SWAP_COUNT, "Resulting swap image count does not match requested swap count!");
  VKM_REQUIRE(vkGetSwapchainImagesKHR(context.device, pSwap->chain, &swapCount, pSwap->images));
  const VkSemaphoreCreateInfo acquireSwapSemaphoreCreateInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VKM_REQUIRE(vkCreateSemaphore(context.device, &acquireSwapSemaphoreCreateInfo, VKM_ALLOC, &pSwap->acquireSemaphore));
  VKM_REQUIRE(vkCreateSemaphore(context.device, &acquireSwapSemaphoreCreateInfo, VKM_ALLOC, &pSwap->renderCompleteSemaphore));
}

void VkmCreateStandardPipeline(const VkRenderPass renderPass, VkmStandardPipe* pStandardPipeline) {
  REQUIRE(context.device != NULL, "Trying to create StandardPipeline when Context has not been created!");
  CreateGlobalSetLayout(pStandardPipeline);
  CreateStandardMaterialSetLayout(pStandardPipeline);
  CreateStandardObjectSetLayout(pStandardPipeline);
  CreateStandardPipelineLayout(pStandardPipeline);
  CreateStandardPipeline(renderPass, pStandardPipeline);
}

static int  GenerateSphereVertexCount(const int slicesCount, const int stacksCount) { return (slicesCount + 1) * (stacksCount + 1); }
static int  GenerateSphereIndexCount(const int slicesCount, const int stacksCount) { return slicesCount * stacksCount * 2 * 3; }
static void GenerateSphere(const int slicesCount, const int stacksCount, const float radius, VkmVertex* pVertex) {
  const float slices = (float)slicesCount;
  const float stacks = (float)stacksCount;
  const float dtheta = 2.0f * VKM_PI / slices;
  const float dphi = VKM_PI / stacks;
  int         idx = 0;
  for (int i = 0; +i <= stacksCount; i++) {
    const float fi = (float)i;
    const float phi = fi * dphi;
    for (int j = 0; j <= slicesCount; j++) {
      const float ji = (float)j;
      const float theta = ji * dtheta;
      const float x = radius * sinf(phi) * cosf(theta);
      const float y = radius * sinf(phi) * sinf(theta);
      const float z = radius * cosf(phi);
      pVertex[idx++] = (VkmVertex){
          .position = {x, y, z},
          .normal = {x, y, z},
          .uv = {ji / slices, fi / stacks},
      };
    }
  }
}
static void GenerateSphereIndices(const int nslices, const int nstacks, uint16_t* pIndices) {
  int idx = 0;
  for (int i = 0; i < nstacks; i++) {
    for (int j = 0; j < nslices; j++) {
      const uint16_t v1 = i * (nslices + 1) + j;
      const uint16_t v2 = i * (nslices + 1) + j + 1;
      const uint16_t v3 = (i + 1) * (nslices + 1) + j;
      const uint16_t v4 = (i + 1) * (nslices + 1) + j + 1;
      pIndices[idx++] = v1;
      pIndices[idx++] = v2;
      pIndices[idx++] = v3;
      pIndices[idx++] = v2;
      pIndices[idx++] = v4;
      pIndices[idx++] = v3;
    }
  }
}
void VkmCreateSphereMesh(const VkmCommandContext* pCommand, const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh) {
  REQUIRE(context.device != NULL, "Trying to create SphereMesh when Context has not been created!");
  {
    pMesh->indexCount = GenerateSphereIndexCount(slicesCount, stackCount);
    uint16_t pIndices[pMesh->indexCount];
    GenerateSphereIndices(slicesCount, stackCount, pIndices);
    uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &pMesh->indexMemory, &pMesh->indexBuffer);
    PopulateBufferViaStaging(pCommand, pIndices, indexBufferSize, pMesh->indexBuffer);
  }
  {
    pMesh->vertexCount = GenerateSphereVertexCount(slicesCount, stackCount);
    VkmVertex pVertices[pMesh->vertexCount];
    GenerateSphere(slicesCount, stackCount, radius, pVertices);
    uint32_t vertexBufferSize = sizeof(VkmVertex) * pMesh->vertexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &pMesh->vertexMemory, &pMesh->vertexBuffer);
    PopulateBufferViaStaging(pCommand, pVertices, vertexBufferSize, pMesh->vertexBuffer);
  }
}

void VkmCreateFramebuffers(const VkRenderPass renderPass, const uint32_t framebufferCount, VkmFramebuffer* pFrameBuffers) {
  REQUIRE(context.device != NULL, "Trying to create Framebuffers when Context has not been created!");
  for (int i = 0; i < framebufferCount; ++i) {
    VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
    imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].colorImageMemory, &pFrameBuffers[i].colorImage, &pFrameBuffers[i].colorImageView);

    imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].normalImageMemory, &pFrameBuffers[i].normalImage, &pFrameBuffers[i].normalImageView);

    imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_DEPTH_BIT, &pFrameBuffers[i].depthImageMemory, &pFrameBuffers[i].depthImage, &pFrameBuffers[i].depthImageView);

    imageCreateInfo.format = VKM_STD_G_BUFFER_FORMAT;
    imageCreateInfo.usage = VKM_STD_G_BUFFER_USAGE;
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].gBufferImageMemory, &pFrameBuffers[i].gBufferImage, &pFrameBuffers[i].gBufferImageView);

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = VKM_PASS_ATTACHMENT_STD_COUNT,
        .pAttachments = (const VkImageView[]){
            [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = pFrameBuffers[i].colorImageView,
            [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = pFrameBuffers[i].normalImageView,
            [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = pFrameBuffers[i].depthImageView,
        },
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .layers = 1,
    };
    VKM_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VKM_ALLOC, &pFrameBuffers[i].framebuffer));
    VKM_REQUIRE(vkCreateSemaphore(context.device, &(VkSemaphoreCreateInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}, VKM_ALLOC, &pFrameBuffers[i].renderCompleteSemaphore));
  }
}