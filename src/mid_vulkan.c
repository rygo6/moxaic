#include "mid_vulkan.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Thread_local VkInstance instance = VK_NULL_HANDLE;
_Thread_local VkmContext context;

static VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;

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

static VkCommandBuffer VkmBeginImmediateCommandBuffer() {
  VkCommandBuffer                   commandBuffer;
  const VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].pool,
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
static void VkmEndImmediateCommandBuffer(VkCommandBuffer commandBuffer) {
  VKM_REQUIRE(vkEndCommandBuffer(commandBuffer));
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  VKM_REQUIRE(vkQueueSubmit(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].queue, 1, &submitInfo, VK_NULL_HANDLE));
  VKM_REQUIRE(vkQueueWaitIdle(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].queue));
  vkFreeCommandBuffers(context.device, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_DEDICATED_TRANSFER].pool, 1, &commandBuffer);
}

//----------------------------------------------------------------------------------
// Pipelines
//----------------------------------------------------------------------------------

#define COLOR_WRITE_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

void vkmCreateShaderModule(const char* pShaderPath, VkShaderModule* pShaderModule) {
  size_t codeSize;
  char*  pCode;
  VkmReadFile(pShaderPath, &codeSize, &pCode);
  const VkShaderModuleCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = codeSize,
      .pCode = (uint32_t*)pCode,
  };
  VKM_REQUIRE(vkCreateShaderModule(context.device, &createInfo, VKM_ALLOC, pShaderModule));
  free(pCode);
}

// Standard Pipeline
#define VKM_PIPE_VERTEX_BINDING_STD_INDEX 0
enum VkmPipeVertexAttributeStandardIndices {
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_POSITION_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_NORMAL_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_UV_INDEX,
  VKM_PIPE_VERTEX_ATTRIBUTE_STD_COUNT,
};
static void CreateStdPipeLayout() {
  VkDescriptorSetLayout pSetLayouts[VKM_PIPE_SET_STD_INDEX_COUNT];
  pSetLayouts[VKM_PIPE_SET_STD_GLOBAL_INDEX] = context.stdPipeLayout.globalSetLayout;
  pSetLayouts[VKM_PIPE_SET_STD_MATERIAL_INDEX] = context.stdPipeLayout.materialSetLayout;
  pSetLayouts[VKM_PIPE_SET_STD_OBJECT_INDEX] = context.stdPipeLayout.objectSetLayout;
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = VKM_PIPE_SET_STD_INDEX_COUNT,
      .pSetLayouts = pSetLayouts,
  };
  VKM_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VKM_ALLOC, &context.stdPipeLayout.pipeLayout));
}

#define DEFAULT_ROBUSTNESS_STATE                                                         \
  (const VkPipelineRobustnessCreateInfoEXT) {                                            \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,                      \
    .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT, \
    .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT, \
    .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,   \
    .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT,           \
  }
#define DEFAULT_VERTEX_INPUT_STATE                                               \
  (const VkPipelineVertexInputStateCreateInfo) {                                 \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,          \
    .vertexBindingDescriptionCount = 1,                                          \
    .pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){           \
        {                                                                        \
            .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,                        \
            .stride = sizeof(VkmVertex),                                         \
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,                            \
        },                                                                       \
    },                                                                           \
    .vertexAttributeDescriptionCount = 3,                                        \
                                                                                 \
    .pVertexAttributeDescriptions = (const VkVertexInputAttributeDescription[]){ \
        {                                                                        \
            .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_POSITION_INDEX,            \
            .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,                        \
            .format = VK_FORMAT_R32G32B32_SFLOAT,                                \
            .offset = offsetof(VkmVertex, position),                             \
        },                                                                       \
        {                                                                        \
            .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_NORMAL_INDEX,              \
            .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,                        \
            .format = VK_FORMAT_R32G32B32_SFLOAT,                                \
            .offset = offsetof(VkmVertex, normal),                               \
        },                                                                       \
        {                                                                        \
            .location = VKM_PIPE_VERTEX_ATTRIBUTE_STD_UV_INDEX,                  \
            .binding = VKM_PIPE_VERTEX_BINDING_STD_INDEX,                        \
            .format = VK_FORMAT_R32G32_SFLOAT,                                   \
            .offset = offsetof(VkmVertex, uv),                                   \
        },                                                                       \
    },                                                                           \
  }
#define DEFAULT_VIEWPORT_STATE                                      \
  (const VkPipelineViewportStateCreateInfo) {                       \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, \
    .viewportCount = 1,                                             \
    .scissorCount = 1,                                              \
  }
#ifdef VKM_DEBUG_WIREFRAME
  #define FILL_MODE VK_POLYGON_MODE_LINE
#else
  #define FILL_MODE VK_POLYGON_MODE_FILL
#endif
#define DEFAULT_RASTERIZATION_STATE                                      \
  (const VkPipelineRasterizationStateCreateInfo) {                       \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, \
    .polygonMode = FILL_MODE,                                            \
    .frontFace = VK_FRONT_FACE_CLOCKWISE,                                \
    .lineWidth = 1.0f,                                                   \
  }
#define DEFAULT_DEPTH_STENCIL_STATE                                      \
  (const VkPipelineDepthStencilStateCreateInfo) {                        \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, \
    .depthTestEnable = VK_TRUE,                                          \
    .depthWriteEnable = VK_TRUE,                                         \
    .depthCompareOp = VK_COMPARE_OP_GREATER,                             \
    .maxDepthBounds = 1.0f,                                              \
  }
#define DEFAULT_DYNAMIC_STATE                                      \
  (const VkPipelineDynamicStateCreateInfo) {                       \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, \
    .dynamicStateCount = 2,                                        \
    .pDynamicStates = (const VkDynamicState[]){                    \
        VK_DYNAMIC_STATE_VIEWPORT,                                 \
        VK_DYNAMIC_STATE_SCISSOR,                                  \
    },                                                             \
  }
#define DEFAULT_OPAQUE_COLOR_BLEND_STATE                               \
  (const VkPipelineColorBlendStateCreateInfo) {                        \
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, \
    .logicOp = VK_LOGIC_OP_COPY,                                       \
    .attachmentCount = 2,                                              \
    .pAttachments = (const VkPipelineColorBlendAttachmentState[]){     \
        {/* Color */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},         \
        {/* Normal */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},        \
    },                                                                 \
  }

void vkmCreateBasicPipe(const char* vertShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe) {
  VkShaderModule vertShader; vkmCreateShaderModule(vertShaderPath, &vertShader);
  VkShaderModule fragShader; vkmCreateShaderModule(fragShaderPath, &fragShader);
  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &DEFAULT_ROBUSTNESS_STATE,
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
      .pVertexInputState = &DEFAULT_VERTEX_INPUT_STATE,
      .pInputAssemblyState = &(const VkPipelineInputAssemblyStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      },
      .pViewportState = &DEFAULT_VIEWPORT_STATE,
      .pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
      .pMultisampleState = &(const VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
      .pColorBlendState = &DEFAULT_OPAQUE_COLOR_BLEND_STATE,
      .pDynamicState = &DEFAULT_DYNAMIC_STATE,
      .layout = layout,
      .renderPass = renderPass,
  };
  VKM_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VKM_ALLOC, pPipe));
  vkDestroyShaderModule(context.device, fragShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, vertShader, VKM_ALLOC);
}

void vkmCreateTessPipe(const char* vertShaderPath, const char* tescShaderPath, const char* teseShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe) {
  VkShaderModule vertShader; vkmCreateShaderModule(vertShaderPath, &vertShader);
  VkShaderModule tescShader; vkmCreateShaderModule(tescShaderPath, &tescShader);
  VkShaderModule teseShader; vkmCreateShaderModule(teseShaderPath, &teseShader);
  VkShaderModule fragShader; vkmCreateShaderModule(fragShaderPath, &fragShader);
  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &DEFAULT_ROBUSTNESS_STATE,
      .stageCount = 4,
      .pStages = (const VkPipelineShaderStageCreateInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_VERTEX_BIT,
              .module = vertShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
              .module = tescShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
              .module = teseShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
              .module = fragShader,
              .pName = "main",
          },
      },
      .pVertexInputState = &DEFAULT_VERTEX_INPUT_STATE,
      .pTessellationState = &(const VkPipelineTessellationStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
          .patchControlPoints = 4,
      },
      .pInputAssemblyState = &(const VkPipelineInputAssemblyStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
      },
      .pViewportState = &DEFAULT_VIEWPORT_STATE,
      .pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
      .pMultisampleState = &(const VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
      .pColorBlendState = &DEFAULT_OPAQUE_COLOR_BLEND_STATE,
      .pDynamicState = &DEFAULT_DYNAMIC_STATE,
      .layout = layout,
      .renderPass = renderPass,
  };
  VKM_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VKM_ALLOC, pPipe));
  vkDestroyShaderModule(context.device, fragShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, tescShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, teseShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, vertShader, VKM_ALLOC);
}


void vkmCreateTaskMeshPipe(const char* taskShaderPath, const char* meshShaderPath, const char* fragShaderPath, const VkRenderPass renderPass, const VkPipelineLayout layout, VkPipeline* pPipe) {
  VkShaderModule taskShader; vkmCreateShaderModule(taskShaderPath, &taskShader);
  VkShaderModule meshShader; vkmCreateShaderModule(meshShaderPath, &meshShader);
  VkShaderModule fragShader; vkmCreateShaderModule(fragShaderPath, &fragShader);
  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &DEFAULT_ROBUSTNESS_STATE,
      .stageCount = 3,
      .pStages = (const VkPipelineShaderStageCreateInfo[]){
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_TASK_BIT_EXT,
              .module = taskShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_MESH_BIT_EXT,
              .module = meshShader,
              .pName = "main",
          },
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
              .module = fragShader,
              .pName = "main",
          },
      },
      .pViewportState = &DEFAULT_VIEWPORT_STATE,
      .pRasterizationState = &DEFAULT_RASTERIZATION_STATE,
      .pMultisampleState = &(const VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &DEFAULT_DEPTH_STENCIL_STATE,
      .pColorBlendState = &DEFAULT_OPAQUE_COLOR_BLEND_STATE,
      .pDynamicState = &DEFAULT_DYNAMIC_STATE,
      .layout = layout,
      .renderPass = renderPass,
  };
  VKM_REQUIRE(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VKM_ALLOC, pPipe));
  vkDestroyShaderModule(context.device, fragShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, taskShader, VKM_ALLOC);
  vkDestroyShaderModule(context.device, meshShader, VKM_ALLOC);
}

//----------------------------------------------------------------------------------
// Descriptors
//----------------------------------------------------------------------------------

static void CreateGlobalSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BIND_STD_GLOBAL_BUFFER,
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
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &context.stdPipeLayout.globalSetLayout))
}
static void CreateStdMaterialSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BIND_STD_MATERIAL_TEXTURE,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pImmutableSamplers = &context.linearSampler,
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &context.stdPipeLayout.materialSetLayout));
}
static void CreateStdObjectSetLayout() {
  const VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(const VkDescriptorSetLayoutBinding){
          .binding = VKM_SET_BIND_STD_OBJECT_BUFFER,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &createInfo, VKM_ALLOC, &context.stdPipeLayout.objectSetLayout));
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
void vkmAllocateDescriptorSets(const VkDescriptorPool descriptorPool, const uint32_t descriptorSetCount, const VkDescriptorSetLayout* pSetLayout, VkDescriptorSet* pSets) {
  const VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .descriptorSetCount = descriptorSetCount,
      .pSetLayouts = pSetLayout,
  };
  VKM_REQUIRE(vkAllocateDescriptorSets(context.device, &allocateInfo, pSets));
}

//----------------------------------------------------------------------------------
// Memory
//----------------------------------------------------------------------------------

static uint32_t FindMemoryTypeIndex(const uint32_t memoryTypeCount, VkMemoryType* pMemoryTypes, const uint32_t memoryTypeBits, const VkMemoryPropertyFlags memPropFlags) {
  for (uint32_t i = 0; i < memoryTypeCount; i++) {
    bool hasTypeBits = memoryTypeBits & 1 << i;
    bool hasPropertyFlags = (pMemoryTypes[i].propertyFlags & memPropFlags) == memPropFlags;
    if (hasTypeBits && hasPropertyFlags) {
      return i;
    }
  }
  int                   index = 0;
  VkMemoryPropertyFlags printFlags = memPropFlags;
  while (printFlags) {
    if (printFlags & 1) {
      printf(" %s ", string_VkMemoryPropertyFlagBits(1U << index));
    }
    ++index;
    printFlags >>= 1;
  }
  PANIC("Failed to find memory with properties!");
  return -1;
}


static size_t  requestedMemoryAllocSize[VK_MAX_MEMORY_TYPES] = {};
static size_t  externalRequestedMemoryAllocSize[VK_MAX_MEMORY_TYPES] = {};
VkDeviceMemory deviceMemory[VK_MAX_MEMORY_TYPES] = {};
void*          pMappedMemory[VK_MAX_MEMORY_TYPES] = {NULL};
//VkDeviceMemory localMemory;
//VkDeviceMemory localVisibleCoherentMemory;
//VkDeviceMemory visibleCoherentMemory;
//static void    VkmAllocContextMemory(const VkMemoryRequirements* pMemReqs, const VkMemoryPropertyFlags memPropFlags, const VkmLocality locality, VkDeviceMemory* pDeviceMem) {
//  VkPhysicalDeviceMemoryProperties memProps;
//  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);
//  const uint32_t                   memTypeIndex = FindMemoryTypeIndex(memProps.memoryTypeCount, memProps.memoryTypes, pMemReqs->memoryTypeBits, memPropFlags);
//  const VkExportMemoryAllocateInfo exportMemAllocInfo = {
//         .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
//         .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
//  };
//  const VkMemoryAllocateInfo memAllocInfo = {
//         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//         .pNext = locality == VKM_LOCALITY_PROCESS_EXPORTED ? &exportMemAllocInfo : NULL,
//         .allocationSize = pMemReqs->size,
//         .memoryTypeIndex = memTypeIndex,
//  };
//  VKM_REQUIRE(vkAllocateMemory(context.device, &memAllocInfo, VKM_ALLOC, pDeviceMem));
//  totalAllocSize += pMemReqs->size;
//  printf("%zu allocated\n", totalAllocSize);
//}

static void PrintMemoryPropertyFlags(VkMemoryPropertyFlags propFlags) {
  int index = 0;
  while (propFlags) {
    if (propFlags & 1) {
      printf("%s ", strlen("VK_MEMORY_PROPERTY_") + string_VkMemoryPropertyFlagBits(1U << index));
    }
    ++index;
    propFlags >>= 1;
  }
  printf("\n");
}

void vkmBeginAllocationRequests() {
  printf("Begin Memory Allocation Requests.\n");
  // what do I do here? should I enable a mechanic to do this twice? or on pass in memory?
  for (int memTypeIndex = 0; memTypeIndex < VK_MAX_MEMORY_TYPES; ++memTypeIndex) {
    requestedMemoryAllocSize[memTypeIndex] = 0;
  }
}
void vkmEndAllocationRequests() {
  printf("End Memory Allocation Requests.\n");
  VkPhysicalDeviceMemoryProperties2 memProps2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
  vkGetPhysicalDeviceMemoryProperties2(context.physicalDevice, &memProps2);
  for (int memTypeIndex = 0; memTypeIndex < VK_MAX_MEMORY_TYPES; ++memTypeIndex) {
    if (requestedMemoryAllocSize[memTypeIndex] == 0) continue;
    const VkMemoryAllocateInfo memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requestedMemoryAllocSize[memTypeIndex],
        .memoryTypeIndex = memTypeIndex,
    };
    VKM_REQUIRE(vkAllocateMemory(context.device, &memAllocInfo, VKM_ALLOC, &deviceMemory[memTypeIndex]));

    VkMemoryPropertyFlags propFlags = memProps2.memoryProperties.memoryTypes[memTypeIndex].propertyFlags;
    const bool            hasDeviceLocal = (propFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const bool            hasHostVisible = (propFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    const bool            hasHostCoherent = (propFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (hasDeviceLocal && hasHostVisible && hasHostCoherent)
      VKM_REQUIRE(vkMapMemory(context.device, deviceMemory[memTypeIndex], 0, requestedMemoryAllocSize[memTypeIndex], 0, &pMappedMemory[memTypeIndex]));
#ifdef VKM_DEBUG_MEMORY_ALLOC
    printf("Shared MemoryType: %d Allocated: %zu Mapped %d", memTypeIndex, requestedMemoryAllocSize[memTypeIndex], pMappedMemory[memTypeIndex] != NULL);
    PrintMemoryPropertyFlags(propFlags);
#endif
  }
}

// so what we need to do here is make something which allows you to pre-emptively figure out all memory requirements, store that state, then have it initialize
void vkmAllocMemory(const VkMemoryRequirements* pMemReqs, const VkMemoryPropertyFlags propFlags, const VkmLocality locality, const VkMemoryDedicatedAllocateInfoKHR* pDedicatedAllocInfo, VkDeviceMemory* pDeviceMemory) {
  VkPhysicalDeviceMemoryProperties memProps;
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);
  const uint32_t                   memTypeIndex = FindMemoryTypeIndex(memProps.memoryTypeCount, memProps.memoryTypes, pMemReqs->memoryTypeBits, propFlags);
  const VkExportMemoryAllocateInfo exportMemAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = pDedicatedAllocInfo,
      .handleTypes = VKM_EXTERNAL_HANDLE_TYPE,
  };
  const VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = locality == VKM_LOCALITY_INTERPROCESS_EXPORTED ? &exportMemAllocInfo : pDedicatedAllocInfo,
      .allocationSize = pMemReqs->size,
      .memoryTypeIndex = memTypeIndex,
  };
  VKM_REQUIRE(vkAllocateMemory(context.device, &memAllocInfo, VKM_ALLOC, pDeviceMemory));
#ifdef VKM_DEBUG_MEMORY_ALLOC
  printf("%sMemoryType: %d Allocated: %zu ", pDedicatedAllocInfo != NULL ? "Dedicated " : "",  memTypeIndex, pMemReqs->size);
  PrintMemoryPropertyFlags(propFlags);
#endif
}
static void CreateAllocBuffer(const VkMemoryPropertyFlags memPropFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const VkmLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = usage,
  };
  VKM_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, VKM_ALLOC, pBuffer));
  VkMemoryDedicatedRequirements dedicatedReqs = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
  VkMemoryRequirements2         memReqs2 = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicatedReqs};
  vkGetBufferMemoryRequirements2(context.device, &(const VkBufferMemoryRequirementsInfo2){.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = *pBuffer}, &memReqs2);
  bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
  bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VKM_DEBUG_MEMORY_ALLOC
  if (requiresDedicated) printf("Dedicated allocation is required for this buffer.\n");
  else if (prefersDedicated) printf("Dedicated allocation is preferred for this buffer.\n");
#endif
  const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .buffer = *pBuffer};
  vkmAllocMemory(&memReqs2.memoryRequirements, memPropFlags, locality, (requiresDedicated || prefersDedicated) ? &dedicatedAllocInfo : NULL, pDeviceMem);
}
void vkmCreateAllocBindBuffer(const VkMemoryPropertyFlags memPropFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const VkmLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer) {
  CreateAllocBuffer(memPropFlags, bufferSize, usage, locality, pDeviceMem, pBuffer);
  VKM_REQUIRE(vkBindBufferMemory(context.device, *pBuffer, *pDeviceMem, 0));
}
void vkmCreateAllocBindMapBuffer(const VkMemoryPropertyFlags memPropFlags, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const VkmLocality locality, VkDeviceMemory* pDeviceMem, VkBuffer* pBuffer, void** ppMapped) {
  vkmCreateAllocBindBuffer(memPropFlags, bufferSize, usage, locality, pDeviceMem, pBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pDeviceMem, 0, bufferSize, 0, ppMapped));
}

void vkmCreateBufferSharedMemory(const VkmRequestAllocationInfo* pRequest, VkBuffer* pBuffer, VkmSharedMemory* pMemory) {
  const VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = pRequest->size,
      .usage = pRequest->usage,
  };
  VKM_REQUIRE(vkCreateBuffer(context.device, &bufferCreateInfo, VKM_ALLOC, pBuffer));
  const VkBufferMemoryRequirementsInfo2 bufMemReqInfo2 = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = *pBuffer};
  VkMemoryDedicatedRequirements         dedicatedReqs = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
  VkMemoryRequirements2                 memReqs2 = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicatedReqs};
  vkGetBufferMemoryRequirements2(context.device, &bufMemReqInfo2, &memReqs2);
  bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
  bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VKM_DEBUG_MEMORY_ALLOC
  if (requiresDedicated) PANIC("Trying to allocate buffer to shared memory that requires dedicated allocation!");
  else if (prefersDedicated) printf("Warning! Trying to allocate buffer to shared memory that prefers dedicated allocation!");
#endif
  VkPhysicalDeviceMemoryProperties2 memProps2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
  vkGetPhysicalDeviceMemoryProperties2(context.physicalDevice, &memProps2);
  pMemory->type = FindMemoryTypeIndex(memProps2.memoryProperties.memoryTypeCount, memProps2.memoryProperties.memoryTypes, memReqs2.memoryRequirements.memoryTypeBits, pRequest->memoryPropertyFlags);
  pMemory->offset = requestedMemoryAllocSize[pMemory->type] + (requestedMemoryAllocSize[pMemory->type] % memReqs2.memoryRequirements.alignment);
  pMemory->size = memReqs2.memoryRequirements.size;
  requestedMemoryAllocSize[pMemory->type] += memReqs2.memoryRequirements.size;
#ifdef VKM_DEBUG_MEMORY_ALLOC
  printf("Request Shared MemoryType: %d Allocation: %zu ", pMemory->type, memReqs2.memoryRequirements.size);
  PrintMemoryPropertyFlags(pRequest->memoryPropertyFlags);
#endif
}

static void CreateStagingBuffer(const void* srcData, const VkDeviceSize bufferSize, VkDeviceMemory* pStagingMemory, VkBuffer* pStagingBuffer) {
  void* dstData;
  vkmCreateAllocBindBuffer(VKM_MEMORY_HOST_VISIBLE_COHERENT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VKM_LOCALITY_CONTEXT, pStagingMemory, pStagingBuffer);
  VKM_REQUIRE(vkMapMemory(context.device, *pStagingMemory, 0, bufferSize, 0, &dstData));
  memcpy(dstData, srcData, bufferSize);
  vkUnmapMemory(context.device, *pStagingMemory);
}
void vkmUpdateBufferViaStaging(const void* srcData, const VkDeviceSize dstOffset, const VkDeviceSize bufferSize, const VkBuffer buffer) {
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(srcData, bufferSize, &stagingBufferMemory, &stagingBuffer);
  const VkCommandBuffer commandBuffer = VkmBeginImmediateCommandBuffer();
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &(VkBufferCopy){.dstOffset = dstOffset, .size = bufferSize});
  VkmEndImmediateCommandBuffer(commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VKM_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VKM_ALLOC);
}
void vkmCreateMeshSharedMemory(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh) {
  pMesh->vertexCount = pCreateInfo->vertexCount;
  uint32_t vertexBufferSize = sizeof(VkmVertex) * pMesh->vertexCount;
  pMesh->vertexOffset = 0;
  pMesh->indexCount = pCreateInfo->indexCount;
  uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
  pMesh->indexOffset = vertexBufferSize;
  const VkmRequestAllocationInfo AllocRequest = {
      .memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .size = pMesh->indexOffset + indexBufferSize,
      .usage = VKM_BUFFER_USAGE_MESH,
      .locality = VKM_LOCALITY_CONTEXT,
      .dedicated = VKM_DEDICATED_MEMORY_FALSE,
  };
  vkmCreateBufferSharedMemory(&AllocRequest, &pMesh->buffer, &pMesh->sharedMemory);
}
void vkmBindUpdateMeshSharedMemory(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh) {
  // Ensure size is same
  const VkBufferMemoryRequirementsInfo2 bufMemReqInfo2 = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = pMesh->buffer};
  VkMemoryRequirements2                 memReqs2 = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  vkGetBufferMemoryRequirements2(context.device, &bufMemReqInfo2, &memReqs2);
  REQUIRE(pMesh->sharedMemory.size == memReqs2.memoryRequirements.size, "Trying to create mesh with a requested allocated of a different size.");
  // bind populate
  VKM_REQUIRE(vkBindBufferMemory(context.device, pMesh->buffer, deviceMemory[pMesh->sharedMemory.type], pMesh->sharedMemory.offset));
  vkmUpdateBufferViaStaging(pCreateInfo->pIndices, pMesh->indexOffset, sizeof(uint16_t) * pMesh->indexCount, pMesh->buffer);
  vkmUpdateBufferViaStaging(pCreateInfo->pVertices, pMesh->vertexOffset, sizeof(VkmVertex) * pMesh->vertexCount, pMesh->buffer);
}
void vkmCreateMesh(const VkmMeshCreateInfo* pCreateInfo, VkmMesh* pMesh) {
  pMesh->indexCount = pCreateInfo->indexCount;
  pMesh->vertexCount = pCreateInfo->vertexCount;
  uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
  uint32_t vertexBufferSize = sizeof(VkmVertex) * pMesh->vertexCount;
  pMesh->indexOffset = 0;
  pMesh->vertexOffset = indexBufferSize + (indexBufferSize % sizeof(VkmVertex));
  vkmCreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pMesh->vertexOffset + vertexBufferSize, VKM_BUFFER_USAGE_MESH, VKM_LOCALITY_CONTEXT, &pMesh->memory, &pMesh->buffer);
  vkmUpdateBufferViaStaging(pCreateInfo->pIndices, pMesh->indexOffset, indexBufferSize, pMesh->buffer);
  vkmUpdateBufferViaStaging(pCreateInfo->pVertices, pMesh->vertexOffset, vertexBufferSize, pMesh->buffer);
}

//----------------------------------------------------------------------------------
// Images
//----------------------------------------------------------------------------------

static inline void vkmCmdPipelineImageBarriers(const VkCommandBuffer commandBuffer, const uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers) {
  vkCmdPipelineBarrier2(commandBuffer, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = imageMemoryBarrierCount, .pImageMemoryBarriers = pImageMemoryBarriers});
}
static inline void vkmCmdPipelineImageBarrier(const VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* pImageMemoryBarrier) {
  vkCmdPipelineBarrier2(commandBuffer, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = pImageMemoryBarrier});
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
  const VkImageMemoryRequirementsInfo2 imgMemReqInfo2 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = *pImage,
  };
  VkMemoryDedicatedRequirements dedicatedReqs = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
  };
  VkMemoryRequirements2 memReqs2 = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
      .pNext = &dedicatedReqs,
  };
  vkGetImageMemoryRequirements2(context.device, &imgMemReqInfo2, &memReqs2);
  bool requiresDedicated = dedicatedReqs.requiresDedicatedAllocation;
  bool prefersDedicated = dedicatedReqs.prefersDedicatedAllocation;
#ifdef VKM_DEBUG_MEMORY_ALLOC
  if (requiresDedicated) printf("Dedicated allocation is required for this image.\n");
  else if (prefersDedicated) printf("Dedicated allocation is preferred for this image.\n");
#endif
  const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .image = *pImage};
  vkmAllocMemory(&memReqs2.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, locality, (requiresDedicated || prefersDedicated) ? &dedicatedAllocInfo : NULL, pMemory);
  VKM_REQUIRE(vkBindImageMemory(context.device, *pImage, *pMemory, 0));
}
static void CreateAllocateBindImageView(const VkImageCreateInfo* pImageCreateInfo, const VkImageAspectFlags aspectMask, const VkmLocality locality, VkDeviceMemory* pMemory, VkImage* pImage, VkImageView* pImageView) {
  CreateAllocBindImage(pImageCreateInfo, locality, pMemory, pImage);
  CreateImageView(pImageCreateInfo, *pImage, aspectMask, pImageView);
}

void vkmCreateTexture(const VkmTextureCreateInfo* pTextureCreateInfo, VkmTexture* pTexture) {
  CreateAllocateBindImageView(&pTextureCreateInfo->imageCreateInfo, pTextureCreateInfo->aspectMask, pTextureCreateInfo->locality, &pTexture->memory, &pTexture->image, &pTexture->view);
  switch (pTextureCreateInfo->locality) {
    default:
    case VKM_LOCALITY_CONTEXT:          break;
    case VKM_LOCALITY_PROCESS:          break;
    case VKM_LOCALITY_INTERPROCESS_EXPORTED: {
      const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .memory = pTexture->memory,
          .handleType = VKM_EXTERNAL_HANDLE_TYPE};
      VKM_INSTANCE_FUNC(vkGetMemoryWin32HandleKHR);
      VKM_REQUIRE(vkGetMemoryWin32HandleKHR(context.device, &getWin32HandleInfo, &pTexture->externalHandle));
      break;
    }
  }
  vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pTexture->image, pTextureCreateInfo->debugName);
}
void vkmCreateTextureFromFile(const char* pPath, VkmTexture* pTexture) {
  int      texChannels, width, height;
  stbi_uc* pImagePixels = stbi_load(pPath, &width, &height, &texChannels, STBI_rgb_alpha);
  REQUIRE(width > 0 && height > 0, "Image height or width is equal to zero.")
  const VkDeviceSize imageBufferSize = width * height * 4;
  VkImageCreateInfo  imageCreateInfo = VKM_DEFAULT_TEXTURE_IMAGE_CREATE_INFO;
  imageCreateInfo.extent = (VkExtent3D){width, height, 1};
  imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  CreateAllocateBindImageView(&imageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, VKM_LOCALITY_CONTEXT, &pTexture->memory, &pTexture->image, &pTexture->view);
  VkBuffer       stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateStagingBuffer(pImagePixels, imageBufferSize, &stagingBufferMemory, &stagingBuffer);
  stbi_image_free(pImagePixels);
  const VkCommandBuffer commandBuffer = VkmBeginImmediateCommandBuffer();
  vkmCmdPipelineImageBarriers(commandBuffer, 1, &VKM_COLOR_IMG_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_TRANSFER_DST, pTexture->image));
  const VkBufferImageCopy region = {
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkmCmdPipelineImageBarriers(commandBuffer, 1, &VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_TRANSFER_DST, VKM_IMG_BARRIER_TRANSFER_READ, pTexture->image));
  VkmEndImmediateCommandBuffer(commandBuffer);
  vkFreeMemory(context.device, stagingBufferMemory, VKM_ALLOC);
  vkDestroyBuffer(context.device, stagingBuffer, VKM_ALLOC);
}

void vkmCreateFramebuffer(const VkRenderPass renderPass, VkFramebuffer* pFramebuffer) {
  const VkFramebufferCreateInfo framebufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = &(const VkFramebufferAttachmentsCreateInfo){
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
          .attachmentImageInfoCount = VKM_PASS_ATTACHMENT_STD_COUNT,
          .pAttachmentImageInfos = (const VkFramebufferAttachmentImageInfo[]){
              {
                  .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                  .usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX],
                  .width = DEFAULT_WIDTH,
                  .height = DEFAULT_HEIGHT,
                  .layerCount = 1,
                  .viewFormatCount = 1,
                  .pViewFormats = &vkmPassFormats[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX]
              },
              {
                  .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                  .usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX],
                  .width = DEFAULT_WIDTH,
                  .height = DEFAULT_HEIGHT,
                  .layerCount = 1,
                  .viewFormatCount = 1,
                  .pViewFormats = &vkmPassFormats[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX]
              },
              {
                  .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                  .usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX],
                  .width = DEFAULT_WIDTH,
                  .height = DEFAULT_HEIGHT,
                  .layerCount = 1,
                  .viewFormatCount = 1,
                  .pViewFormats = &vkmPassFormats[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX]
              },
          },
      },
      .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
      .renderPass = renderPass,
      .attachmentCount = VKM_PASS_ATTACHMENT_STD_COUNT,
      //        .pAttachments = (const VkImageView[]){
      //            [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = pFrameBuffers[i].color.view,
      //            [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = pFrameBuffers[i].normal.view,
      //            [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = pFrameBuffers[i].depth.view,
      //        },
      .width = DEFAULT_WIDTH,
      .height = DEFAULT_HEIGHT,
      .layers = 1,
  };
  VKM_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VKM_ALLOC, pFramebuffer));
}

void vkmCreateStdFramebuffers(const uint32_t framebufferCount, const VkmLocality locality, VkmFramebuffer* pFrameBuffers) {
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
    case VKM_LOCALITY_INTERPROCESS_EXPORTED: textureCreateInfo.imageCreateInfo.pNext = &externalImageInfo; break;
  }
  for (int i = 0; i < framebufferCount; ++i) {
    textureCreateInfo.imageCreateInfo.extent = (VkExtent3D){DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f};

    textureCreateInfo.debugName = "CompColorFramebuffer";
    textureCreateInfo.imageCreateInfo.format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    textureCreateInfo.imageCreateInfo.usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX];
    vkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].color);

    textureCreateInfo.debugName = "CompNormalFramebuffer";
    textureCreateInfo.imageCreateInfo.format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    textureCreateInfo.imageCreateInfo.usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX];
    vkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].normal);

    textureCreateInfo.debugName = "CompDepthFramebuffer";
    textureCreateInfo.imageCreateInfo.format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.imageCreateInfo.usage = vkmPassUsages[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX];
    textureCreateInfo.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].depth);

    // I may or may not need the gbuffer in the comp but lets leave it for now... no lets get rid of this and turn it into a node construct
    textureCreateInfo.debugName = "CompGBufferFramebuffer";
    textureCreateInfo.imageCreateInfo.format = VKM_G_BUFFER_FORMAT;
    textureCreateInfo.imageCreateInfo.usage = VKM_G_BUFFER_USAGE;
    textureCreateInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkmCreateTexture(&textureCreateInfo, &pFrameBuffers[i].gBuffer);

//    const VkFramebufferCreateInfo framebufferCreateInfo = {
//        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
//        .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
//        .renderPass = context.renderPass,
////        .attachmentCount = VKM_PASS_ATTACHMENT_STD_COUNT,
////        .pAttachments = (const VkImageView[]){
////            [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = pFrameBuffers[i].color.view,
////            [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = pFrameBuffers[i].normal.view,
////            [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = pFrameBuffers[i].depth.view,
////        },
//        .width = DEFAULT_WIDTH,
//        .height = DEFAULT_HEIGHT,
//        .layers = 1,
//    };
//    VKM_REQUIRE(vkCreateFramebuffer(context.device, &framebufferCreateInfo, VKM_ALLOC, &pFrameBuffers[i].framebuffer));
//    VKM_REQUIRE(vkCreateSemaphore(context.device, &(VkSemaphoreCreateInfo){.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}, VKM_ALLOC, &pFrameBuffers[i].renderCompleteSemaphore));
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
        //                "VK_LAYER_KHRONOS_validation",
        //        "VK_LAYER_LUNARG_monitor"
    };
    const char* ppEnabledInstanceExtensionNames[] = {
        //        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        //        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        //        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
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
        .enabledLayerCount = _countof(ppEnabledLayerNames),
        .ppEnabledLayerNames = ppEnabledLayerNames,
        .enabledExtensionCount = _countof(ppEnabledInstanceExtensionNames),
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
    //        VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = {};
    //        // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    //        // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    //        messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    //        messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //        VkDebugUtilsMessageTypeFlagsEXT messageType = {};
    //        // messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    //        messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    //        messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    //        const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
    //            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    //            .messageSeverity = messageSeverity,
    //            .messageType = messageType,
    //            .pfnUserCallback = VkmDebugUtilsCallback,
    //        };
    //        VKM_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
    //        VKM_REQUIRE(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, VKM_ALLOC, &debugUtilsMessenger));
  }
}

void vkmCreateContext(const VkmContextCreateInfo* pContextCreateInfo) {
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

    printf("minUniformBufferOffsetAlignment: %llu\n", physicalDeviceProperties.properties.limits.minUniformBufferOffsetAlignment);
    printf("minStorageBufferOffsetAlignment: %llu\n", physicalDeviceProperties.properties.limits.minStorageBufferOffsetAlignment);
    REQUIRE(physicalDeviceProperties.properties.apiVersion >= VKM_VERSION, "Insufficient Vulkan API Version");
  }

  {  // Device
    const VkPhysicalDevicePipelineRobustnessFeaturesEXT physicalDevicePipelineRobustnessFeaturesEXT = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT,
        .pipelineRobustness = VK_TRUE,
    };
    const VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT physicalDevicePageableDeviceLocalMemoryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,
        .pNext = (void*)&physicalDevicePipelineRobustnessFeaturesEXT,
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
        .imagelessFramebuffer = VK_TRUE,
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
            .tessellationShader = VK_TRUE,
            .robustBufferAccess = VK_TRUE,
#ifdef VKM_DEBUG_WIREFRAME
            .fillModeNonSolid = VK_TRUE,
#endif
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
        .enabledExtensionCount = _countof(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    VKM_REQUIRE(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VKM_ALLOC, &context.device));
  }

  for (int i = 0; i < VKM_QUEUE_FAMILY_TYPE_COUNT; ++i) {
    if (pContextCreateInfo->queueFamilyCreateInfos[i].queueCount == 0)
      continue;

    vkGetDeviceQueue(context.device, context.queueFamilies[i].index, 0, &context.queueFamilies[i].queue);
    vkmSetDebugName(VK_OBJECT_TYPE_QUEUE, (uint64_t)context.queueFamilies[i].queue, string_QueueFamilyType[i]);
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queueFamilies[i].index,
    };
    VKM_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &context.queueFamilies[i].pool));
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
  }
}

void vkmCreateStdRenderPass() {
  const VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .attachmentCount = 3,
      .pAttachments = (const VkAttachmentDescription2[]){
          [VKM_PASS_ATTACHMENT_STD_COLOR_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_COLOR_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          },
          [VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_NORMAL_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
          },
          [VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX] = {
              .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
              .format = vkmPassFormats[VKM_PASS_ATTACHMENT_STD_DEPTH_INDEX],
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
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
              .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
              .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .dependencyFlags = 0,
          },
          {
              .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
              .srcSubpass = 0,
              .dstSubpass = VK_SUBPASS_EXTERNAL,
              .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
              .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
              .dependencyFlags = 0,
          },
      },
  };
  VKM_REQUIRE(vkCreateRenderPass2(context.device, &renderPassCreateInfo2, VKM_ALLOC, &context.renderPass));
  vkmSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)context.renderPass, "ContextRenderPass");
}

void VkmCreateSampler(const VkmSamplerCreateInfo* pDesc, VkSampler* pSampler) {
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

void vkmCreateSwap(const VkSurfaceKHR surface, const VkmQueueFamilyType presentQueueFamily, VkmSwap* pSwap) {
  VkBool32 presentSupport = VK_FALSE;
  VKM_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.queueFamilies[presentQueueFamily].index, surface, &presentSupport));
  REQUIRE(presentSupport, "Queue can't present to surface!")
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
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) vkmSetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)pSwap->images[i], "SwapImage");
}

void vkmCreateStdPipeLayout() {
  CreateGlobalSetLayout();
  CreateStdMaterialSetLayout();
  CreateStdObjectSetLayout();
  CreateStdPipeLayout();
}

void vkmCreateTimeline(VkSemaphore* pSemaphore) {
  const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
  const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineSemaphoreTypeCreateInfo,
  };
  VKM_REQUIRE(vkCreateSemaphore(context.device, &timelineSemaphoreCreateInfo, VKM_ALLOC, pSemaphore));
}

void vkmCreateGlobalSet(VkmGlobalSet* pSet) {
  vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipeLayout.globalSetLayout, &pSet->set);
  vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pSet->memory, &pSet->buffer, (void**)&pSet->pMapped);
  vkUpdateDescriptorSets(context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(pSet->set, pSet->buffer), 0, NULL);
}

#ifdef WIN32
void midVkCreateVulkanSurface(HINSTANCE hInstance, HWND hWnd, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
  const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = hInstance,
      .hwnd = hWnd,
  };
  VKM_INSTANCE_FUNC(vkCreateWin32SurfaceKHR);
  VKM_REQUIRE(vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, pAllocator, pSurface));
}
#endif
