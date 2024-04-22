#include "renderer.h"
#include "globals.h"
#include "stb_image.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------

typedef struct Vertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
} Vertex;
enum VertexAttributes {
  VERTEX_ATTRIBUTE_POSITION,
  VERTEX_ATTRIBUTE_NORMAL,
  VERTEX_ATTRIBUTE_UV,
  VERTEX_ATTRIBUTE_COUNT,
};

static VkmContext context;

//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

typedef enum Support {
  SUPPORT_OPTIONAL,
  SUPPORT_YES,
  SUPPORT_NO,
  SUPPORT_COUNT,
} Support;
static const char* string_Support[] = {
    [SUPPORT_OPTIONAL] = "SUPPORT_OPTIONAL",
    [SUPPORT_YES] = "SUPPORT_YES",
    [SUPPORT_NO] = "SUPPORT_NO",
};

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
  VK_REQUIRE(vkAllocateCommandBuffers(context.device, &allocateInfo, &commandBuffer));
  const VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_REQUIRE(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  return commandBuffer;
}
static void EndImmediateCommandBuffer(const VkCommandPool commandPool, const VkQueue graphicsQueue, VkCommandBuffer commandBuffer) {
  VK_REQUIRE(vkEndCommandBuffer(commandBuffer));
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  VK_REQUIRE(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  VK_REQUIRE(vkQueueWaitIdle(graphicsQueue));
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
  VK_REQUIRE(vkCreateShaderModule(context.device, &createInfo, VK_ALLOC, &shaderModule));
  free(pCode);
  return shaderModule;
}

// Standard Pipeline
static void CreateStandardPipelineLayout() {
  VkDescriptorSetLayout pSetLayouts[PIPE_SET_BINDING_STANDARD_COUNT];
  pSetLayouts[MVK_PIPE_SET_BINDING_STANDARD_GLOBAL] = context.globalSetLayout;
  pSetLayouts[MVK_PIPE_SET_BINDING_STANDARD_MATERIAL] = context.materialSetLayout;
  pSetLayouts[MVK_PIPE_SET_BINDING_STANDARD_OBJECT] = context.objectSetLayout;
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = PIPE_SET_BINDING_STANDARD_COUNT,
      .pSetLayouts = pSetLayouts,
  };
  VK_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VK_ALLOC, &context.standardPipelineLayout));
}

static const VkFormat          G_BUFFER_FORMAT = VK_FORMAT_R32_SFLOAT;
static const VkImageUsageFlags G_BUFFER_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
static const VkFormat          RENDER_PASS_FORMATS[] = {
    [PASS_COLOR_ATTACHMENT] = VK_FORMAT_R8G8B8A8_UNORM,
    [PASS_NORMAL_ATTACHMENT] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [PASS_DEPTH_ATTACHMENT] = VK_FORMAT_D32_SFLOAT,
};
static const VkImageUsageFlags RENDER_PASS_USAGES[] = {
    [PASS_COLOR_ATTACHMENT] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    [PASS_NORMAL_ATTACHMENT] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    [PASS_DEPTH_ATTACHMENT] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
};
void CreateStandardPipeline() {
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
                  .binding = PIPE_VERTEX_BINDING_STANDARD,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
          .vertexAttributeDescriptionCount = 3,
          .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
              {
                  .location = VERTEX_ATTRIBUTE_POSITION,
                  .binding = PIPE_VERTEX_BINDING_STANDARD,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, position),
              },
              {
                  .location = VERTEX_ATTRIBUTE_NORMAL,
                  .binding = PIPE_VERTEX_BINDING_STANDARD,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, normal),
              },
              {
                  .location = VERTEX_ATTRIBUTE_UV,
                  .binding = PIPE_VERTEX_BINDING_STANDARD,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(Vertex, uv),
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
      .layout = context.standardPipelineLayout,
      .renderPass = context.renderPass,
  };
  VK_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, &context.standardPipeline));
  vkDestroyShaderModule(context.device, fragShader, VK_ALLOC);
  vkDestroyShaderModule(context.device, vertShader, VK_ALLOC);
}

//----------------------------------------------------------------------------------
// Descriptors
//----------------------------------------------------------------------------------

static void CreateGlobalSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = SET_BINDING_GLOBAL_BUFFER,
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
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.globalSetLayout));
}
static void CreateStandardMaterialSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = SET_BINDING_STANDARD_MATERIAL_TEXTURE,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.materialSetLayout));
}
static void CreateStandardObjectSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = SET_BINDING_STANDARD_OBJECT_BUFFER,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.objectSetLayout));
}

VkDescriptorSet AllocateDescriptorSet(const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet) {
  const VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = context.descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = pSetLayout,
  };
  VK_REQUIRE(vkAllocateDescriptorSets(context.device, &allocateInfo, pSet));
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
  VK_REQUIRE(vkAllocateMemory(context.device, &allocateInfo, VK_ALLOC, pDeviceMemory));
}
static void CreateAllocateBindBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = usage,
  };
  VK_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, NULL, pBuffer));
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(context.device, *pBuffer, &memoryRequirements);
  AllocateMemory(&memoryRequirements, memoryPropertyFlags, pDeviceMemory);
  VK_REQUIRE(vkBindBufferMemory(context.device, *pBuffer, *pDeviceMemory, 0));
}
void CreateAllocateBindMapBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer, void** ppMapped) {
  CreateAllocateBindBuffer(memoryPropertyFlags, bufferSize, usage, pDeviceMemory, pBuffer);
  VK_REQUIRE(vkMapMemory(context.device, *pDeviceMemory, 0, bufferSize, 0, ppMapped));
}
static void CreateStagingBuffer(const void* srcData, const VkDeviceSize bufferSize, VkDeviceMemory* pStagingBufferMemory, VkBuffer* pStagingBuffer) {
  void* dstData;
  CreateAllocateBindBuffer(MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, pStagingBufferMemory, pStagingBuffer);
  VK_REQUIRE(vkMapMemory(context.device, *pStagingBufferMemory, 0, bufferSize, 0, &dstData));
  memcpy(dstData, srcData, bufferSize);
  vkUnmapMemory(context.device, *pStagingBufferMemory);
}
static void PopulateBufferViaStaging(const void* srcData, const VkDeviceSize bufferSize, const VkBuffer buffer) {
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(srcData, bufferSize, &stagingBufferMemory, &stagingBuffer);
  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.size = bufferSize});
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VK_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VK_ALLOC);
}

//----------------------------------------------------------------------------------
// Images
//----------------------------------------------------------------------------------

static void TransitionImageLayoutImmediate(const ImageBarrier* pSrc, const ImageBarrier* pDst, const VkImageAspectFlags aspectMask, const VkImage image) {
  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  vkmCommandImageBarrier(commandBuffer, pSrc, pDst, aspectMask, image);
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
}

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
  VK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VK_ALLOC, pImageView));
}
static void CreateAllocateBindImage(const VkImageCreateInfo* pImageCreateInfo, VkDeviceMemory* pMemory, VkImage* pImage) {
  VK_REQUIRE(vkCreateImage(context.device, pImageCreateInfo, VK_ALLOC, pImage));
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, *pImage, &memRequirements);
  AllocateMemory(&memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pMemory);
  VK_REQUIRE(vkBindImageMemory(context.device, *pImage, *pMemory, 0));
}
static void CreateAllocateBindImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImageAspectFlags aspectMask, VkDeviceMemory* pMemory, VkImage* pImage, VkImageView* pImageView) {
  CreateAllocateBindImage(pImageCreateInfo, pMemory, pImage);
  CreateImageView(pImageCreateInfo, *pImage, aspectMask, pImageView);
}

void CreateTextureFromFile(const char* pPath, Texture* pTexture) {
  int      texChannels, width, height;
  stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
  REQUIRE(width > 0 && height > 0, "Image height or width is equal to zero.")
  const VkDeviceSize imageBufferSize = width * height * 4;
  pTexture->extent = (VkExtent3D){width, height, 1};

  VkBuffer           stagingBuffer;
  VkDeviceMemory     stagingBufferMemory;
  CreateStagingBuffer(pImagePixels, imageBufferSize, &stagingBufferMemory, &stagingBuffer);
  stbi_image_free(pImagePixels);

  VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
  imageCreateInfo.extent = pTexture->extent;
  imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pTexture->imageMemory, &pTexture->image, &pTexture->imageView);

  const VkCommandBuffer commandBuffer = BeginImmediateCommandBuffer(context.graphicsCommandPool);
  vkmCommandImageBarrier(commandBuffer, &UNDEFINED_IMAGE_BARRIER, &TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image);
  const VkBufferImageCopy region = {
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkmCommandImageBarrier(commandBuffer, &TRANSFER_DST_IMAGE_BARRIER, &SHADER_READ_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image);
  EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer);
}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------

typedef struct QueueDesc {
  Support      graphics;
  Support      compute;
  Support      transfer;
  Support      globalPriority;
  VkSurfaceKHR presentSurface;
} QueueDesc;
static uint32_t FindQueueIndex(const VkPhysicalDevice physicalDevice, const QueueDesc* pQueueDesc) {
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

    if (pQueueDesc->graphics == SUPPORT_YES && !graphicsSupport) continue;
    if (pQueueDesc->graphics == SUPPORT_NO && graphicsSupport) continue;
    if (pQueueDesc->compute == SUPPORT_YES && !computeSupport) continue;
    if (pQueueDesc->compute == SUPPORT_NO && computeSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_YES && !transferSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_NO && transferSupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_YES && !globalPrioritySupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_NO && globalPrioritySupport) continue;
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

const VkmContext* mxcInitRendererContext() {
  {  // Instance
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
            .apiVersion = VK_VERSION,
        },
        .enabledLayerCount = COUNT(ppEnabledLayerNames),
        .ppEnabledLayerNames = ppEnabledLayerNames,
        .enabledExtensionCount = COUNT(ppEnabledInstanceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
    };
    VK_REQUIRE(vkCreateInstance(&instanceCreationInfo, VK_ALLOC, &context.instance));
    printf("Instance Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }
  {
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = 0;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT messageType = 0;
    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = DebugUtilsCallback,
    };
    VK_PFN_LOAD(vkCreateDebugUtilsMessengerEXT);
    VK_REQUIRE(vkCreateDebugUtilsMessengerEXT(context.instance, &debugUtilsMessengerCreateInfo, VK_ALLOC, &context.debugUtilsMessenger));
  }
  {  // PhysicalDevice
    uint32_t deviceCount = 0;
    VK_REQUIRE(vkEnumeratePhysicalDevices(context.instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    VK_REQUIRE(vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices));
    context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    REQUIRE(physicalDeviceProperties.properties.apiVersion >= VK_VERSION, "Insufficinet Vulkan API Version");
  }
  {  // Surface
    VK_REQUIRE(mxcCreateSurface(context.instance, VK_ALLOC, &context.surface));
  }
  {  // QueueIndices
    const QueueDesc graphicsQueueDesc = {.graphics = SUPPORT_YES, .compute = SUPPORT_YES, .transfer = SUPPORT_YES, .globalPriority = SUPPORT_YES, .presentSurface = context.surface};
    context.graphicsQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &graphicsQueueDesc);
    const QueueDesc computeQueueDesc = {.graphics = SUPPORT_NO, .compute = SUPPORT_YES, .transfer = SUPPORT_YES, .globalPriority = SUPPORT_YES, .presentSurface = context.surface};
    context.computeQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &computeQueueDesc);
    const QueueDesc transferQueueDesc = {.graphics = SUPPORT_NO, .compute = SUPPORT_NO, .transfer = SUPPORT_YES};
    context.transferQueueFamilyIndex = FindQueueIndex(context.physicalDevice, &transferQueueDesc);
  }
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
        ExternalMemoryExntensionName,
        ExternalSemaphoreExntensionName,
        ExternalFenceExntensionName,
    };
    const VkDeviceQueueGlobalPriorityCreateInfoEXT deviceQueueGlobalPriorityCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
        .globalPriority = isCompositor ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT,
    };
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = 3,
        .pQueueCreateInfos = (const VkDeviceQueueCreateInfo[]){
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.graphicsQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){isCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.computeQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){isCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = context.transferQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(const float){0},
            },
        },
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    VK_REQUIRE(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VK_ALLOC, &context.device));
  }
  {  // Queues
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    REQUIRE((context.graphicsQueue != NULL), "graphicsQueue not found!");
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    REQUIRE((context.computeQueue != NULL), "computeQueue not found!");
    vkGetDeviceQueue(context.device, context.transferQueueFamilyIndex, 0, &context.transferQueue);
    REQUIRE((context.transferQueue != NULL), "transferQueue not found!");
  }
  // PFN_LOAD(vkSetDebugUtilsObjectNameEXT);
  {  // RenderPass
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
            {.format = RENDER_PASS_FORMATS[PASS_COLOR_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = RENDER_PASS_FORMATS[PASS_NORMAL_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = RENDER_PASS_FORMATS[PASS_DEPTH_ATTACHMENT], DEFAULT_ATTACHMENT_DESCRIPTION},
        },
        .subpassCount = 1,
        .pSubpasses = &(const VkSubpassDescription){
            .colorAttachmentCount = 2,
            .pColorAttachments = (const VkAttachmentReference[]){
                {
                    .attachment = PASS_COLOR_ATTACHMENT,
                    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                },
                {
                    .attachment = PASS_NORMAL_ATTACHMENT,
                    .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                },
            },
            .pDepthStencilAttachment = &(const VkAttachmentReference){
                .attachment = PASS_DEPTH_ATTACHMENT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            },
        },
    };
    VK_REQUIRE(vkCreateRenderPass(context.device, &renderPassCreateInfo, VK_ALLOC, &context.renderPass));
#undef DEFAULT_ATTACHMENT_DESCRIPTION
  }
  {  // Pools
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex,
    };
    VK_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &context.graphicsCommandPool));
    const VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.computeQueueFamilyIndex,
    };
    VK_REQUIRE(vkCreateCommandPool(context.device, &computeCommandPoolCreateInfo, VK_ALLOC, &context.computeCommandPool));
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
    VK_REQUIRE(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, VK_ALLOC, &context.descriptorPool));
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    VK_REQUIRE(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VK_ALLOC, &context.timeQueryPool));
  }
  {  // CommandBuffers
    const VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.graphicsCommandPool,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(context.device, &graphicsCommandBufferAllocateInfo, &context.graphicsCommandBuffer));
    const VkCommandBufferAllocateInfo computeCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.computeCommandPool,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(context.device, &computeCommandBufferAllocateInfo, &context.computeCommandBuffer));
  }
  {
#define DEFAULT_LINEAR_SAMPLER                           \
  .magFilter = VK_FILTER_LINEAR,                         \
  .minFilter = VK_FILTER_LINEAR,                         \
  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,           \
  .maxLod = 16.0,                                        \
  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, \
  .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK

    const VkSamplerCreateInfo nearestSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .maxLod = 16.0,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &nearestSampleCreateInfo, VK_ALLOC, &context.nearestSampler));
    const VkSamplerCreateInfo linearSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &linearSampleCreateInfo, VK_ALLOC, &context.linearSampler));
    const VkSamplerCreateInfo minSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(const VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &minSamplerCreateInfo, VK_ALLOC, &context.minSampler));
    const VkSamplerCreateInfo maxSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(const VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    VK_REQUIRE(vkCreateSampler(context.device, &maxSamplerCreateInfo, VK_ALLOC, &context.minSampler));

#undef DEFAULT_LINEAR_SAMPLER
  }
  {  // Swapchain
    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.surface,
        .minImageCount = VK_SWAP_COUNT,
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
    VK_REQUIRE(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, VK_ALLOC, &context.swap.chain));
    uint32_t swapCount;
    VK_REQUIRE(vkGetSwapchainImagesKHR(context.device, context.swap.chain, &swapCount, NULL));
    REQUIRE(swapCount == VK_SWAP_COUNT, "Resulting swap image count does not match requested swap count!");
    VK_REQUIRE(vkGetSwapchainImagesKHR(context.device, context.swap.chain, &swapCount, context.swapImages));

    for (int i = 0; i < VK_SWAP_COUNT; ++i) {
      const VkImageViewCreateInfo imageViewCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = context.swapImages[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_B8G8R8A8_SRGB,
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
      };
      VK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VK_ALLOC, &context.swapImageViews[i]));
      TransitionImageLayoutImmediate(&UNDEFINED_IMAGE_BARRIER, &PRESENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, context.swapImages[i]);
    }
  }
  {  // Semaphores
    const VkSemaphoreCreateInfo acquireSwapSemaphoreCreateInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_REQUIRE(vkCreateSemaphore(context.device, &acquireSwapSemaphoreCreateInfo, VK_ALLOC, &context.swap.acquireSemaphore));
    VK_REQUIRE(vkCreateSemaphore(context.device, &acquireSwapSemaphoreCreateInfo, VK_ALLOC, &context.swap.renderCompleteSemaphore));
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineSemaphoreTypeCreateInfo,
    };
    VK_REQUIRE(vkCreateSemaphore(context.device, &timelineSemaphoreCreateInfo, VK_ALLOC, &context.timeline.semaphore));
  }
  {  // Standard Pipeline
    CreateGlobalSetLayout();
    CreateStandardMaterialSetLayout();
    CreateStandardObjectSetLayout();
    CreateStandardPipelineLayout();
    CreateStandardPipeline();
  }
  return &context;
}

static int  GenerateSphereVertexCount(const int slicesCount, const int stacksCount) { return (slicesCount + 1) * (stacksCount + 1); }
static int  GenerateSphereIndexCount(const int slicesCount, const int stacksCount) { return slicesCount * stacksCount * 2 * 3; }
static void GenerateSphere(const int slicesCount, const int stacksCount, const float radius, Vertex* pVertex) {
  const float slices = (float)slicesCount;
  const float stacks = (float)stacksCount;
  const float dtheta = 2.0f * PI / slices;
  const float dphi = PI / stacks;
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
      pVertex[idx++] = (Vertex){
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
void CreateSphereMeshBuffers(const float radius, const int slicesCount, const int stackCount, Mesh* pMesh) {
  {
    pMesh->indexCount = GenerateSphereIndexCount(slicesCount, stackCount);
    uint16_t pIndices[pMesh->indexCount];
    GenerateSphereIndices(slicesCount, stackCount, pIndices);
    uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &pMesh->indexMemory, &pMesh->indexBuffer);
    PopulateBufferViaStaging(pIndices, indexBufferSize, pMesh->indexBuffer);
  }
  {
    pMesh->vertexCount = GenerateSphereVertexCount(slicesCount, stackCount);
    Vertex pVertices[pMesh->vertexCount];
    GenerateSphere(slicesCount, stackCount, radius, pVertices);
    uint32_t vertexBufferSize = sizeof(Vertex) * pMesh->vertexCount;
    CreateAllocateBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &pMesh->vertexMemory, &pMesh->vertexBuffer);
    PopulateBufferViaStaging(pVertices, vertexBufferSize, pMesh->vertexBuffer);
  }
}

void CreateFramebuffers(const uint32_t framebufferCount, Framebuffer* pFrameBuffers) {
  for (int i = 0; i < framebufferCount; ++i) {
    VkImageCreateInfo imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
    imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    imageCreateInfo.format = RENDER_PASS_FORMATS[PASS_COLOR_ATTACHMENT];
    imageCreateInfo.usage = RENDER_PASS_USAGES[PASS_COLOR_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].colorImageMemory, &pFrameBuffers[i].colorImage, &pFrameBuffers[i].colorImageView);
    imageCreateInfo.format = RENDER_PASS_FORMATS[PASS_NORMAL_ATTACHMENT];
    imageCreateInfo.usage = RENDER_PASS_USAGES[PASS_NORMAL_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].normalImageMemory, &pFrameBuffers[i].normalImage, &pFrameBuffers[i].normalImageView);
    imageCreateInfo.format = RENDER_PASS_FORMATS[PASS_DEPTH_ATTACHMENT];
    imageCreateInfo.usage = RENDER_PASS_USAGES[PASS_DEPTH_ATTACHMENT];
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_DEPTH_BIT, &pFrameBuffers[i].depthImageMemory, &pFrameBuffers[i].depthImage, &pFrameBuffers[i].depthImageView);

    imageCreateInfo.format = G_BUFFER_FORMAT;
    imageCreateInfo.usage = G_BUFFER_USAGE;
    CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, &pFrameBuffers[i].gBufferImageMemory, &pFrameBuffers[i].gBufferImage, &pFrameBuffers[i].gBufferImageView);

    VkImageView attachments[PASS_ATTACHMENT_COUNT];
    attachments[PASS_COLOR_ATTACHMENT] = pFrameBuffers[i].colorImageView;
    attachments[PASS_NORMAL_ATTACHMENT] = pFrameBuffers[i].normalImageView;
    attachments[PASS_DEPTH_ATTACHMENT] = pFrameBuffers[i].depthImageView;
    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = context.renderPass,
        .attachmentCount = PASS_ATTACHMENT_COUNT,
        .pAttachments = attachments,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .layers = 1,
    };
    VK_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VK_ALLOC, &pFrameBuffers[i].framebuffer));
    VK_REQUIRE(vkCreateSemaphore(context.device, &(VkSemaphoreCreateInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}, VK_ALLOC, &pFrameBuffers[i].renderCompleteSemaphore));
  }
}