#include "renderer.h"
#include "globals.h"
#include "stb_image.h"
#include "window.h"

#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VkInstance                      instance = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;

_Thread_local VkmContext context = {};

//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

static void VkmReadFile(const char* pPath, size_t* pFileLength, char** ppFileContents) {
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

static VkBool32 VkmDebugUtilsCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  switch (messageSeverity) {
    default:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("%s\n", pCallbackData->pMessage); return VK_FALSE;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   PANIC(pCallbackData->pMessage); return VK_FALSE;
  }
}

static VkCommandBuffer VkmBeginImmediateCommandBuffer(const VkCommandPool commandPool) {
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
  VkmReadFile(pShaderPath, &codeSize, &pCode);
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
      .pStages = (const VkPipelineShaderStageCreateInfo[]){
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
      .pVertexInputState = &(const VkPipelineVertexInputStateCreateInfo){
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
          .pVertexAttributeDescriptions = (const VkVertexInputAttributeDescription[]){
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

void vkmAllocateDescriptorSet(const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSet) {
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
static void VkmAllocMemory(const VkMemoryRequirements* pMemoryRequirements, const VkMemoryPropertyFlags pMemoryPropertyFlags, const VkmLocality locality, VkDeviceMemory* pDeviceMemory) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memoryProperties);
  const uint32_t memoryTypeIndex = FindMemoryTypeIndex(&memoryProperties, pMemoryRequirements, pMemoryPropertyFlags);
  // todo your supposed check if it wants dedicated memory
  //    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
  //            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
  //            .image = pTestTexture->image,
  //            .buffer = VK_NULL_HANDLE,
  //    };
  const VkExportMemoryAllocateInfo exportMemoryAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      //            .pNext =&dedicatedAllocInfo,
      .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
  };
  const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = locality == VKM_LOCALITY_EXTERNAL_PROCESS_SHARED ? &exportMemoryAllocInfo : NULL,
      .allocationSize = pMemoryRequirements->size,
      .memoryTypeIndex = memoryTypeIndex,
  };
  VKM_REQUIRE(vkAllocateMemory(context.device, &ai, VKM_ALLOC, pDeviceMemory));
}
static void VkmCreateAllocBindBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const VkmLocality locality, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = usage,
  };
  VKM_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, NULL, pBuffer));
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(context.device, *pBuffer, &memoryRequirements);
  VkmAllocMemory(&memoryRequirements, memoryPropertyFlags, locality, pDeviceMemory);
  VKM_REQUIRE(vkBindBufferMemory(context.device, *pBuffer, *pDeviceMemory, 0));
}
void vkmCreateAllocBindMapBuffer(const VkMemoryPropertyFlags memoryPropertyFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const VkmLocality locality, VkDeviceMemory* pDeviceMemory, VkBuffer* pBuffer, void** ppMapped) {
  VkmCreateAllocBindBuffer(memoryPropertyFlags, bufferSize, usage, locality, pDeviceMemory, pBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pDeviceMemory, 0, bufferSize, 0, ppMapped));
}
static void CreateStagingBuffer(const void* srcData, const VkDeviceSize bufferSize, const VkmLocality locality, VkDeviceMemory* pStagingBufferMemory, VkBuffer* pStagingBuffer) {
  void* dstData;
  VkmCreateAllocBindBuffer(VKM_MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, locality, pStagingBufferMemory, pStagingBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pStagingBufferMemory, 0, bufferSize, 0, &dstData));
  memcpy(dstData, srcData, bufferSize);
  vkUnmapMemory(context.device, *pStagingBufferMemory);
}
static void PopulateBufferViaStaging(const VkCommandPool pool, const VkQueue queue, const void* srcData, const VkDeviceSize bufferSize, const VkBuffer buffer) {
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(srcData, bufferSize, VKM_LOCALITY_NODE_LOCAL, &stagingBufferMemory, &stagingBuffer);
  const VkCommandBuffer commandBuffer = VkmBeginImmediateCommandBuffer(pool);
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.size = bufferSize});
  EndImmediateCommandBuffer(pool, queue, commandBuffer);
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
static void CreateAllocBindImage(const VkImageCreateInfo* pImageCreateInfo, const VkmLocality locality, VkDeviceMemory* pMemory, VkImage* pImage) {
  VKM_REQUIRE(vkCreateImage(context.device, pImageCreateInfo, VKM_ALLOC, pImage));
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, *pImage, &memRequirements);
  VkmAllocMemory(&memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, locality, pMemory);
  VKM_REQUIRE(vkBindImageMemory(context.device, *pImage, *pMemory, 0));
}
static void CreateAllocateBindImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImageAspectFlags aspectMask, const VkmLocality locality, VkDeviceMemory* pMemory, VkImage* pImage, VkImageView* pImageView) {
  CreateAllocBindImage(pImageCreateInfo, locality, pMemory, pImage);
  CreateImageView(pImageCreateInfo, *pImage, aspectMask, pImageView);
}

typedef struct VkmTextureCreateInfo {
  VkImageCreateInfo  imageCreateInfo;
  VkImageAspectFlags aspectMask;
  VkmLocality        locality;
} VkmTextureCreateInfo;
void VkmCreateTexture(const VkmTextureCreateInfo* pTextureCreateInfo, VkmTexture* pTexture) {
  VKM_REQUIRE(vkCreateImage(context.device, &pTextureCreateInfo->imageCreateInfo, VKM_ALLOC, &pTexture->image));
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, pTexture->image, &memRequirements);
  VkmAllocMemory(&memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pTextureCreateInfo->locality, &pTexture->imageMemory);
  VKM_REQUIRE(vkBindImageMemory(context.device, pTexture->image, pTexture->imageMemory, 0));
  CreateImageView(&pTextureCreateInfo->imageCreateInfo, pTexture->image, pTextureCreateInfo->aspectMask, &pTexture->imageView);
  switch (pTextureCreateInfo->locality) {
    default:
    case VKM_LOCALITY_NODE_LOCAL:              break;
    case VKM_LOCALITY_CONTEXT_SHARED:          break;
    case VKM_LOCALITY_DEVICE_SHARED:           break;
    case VKM_LOCALITY_EXTERNAL_PROCESS_SHARED: {
      const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .memory = pTexture->imageMemory,
          .handleType = VKM_EXTERNAL_HANDLE_TYPE};
      VKM_INSTANCE_FUNC(vkGetMemoryWin32HandleKHR);
      VKM_REQUIRE(vkGetMemoryWin32HandleKHR(context.device, &getWin32HandleInfo, &pTexture->externalHandle));
      break;
    }
  }
}
void vkmCreateTextureFromFile(const VkCommandPool pool, const VkQueue queue, const char* pPath, VkmTexture* pTexture) {
  int      texChannels, width, height;
  stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
  REQUIRE(width > 0 && height > 0, "Image height or width is equal to zero.")
  const VkDeviceSize imageBufferSize = width * height * 4;
  VkImageCreateInfo  imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO;
  imageCreateInfo.extent = (VkExtent3D){width, height, 1};
  imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, VKM_LOCALITY_NODE_LOCAL, &pTexture->imageMemory, &pTexture->image, &pTexture->imageView);

  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(pImagePixels, imageBufferSize, VKM_LOCALITY_NODE_LOCAL, &stagingBufferMemory, &stagingBuffer);
  stbi_image_free(pImagePixels);
  const VkCommandBuffer commandBuffer = VkmBeginImmediateCommandBuffer(pool);
  vkmCommandPipelineImageBarrier(commandBuffer, 1, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image));
  const VkBufferImageCopy region = {
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkmCommandPipelineImageBarrier(commandBuffer, 1, &VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_TRANSFER_READ_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, pTexture->image));
  EndImmediateCommandBuffer(pool, queue, commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VKM_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VKM_ALLOC);
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
void vkmCreateStandardFramebuffers(const VkRenderPass renderPass, const uint32_t framebufferCount, const VkmLocality locality, VkmFramebuffer* pFrameBuffers) {
  const VkExternalMemoryImageCreateInfo externalImageInfo = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
  };
  VkmTextureCreateInfo textureCreateInfo = {
      .imageCreateInfo = DEFAULT_IMAGE_CREATE_INFO,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .locality = locality,
  };
  switch (locality) {
    default:
    case VKM_LOCALITY_NODE_LOCAL:              break;
    case VKM_LOCALITY_CONTEXT_SHARED:          break;
    case VKM_LOCALITY_DEVICE_SHARED:           break;
    case VKM_LOCALITY_EXTERNAL_PROCESS_SHARED: textureCreateInfo.imageCreateInfo.pNext = &externalImageInfo; break;
  }
  for (int i = 0; i < framebufferCount; ++i) {
    textureCreateInfo.imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    VkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].color);

    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    VkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].normal);

    textureCreateInfo.imageCreateInfo.format = VKM_PASS_STD_FORMATS[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.imageCreateInfo.usage = VKM_PASS_STD_USAGES[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].depth);

    textureCreateInfo.imageCreateInfo.format = VKM_STD_G_BUFFER_FORMAT;
    textureCreateInfo.imageCreateInfo.usage = VKM_STD_G_BUFFER_USAGE;
    textureCreateInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].gBuffer);

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = VKM_PASS_ATTACHMENT_STD_COUNT,
        .pAttachments = (const VkImageView[]){
            [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = pFrameBuffers[i].color.imageView,
            [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = pFrameBuffers[i].normal.imageView,
            [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = pFrameBuffers[i].depth.imageView,
        },
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .layers = 1,
    };
    VKM_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VKM_ALLOC, &pFrameBuffers[i].framebuffer));
    VKM_REQUIRE(vkCreateSemaphore(context.device, &(VkSemaphoreCreateInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}, VKM_ALLOC, &pFrameBuffers[i].renderCompleteSemaphore));
  }
}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------

static uint32_t FindQueueIndex(const VkPhysicalDevice physicalDevice, const VkmQueueFamilyCreateInfo* pQueueDesc) {
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

  for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex) {

    bool graphicsSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    bool computeSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    bool transferSupport = queueFamilyProperties[queueFamilyIndex].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;

    if (pQueueDesc->supportsGraphics == VKM_SUPPORT_YES && !graphicsSupport) continue;
    if (pQueueDesc->supportsGraphics == VKM_SUPPORT_NO && graphicsSupport) continue;
    if (pQueueDesc->supportsCompute == VKM_SUPPORT_YES && !computeSupport) continue;
    if (pQueueDesc->supportsCompute == VKM_SUPPORT_NO && computeSupport) continue;
    if (pQueueDesc->supportsTransfer == VKM_SUPPORT_YES && !transferSupport) continue;
    if (pQueueDesc->supportsTransfer == VKM_SUPPORT_NO && transferSupport) continue;

    if (pQueueDesc->globalPriority != 0) {
      bool globalPrioritySupported = false;
      for (int i = 0; i < queueFamilyGlobalPriorityProperties[queueFamilyIndex].priorityCount; ++i) {
        if (queueFamilyGlobalPriorityProperties[queueFamilyIndex].priorities[i] == pQueueDesc->globalPriority) {
          globalPrioritySupported = true;
          break;
        }
      }

      if (!globalPrioritySupported)
        continue;
    }

    return queueFamilyIndex;
  }

  fprintf(stderr, "Can't find queue family: graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
          string_Support[pQueueDesc->supportsGraphics],
          string_Support[pQueueDesc->supportsCompute],
          string_Support[pQueueDesc->supportsTransfer],
          string_VkQueueGlobalPriorityKHR(pQueueDesc->globalPriority));
  PANIC("Can't find queue family");
  return -1;
}

void vkmInitialize() {
  {
    const char* ppEnabledLayerNames[] = {
        //        "VK_LAYER_KHRONOS_validation",
        //        "VK_LAYER_LUNARG_monitor"
    };
    const char* ppEnabledInstanceExtensionNames[] = {
        //        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        //        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VKM_PLATFORM_SURFACE_EXTENSION_NAME,
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
    VKM_REQUIRE(vkCreateInstance(&instanceCreationInfo, VKM_ALLOC, &instance));
    printf("Instance Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }
  {
    //    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = {};
    //    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    //    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    //    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    //    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //    VkDebugUtilsMessageTypeFlagsEXT messageType = {};
    //    // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    //    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    //    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    //    const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
    //        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    //        .messageSeverity = messageSeverity,
    //        .messageType = messageType,
    //        .pfnUserCallback = VkmDebugUtilsCallback,
    //    };
    //    VKM_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
    //    VKM_REQUIRE(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, VKM_ALLOC, &debugUtilsMessenger));
  }
}

void vkmCreateContext(const VkmContextCreateInfo* pContextCreateInfo, VkmContext* pContext) {
  REQUIRE(instance != NULL, "Trying to create Context when Instance has not been created!");

  {  // PhysicalDevice
    uint32_t deviceCount = 0;
    VKM_REQUIRE(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    VKM_REQUIRE(vkEnumeratePhysicalDevices(instance, &deviceCount, devices));
    context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    REQUIRE(physicalDeviceProperties.properties.apiVersion >= VKM_VERSION, "Insufficient Vulkan API Version");
  }

  {  // Device
    const VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT physicalDevicePageableDeviceLocalMemoryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
        .pageableDeviceLocalMemory = VK_TRUE,
    };
    const VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
        .pNext = (void*)&physicalDevicePageableDeviceLocalMemoryFeatures,
        .globalPriorityQuery = VK_TRUE,
    };
    const VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = (void*)&physicalDeviceGlobalPriorityQueryFeatures,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
        .nullDescriptor = VK_FALSE,
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
        //        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
        //        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        //        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        //        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME,
        //        VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME,
        VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
        VKM_EXTERNAL_MEMORY_EXTENSION_NAME,
        VKM_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VKM_EXTERNAL_FENCE_EXTENSION_NAME,
    };
    uint32_t activeQueueIndex = 0;
    uint32_t activeQueueCount = 0;
    for (int i = 0; i < VKM_QUEUE_FAMILY_TYPE_COUNT; ++i) activeQueueCount += pContextCreateInfo->queueFamilyCreateInfos[i].queueCount > 0;
    VkDeviceQueueCreateInfo                  activeQueueCreateInfos[activeQueueCount];
    VkDeviceQueueGlobalPriorityCreateInfoEXT activeQueueGlobalPriorityCreateInfos[activeQueueCount];
    for (int queueFamilyTypeIndex = 0; queueFamilyTypeIndex < VKM_QUEUE_FAMILY_TYPE_COUNT; ++queueFamilyTypeIndex) {
      if (pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].queueCount == 0) continue;
      context.queueFamilies[queueFamilyTypeIndex].index = FindQueueIndex(context.physicalDevice, &pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex]);
      activeQueueGlobalPriorityCreateInfos[activeQueueIndex] = (VkDeviceQueueGlobalPriorityCreateInfoEXT){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
          .globalPriority = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].globalPriority,
      };
      activeQueueCreateInfos[activeQueueIndex] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].globalPriority != 0 ? &activeQueueGlobalPriorityCreateInfos[activeQueueIndex] : NULL,
          .queueFamilyIndex = context.queueFamilies[queueFamilyTypeIndex].index,
          .queueCount = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].queueCount,
          .pQueuePriorities = pContextCreateInfo->queueFamilyCreateInfos[queueFamilyTypeIndex].pQueuePriorities};
      activeQueueIndex++;
    }
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = activeQueueCount,
        .pQueueCreateInfos = activeQueueCreateInfos,
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    VKM_REQUIRE(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VKM_ALLOC, &context.device));
  }

  for (int i = 0; i < VKM_QUEUE_FAMILY_TYPE_COUNT; ++i) {
    if (pContextCreateInfo->queueFamilyCreateInfos[i].queueCount == 0)
      continue;

    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queueFamilies[i].index,
    };
    VKM_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &context.queueFamilies[i].pool));
  }

  {  // Semaphore
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineSemaphoreTypeCreateInfo,
    };
    VKM_REQUIRE(vkCreateSemaphore(context.device, &timelineSemaphoreCreateInfo, VKM_ALLOC, &context.timeline.semaphore));
    context.timeline.value = 0;
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

void VkmCreateStandardRenderPass(VkRenderPass* pRenderPass) {
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

void VkmCreateSampler(const VkmSamplerDesc* pDesc, VkSampler* pSampler) {
  const VkSamplerCreateInfo minSamplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = &(const VkSamplerReductionModeCreateInfo){
          .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
          .reductionMode = pDesc->reductionMode,
      },
      .magFilter = pDesc->filter,
      .minFilter = pDesc->filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .maxLod = 16.0,
      .addressModeU = pDesc->addressMode,
      .addressModeV = pDesc->addressMode,
      .addressModeW = pDesc->addressMode,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK};
  VKM_REQUIRE(vkCreateSampler(context.device, &minSamplerCreateInfo, VKM_ALLOC, pSampler));
}

void VkmCreateSwap(const VkSurfaceKHR surface, VkmSwap* pSwap) {
  const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
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

void vkmCreateStandardPipeline(const VkRenderPass renderPass, VkmStandardPipe* pStandardPipeline) {
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
static void GenerateSphereIndices(const int slicesCount, const int stacksCount, uint16_t* pIndices) {
  int idx = 0;
  for (int i = 0; i < stacksCount; i++) {
    for (int j = 0; j < slicesCount; j++) {
      const uint16_t v1 = i * (slicesCount + 1) + j;
      const uint16_t v2 = i * (slicesCount + 1) + j + 1;
      const uint16_t v3 = (i + 1) * (slicesCount + 1) + j;
      const uint16_t v4 = (i + 1) * (slicesCount + 1) + j + 1;
      pIndices[idx++] = v1;
      pIndices[idx++] = v2;
      pIndices[idx++] = v3;
      pIndices[idx++] = v2;
      pIndices[idx++] = v4;
      pIndices[idx++] = v3;
    }
  }
}
void vkmCreateSphereMesh(const VkCommandPool pool, const VkQueue queue, const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh) {
  {
    pMesh->indexCount = GenerateSphereIndexCount(slicesCount, stackCount);
    uint16_t pIndices[pMesh->indexCount];
    GenerateSphereIndices(slicesCount, stackCount, pIndices);
    uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
    VkmCreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &pMesh->indexMemory, &pMesh->indexBuffer);
    PopulateBufferViaStaging(pool, queue, pIndices, indexBufferSize, pMesh->indexBuffer);
  }
  {
    pMesh->vertexCount = GenerateSphereVertexCount(slicesCount, stackCount);
    VkmVertex pVertices[pMesh->vertexCount];
    GenerateSphere(slicesCount, stackCount, radius, pVertices);
    uint32_t vertexBufferSize = sizeof(VkmVertex) * pMesh->vertexCount;
    VkmCreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &pMesh->vertexMemory, &pMesh->vertexBuffer);
    PopulateBufferViaStaging(pool, queue, pVertices, vertexBufferSize, pMesh->vertexBuffer);
  }
}
