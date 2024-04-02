#include "renderer.h"
#include "globals.h"
#include "window.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

//----------------------------------------------------------------------------------
// Global Constants
//----------------------------------------------------------------------------------

#define VK_VERSION VK_MAKE_API_VERSION(0, 1, 3, 2)
#define VK_ALLOC   NULL

#define CHECK_RESULT(command)                                              \
  {                                                                        \
    VkResult result = command;                                             \
    if (__builtin_expect(result != VK_SUCCESS, 1)) {                       \
      printf("!!! Error! %s = %s\n\n", #command, string_VkResult(result)); \
      return result;                                                       \
    }                                                                      \
  }

#define PFN_LOAD(vkFunction)                                                                            \
  PFN_##vkFunction vkFunction = (PFN_##vkFunction)vkGetInstanceProcAddr(context.instance, #vkFunction); \
  assert(vkFunction != NULL && "Couldn't load " #vkFunction)

#define COUNT(array) sizeof(array) / sizeof(array[0])

#define COLOR_BUFFER_FORMAT     VK_FORMAT_R8G8B8A8_UNORM
#define NORMAL_BUFFER_FORMAT    VK_FORMAT_R16G16B16A16_SFLOAT
#define DEPTH_BUFFER_FORMAT     VK_FORMAT_D32_SFLOAT
#define COLOR_ATTACHMENT_INDEX  0
#define NORMAL_ATTACHMENT_INDEX 1
#define DEPTH_ATTACHMENT_INDEX  2

#define SWAP_COUNT 2

//----------------------------------------------------------------------------------
// Math
//----------------------------------------------------------------------------------

#define MATH_ALIGN __attribute((aligned(16)))

typedef MATH_ALIGN float    vec2[2];
typedef MATH_ALIGN float    vec3[3];
typedef MATH_ALIGN float    vec4[4];
typedef MATH_ALIGN vec4     mat4[4];
typedef MATH_ALIGN uint32_t ivec2[2];

typedef struct Vertex {
  vec3 position;
  vec3 normal;
  vec2 uv;
} Vertex;
enum VertexAttributes {
  VERTEX_ATTR_POSITION,
  VERTEX_ATTR_NORMAL,
  VERTEX_ATTR_UV,
  VERTEX_ATTR_COUNT
};

//----------------------------------------------------------------------------------
// Global State
//----------------------------------------------------------------------------------

typedef struct Camera {
  vec3 position;
  vec4 rotation;
} Camera;

typedef struct GlobalSetState {
  mat4  view;
  mat4  proj;
  mat4  viewProj;
  mat4  invView;
  mat4  invProj;
  mat4  invViewProj;
  ivec2 screenSize;
} GlobalSetState;

typedef struct StandardObjectSetState {
  mat4 model;
} StandardObjectState;

static struct Context {
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VkPhysicalDevice physicalDevice;

  VkDevice device;

  VkSampler nearestSampler;
  VkSampler linearSampler;
  VkSampler minSampler;
  VkSampler maxSampler;

  VkRenderPass renderPass;

  GlobalSetState        globalSetMapped;
  VkDeviceMemory        globalSetMemory;
  VkBuffer              globalSetBuffer;
  VkDescriptorSetLayout globalSetLayout;
  VkDescriptorSet       globalSet;

  VkDescriptorSetLayout materialSetLayout;
  VkDescriptorSetLayout objectSetLayout;

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkSwapchainKHR swapchain;
  VkImage        swapImages[SWAP_COUNT];
  VkImageView    swapImageViews[SWAP_COUNT];

  VkCommandPool   graphicsCommandPool;
  VkCommandBuffer graphicsCommandBuffer;
  uint32_t        graphicsQueueFamilyIndex;
  VkQueue         graphicsQueue;

  VkCommandPool   computeCommandPool;
  VkCommandBuffer computeCommandBuffer;
  uint32_t        computeQueueFamilyIndex;
  VkQueue         computeQueue;

  uint32_t transferQueueFamilyIndex;
  VkQueue  transferQueue;

  VkDescriptorPool descriptorPool;

  VkQueryPool timeQueryPool;

} context;


//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

static int ReadFile(const char* pPath, int* pFileLength, char** ppFileContents) {
  FILE* file = fopen(pPath, "rb");
  if (file == NULL) {
    printf("File can't be opened! %s\n", pPath);
    return 1;
  }
  fseek(file, 0, SEEK_END);
  *pFileLength = ftell(file);
  rewind(file);
  *ppFileContents = malloc(*pFileLength * sizeof(char));
  size_t readCount = fread(*ppFileContents, *pFileLength, 1, file);
  if (readCount == 0) {
    printf("Failed to read file! %s\n", pPath);
    return 1;
  }
  fclose(file);
}

//----------------------------------------------------------------------------------
// Descriptors
//----------------------------------------------------------------------------------

VkResult CreateGlobalSetLayout() {
  VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(VkDescriptorSetLayoutBinding){
          .binding = 0,
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
  return vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.globalSetLayout);
}
#define SET_WRITE_GLOBAL                                 \
  {                                                      \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
    .dstSet = context.globalSet,                         \
    .dstBinding = 0,                                     \
    .descriptorCount = 1,                                \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(VkDescriptorBufferInfo){            \
        .buffer = context.globalSetBuffer,               \
        .range = sizeof(GlobalSetState),                 \
    },                                                   \
  }

VkResult CreateStandardMaterialSetLayout() {
  VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(VkDescriptorSetLayoutBinding){
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  return vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.materialSetLayout);
}
#define SET_WRITE_STANDARD_MATERIAL(materialSet, imageView)      \
  {                                                              \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = materialSet,                                       \
    .dstBinding = 0,                                             \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(VkDescriptorImageInfo){                      \
        .sampler = context.linearSampler,                        \
        .imageView = imageView,                                  \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }

VkResult CreateStandardObjectSetLayout() {
  VkDescriptorSetLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &(VkDescriptorSetLayoutBinding){
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };
  return vkCreateDescriptorSetLayout(context.device, &createInfo, VK_ALLOC, &context.objectSetLayout);
}
#define SET_WRITE_STANDARD_OBJECT(objectSet, buffer)     \
  {                                                      \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
    .dstSet = objectSet,                                 \
    .dstBinding = 0,                                     \
    .descriptorCount = 1,                                \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(VkDescriptorBufferInfo){            \
        .buffer = buffer,                                \
        .range = sizeof(StandardObjectState),            \
    },                                                   \
  }

//----------------------------------------------------------------------------------
// Pipelines
//----------------------------------------------------------------------------------


#define COLOR_WRITE_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

VkResult CreateShaderModule(const char* pShaderPath, VkShaderModule* pShaderModule) {
  VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  if (ReadFile(pShaderPath, &createInfo.codeSize, &createInfo.pCode)) {
    free(createInfo.pCode);
    return VK_INCOMPLETE;
  }
  VkResult result = vkCreateShaderModule(context.device, &createInfo, VK_ALLOC, pShaderModule);
  free(createInfo.pCode);
  return result;
}

#define STANDARD_VERTEX_BINDING 0
enum StandardSetBinding {
  STANDARD_SET_BINDING_GLOBAL,
  STANDARD_SET_BINDING_STANDARD_MATERIAL,
  STANDARD_SET_BINDING_STANDARD_OBJECT,
  STANDARD_SET_BINDING_STANDARD_COUNT,
};
VkResult CreateStandardPipelineLayout() {
  VkDescriptorSetLayout pSetLayouts[STANDARD_SET_BINDING_STANDARD_COUNT];
  pSetLayouts[STANDARD_SET_BINDING_GLOBAL] = context.globalSetLayout;
  pSetLayouts[STANDARD_SET_BINDING_STANDARD_MATERIAL] = context.materialSetLayout;
  pSetLayouts[STANDARD_SET_BINDING_STANDARD_OBJECT] = context.objectSetLayout;
  VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = STANDARD_SET_BINDING_STANDARD_COUNT,
      .pSetLayouts = pSetLayouts,
  };
  return vkCreatePipelineLayout(context.device, &createInfo, VK_ALLOC, &context.standardPipelineLayout);
}

VkResult CreateStandardPipeline() {
  VkShaderModule vertShader;
  CHECK_RESULT(CreateShaderModule("./shaders/basic_material.vert.spv", &vertShader));
  VkShaderModule fragShader;
  CHECK_RESULT(CreateShaderModule("./shaders/basic_material.frag.spv", &fragShader));

  VkGraphicsPipelineCreateInfo pipelineInfo = {
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
                  .binding = STANDARD_VERTEX_BINDING,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
          .vertexAttributeDescriptionCount = 3,
          .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
              {
                  .location = VERTEX_ATTR_POSITION,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, position),
              },
              {
                  .location = VERTEX_ATTR_NORMAL,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, normal),
              },
              {
                  .location = VERTEX_ATTR_UV,
                  .binding = STANDARD_VERTEX_BINDING,
                  .format = VK_FORMAT_R32G32_SFLOAT,
                  .offset = offsetof(Vertex, uv),
              },
          },
      },
      .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE,
      },
      .pViewportState = &(VkPipelineViewportStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          .viewportCount = 1,
          .scissorCount = 1,
      },
      .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_POLYGON_MODE_FILL,
          .frontFace = VK_FRONT_FACE_CLOCKWISE,
          .lineWidth = 1.0f,
      },
      .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      },
      .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
          .maxDepthBounds = 1.0f,
      },
      .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOp = VK_LOGIC_OP_COPY,
          .attachmentCount = 2,
          .pAttachments = (VkPipelineColorBlendAttachmentState[]){
              {/* Color */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
              {/* Normal */ .colorWriteMask = COLOR_WRITE_MASK_RGBA},
          },
          .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
      },
      .pDynamicState = &(VkPipelineDynamicStateCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = 2,
          .pDynamicStates = (VkDynamicState[]){
              VK_DYNAMIC_STATE_VIEWPORT,
              VK_DYNAMIC_STATE_SCISSOR,
          },
      },
      .layout = context.standardPipelineLayout,
      .renderPass = context.renderPass,
  };
  VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, &context.standardPipeline);
  vkDestroyShaderModule(context.device, vertShader, VK_ALLOC);
  vkDestroyShaderModule(context.device, fragShader, VK_ALLOC);
  return result;
}

//----------------------------------------------------------------------------------
// Image Barriers
//----------------------------------------------------------------------------------
static VkResult BeginImmediateCommandBuffer(
    VkCommandPool    commandPool,
    VkCommandBuffer* pCommandBuffer) {
  VkCommandBufferAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  CHECK_RESULT(vkAllocateCommandBuffers(context.device, &allocInfo, pCommandBuffer));
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  CHECK_RESULT(vkBeginCommandBuffer(*pCommandBuffer, &beginInfo));
}

static VkResult EndImmediateCommandBuffer(
    VkCommandPool   commandPool,
    VkQueue         graphicsQueue,
    VkCommandBuffer commandBuffer) {
  CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  CHECK_RESULT(vkQueueWaitIdle(graphicsQueue));
  vkFreeCommandBuffers(context.device, commandPool, 1, &commandBuffer);
}

typedef enum VkQueueBarrier {
  QUEUE_BARRIER_IGNORE,
  QUEUE_BARRIER_GRAPHICS,
  QUEUE_BARRIER_COMPUTE,
  QUEUE_BARRIER_TRANSITION,
  QUEUE_BARRIER_FAMILY_EXTERNAL,
} VkQueueBarrier;
typedef struct VkImageBarrier {
  VkAccessFlagBits2        accessMask;
  VkImageLayout            layout;
  VkQueueBarrier           queueFamily;
  VkPipelineStageFlagBits2 stageMask;
} VkImageBarrier;
const VkImageBarrier UndefinedImageBarrier = {
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
    .queueFamily = QUEUE_BARRIER_IGNORE,
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
};
const VkImageBarrier PresentImageBarrier = {
    .accessMask = VK_ACCESS_2_NONE,
    .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .queueFamily = QUEUE_BARRIER_COMPUTE,
    .stageMask = VK_PIPELINE_STAGE_2_NONE,
};

static VkResult TransitionImageLayoutImmediate(
    const VkImageBarrier* pSrc,
    const VkImageBarrier* pDst,
    VkImageAspectFlags    aspectMask,
    VkImage               image) {
  VkCommandBuffer commandBuffer;
  CHECK_RESULT(BeginImmediateCommandBuffer(context.graphicsCommandPool, &commandBuffer));
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = pSrc->stageMask,
      .srcAccessMask = pSrc->accessMask,
      .dstStageMask = pDst->stageMask,
      .dstAccessMask = pDst->accessMask,
      .oldLayout = pSrc->layout,
      .newLayout = pDst->layout,
      .srcQueueFamilyIndex = pSrc->queueFamily,
      .dstQueueFamilyIndex = pSrc->queueFamily,
      .image = image,
      .subresourceRange = {
          .aspectMask = aspectMask,
          .levelCount = 1,
          .layerCount = 1,
      },
  };
  VkDependencyInfo dependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  CHECK_RESULT(EndImmediateCommandBuffer(context.graphicsCommandPool, context.graphicsQueue, commandBuffer));
}

//----------------------------------------------------------------------------------
// Context
//----------------------------------------------------------------------------------
static VkBool32 DebugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                        pUserData) {
  printf("%s\n", pCallbackData->pMessage);
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    assert(0 && "Validation Error!");
  }
  return VK_FALSE;
}

static VkResult FindMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties* pPhysicalDeviceMemoryProperties,
    const VkMemoryRequirements*             pMemoryRequirements,
    VkMemoryPropertyFlags                   memoryPropertyFlags,
    uint32_t*                               pMemoryTypeIndex) {
  VkResult result = VK_INCOMPLETE;
  for (uint32_t i = 0; i < pPhysicalDeviceMemoryProperties->memoryTypeCount; i++) {
    bool hasTypeBits = pMemoryRequirements->memoryTypeBits & 1 << i;
    bool hasPropertyFlags = (pPhysicalDeviceMemoryProperties->memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;
    if (hasTypeBits && hasPropertyFlags) {
      *pMemoryTypeIndex = i;
      result = VK_SUCCESS;
      break;
    }
  }

  if (result != VK_SUCCESS) {
    printf("Failed to find memory with properties: ");
    int index = 0;
    while (memoryPropertyFlags) {
      if (memoryPropertyFlags & 1) {
        printf(" %s ", string_VkMemoryPropertyFlagBits(1U << index));
      }
      ++index;
      memoryPropertyFlags >>= 1;
    }
  }

  return result;
}

static VkResult AllocateBufferMemory(
    VkBuffer              buffer,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceMemory*       pDeviceMemory) {
  uint32_t                         memoryTypeIndex;
  VkMemoryRequirements             memoryRequirements;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetBufferMemoryRequirements(context.device, buffer, &memoryRequirements);
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memoryProperties);
  CHECK_RESULT(FindMemoryTypeIndex(&memoryProperties, &memoryRequirements, memoryPropertyFlags, &memoryTypeIndex));

  VkMemoryAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memoryRequirements.size,
      .memoryTypeIndex = memoryTypeIndex,
  };
  CHECK_RESULT(vkAllocateMemory(context.device, &allocateInfo, VK_ALLOC, pDeviceMemory));
}

typedef enum VkSupport {
  SUPPORT_OPTIONAL,
  SUPPORT_YES,
  SUPPORT_NO,
} VkSupport;
const char* string_Support(VkSupport support) {
  switch (support) {
  case SUPPORT_OPTIONAL: return "Optional";
  case SUPPORT_YES: return "Yes";
  case SUPPORT_NO: return "No";
  default: assert(0 && "Error! string_Support does not support passed value!");
  }
}
typedef struct VkQueueDesc {
  VkSupport    graphics;
  VkSupport    compute;
  VkSupport    transfer;
  VkSupport    globalPriority;
  VkSurfaceKHR presentSurface;
} QueueDesc;
static VkResult FindQueueIndex(VkPhysicalDevice physicalDevice, const QueueDesc* pQueueDesc, uint32_t* pQueueIndex) {
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
    if (pQueueDesc->presentSurface != NULL)
      vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, pQueueDesc->presentSurface, &presentSupport);

    if (pQueueDesc->graphics == SUPPORT_YES && !graphicsSupport) continue;
    if (pQueueDesc->graphics == SUPPORT_NO && graphicsSupport) continue;
    if (pQueueDesc->compute == SUPPORT_YES && !computeSupport) continue;
    if (pQueueDesc->compute == SUPPORT_NO && computeSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_YES && !transferSupport) continue;
    if (pQueueDesc->transfer == SUPPORT_NO && transferSupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_YES && !globalPrioritySupport) continue;
    if (pQueueDesc->globalPriority == SUPPORT_NO && globalPrioritySupport) continue;
    if (pQueueDesc->presentSurface != NULL && !presentSupport) continue;

    *pQueueIndex = i;
    return VK_SUCCESS;
  }

  printf("Can't find queue family! graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
         string_Support(pQueueDesc->graphics),
         string_Support(pQueueDesc->compute),
         string_Support(pQueueDesc->transfer),
         string_Support(pQueueDesc->globalPriority),
         pQueueDesc->presentSurface == NULL ? "No" : "Yes");
  return VK_ERROR_FEATURE_NOT_PRESENT;
}

int vkInitContext() {
  {  // Instance
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity = 0;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    VkDebugUtilsMessageTypeFlagsEXT messageType = 0;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = DebugCallback,
    };
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
    VkInstanceCreateInfo instanceCreationInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugUtilsMessengerCreateInfo,
        .pApplicationInfo = &(VkApplicationInfo){
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
    CHECK_RESULT(vkCreateInstance(&instanceCreationInfo, VK_ALLOC, &context.instance));
    printf("Instance Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MAJOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_MINOR(instanceCreationInfo.pApplicationInfo->apiVersion),
           VK_API_VERSION_PATCH(instanceCreationInfo.pApplicationInfo->apiVersion));
  }

  {  // PhysicalDevice
    uint32_t deviceCount = 0;
    CHECK_RESULT(vkEnumeratePhysicalDevices(context.instance, &deviceCount, NULL));
    VkPhysicalDevice devices[deviceCount];
    CHECK_RESULT(vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices));
    context.physicalDevice = devices[0];  // We are just assuming the best GPU is first. So far this seems to be true.
    VkPhysicalDeviceProperties2 physicalDeviceProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &physicalDeviceProperties);
    printf("PhysicalDevice: %s\n", physicalDeviceProperties.properties.deviceName);
    printf("PhysicalDevice Vulkan API version: %d.%d.%d.%d\n",
           VK_API_VERSION_VARIANT(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion),
           VK_API_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion));
    assert(physicalDeviceProperties.properties.apiVersion >= VK_VERSION && "Insufficinet Vulkan API Version");
  }

  {  // Surface
    CHECK_RESULT(mxCreateSurface(context.instance, VK_ALLOC, &context.surface));
  }

  {  // QueueIndices
    QueueDesc graphicsQueueDesc = {
        .graphics = SUPPORT_YES,
        .compute = SUPPORT_YES,
        .transfer = SUPPORT_YES,
        .globalPriority = SUPPORT_YES,
        .presentSurface = context.surface};
    CHECK_RESULT(FindQueueIndex(context.physicalDevice, &graphicsQueueDesc, &context.graphicsQueueFamilyIndex));
    QueueDesc computeQueueDesc = {
        .graphics = SUPPORT_NO,
        .compute = SUPPORT_YES,
        .transfer = SUPPORT_YES,
        .globalPriority = SUPPORT_YES,
        .presentSurface = context.surface};
    CHECK_RESULT(FindQueueIndex(context.physicalDevice, &computeQueueDesc, &context.computeQueueFamilyIndex));
    QueueDesc transferQueueDesc = {
        .graphics = SUPPORT_NO,
        .compute = SUPPORT_NO,
        .transfer = SUPPORT_YES};
    CHECK_RESULT(FindQueueIndex(context.physicalDevice, &transferQueueDesc, &context.transferQueueFamilyIndex));
  }

  {  // Device
    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT,
        .pNext = NULL,
        .globalPriorityQuery = VK_TRUE,
    };
    VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = &physicalDeviceGlobalPriorityQueryFeatures,
        .robustBufferAccess2 = VK_TRUE,
        .robustImageAccess2 = VK_TRUE,
    };
    VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = &physicalDeviceRobustness2Features,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &physicalDeviceMeshShaderFeatures,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &physicalDeviceVulkan13Features,
        .samplerFilterMinmax = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
    };
    VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &physicalDeviceVulkan12Features,
    };
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &physicalDeviceVulkan11Features,
        .features = {
            .robustBufferAccess = VK_TRUE,
        }};
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
    VkDeviceQueueGlobalPriorityCreateInfoEXT deviceQueueGlobalPriorityCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
        .globalPriority = IsCompositor ? VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT : VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT,
    };
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures,
        .queueCreateInfoCount = 3,
        .pQueueCreateInfos = (VkDeviceQueueCreateInfo[]){
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.graphicsQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){IsCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = &deviceQueueGlobalPriorityCreateInfo,
                .queueFamilyIndex = context.computeQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){IsCompositor ? 1.0f : 0.0f},
            },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = context.transferQueueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &(float){0},
            },
        },
        .enabledExtensionCount = COUNT(ppEnabledDeviceExtensionNames),
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
    };
    CHECK_RESULT(vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VK_ALLOC, &context.device));
  }

  {  // Queues
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    assert(context.graphicsQueue != NULL && "graphicsQueue not found!");
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    assert(context.computeQueue != NULL && "computeQueue not found!");
    vkGetDeviceQueue(context.device, context.transferQueueFamilyIndex, 0, &context.transferQueue);
    assert(context.transferQueue != NULL && "transferQueue not found!");
  }

  // PFN_LOAD(vkSetDebugUtilsObjectNameEXT);

  {  // RenderPass
#define VK_DEFAULT_ATTACHMENT_DESCRIPTION             \
  .samples = VK_SAMPLE_COUNT_1_BIT,                   \
  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              \
  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,            \
  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   \
  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, \
  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         \
  .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 3,
        .pAttachments = (VkAttachmentDescription[]){
            {.format = COLOR_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = NORMAL_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
            {.format = DEPTH_BUFFER_FORMAT, VK_DEFAULT_ATTACHMENT_DESCRIPTION},
        },
        .subpassCount = 1,
        .pSubpasses = &(VkSubpassDescription){
            .colorAttachmentCount = 2,
            .pColorAttachments = (VkAttachmentReference[]){
                {.attachment = COLOR_ATTACHMENT_INDEX, .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                {.attachment = NORMAL_ATTACHMENT_INDEX, .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
            },
            .pDepthStencilAttachment = &(VkAttachmentReference){
                .attachment = DEPTH_ATTACHMENT_INDEX,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            },
        },
    };
    CHECK_RESULT(vkCreateRenderPass(context.device, &renderPassCreateInfo, VK_ALLOC, &context.renderPass));

#undef VK_DEFAULT_ATTACHMENT_DESCRIPTION
  }

  {  // Pools
    VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex,
    };
    CHECK_RESULT(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &context.graphicsCommandPool));
    VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context.computeQueueFamilyIndex,
    };
    CHECK_RESULT(vkCreateCommandPool(context.device, &computeCommandPoolCreateInfo, VK_ALLOC, &context.computeCommandPool));

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 30,
        .poolSizeCount = 3,
        .pPoolSizes = (VkDescriptorPoolSize[]){
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
        },
    };
    CHECK_RESULT(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, VK_ALLOC, &context.descriptorPool));

    VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    CHECK_RESULT(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VK_ALLOC, &context.timeQueryPool));
  }

  {  // CommandBuffers
    VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.graphicsCommandPool,
        .commandBufferCount = 1,
    };
    CHECK_RESULT(vkAllocateCommandBuffers(context.device, &graphicsCommandBufferAllocateInfo, &context.graphicsCommandBuffer));
    VkCommandBufferAllocateInfo computeCommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.computeCommandPool,
        .commandBufferCount = 1,
    };
    CHECK_RESULT(vkAllocateCommandBuffers(context.device, &computeCommandBufferAllocateInfo, &context.computeCommandBuffer));
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

    VkSamplerCreateInfo nearestSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .maxLod = 16.0,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };
    CHECK_RESULT(vkCreateSampler(context.device, &nearestSampleCreateInfo, VK_ALLOC, &context.nearestSampler));

    VkSamplerCreateInfo linearSampleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        DEFAULT_LINEAR_SAMPLER,
    };
    CHECK_RESULT(vkCreateSampler(context.device, &linearSampleCreateInfo, VK_ALLOC, &context.linearSampler));

    VkSamplerCreateInfo minSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    CHECK_RESULT(vkCreateSampler(context.device, &minSamplerCreateInfo, VK_ALLOC, &context.minSampler));

    VkSamplerCreateInfo maxSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = &(VkSamplerReductionModeCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
        },
        DEFAULT_LINEAR_SAMPLER,
    };
    CHECK_RESULT(vkCreateSampler(context.device, &maxSamplerCreateInfo, VK_ALLOC, &context.minSampler));

#undef DEFAULT_LINEAR_SAMPLER
  }

  {  // Swapchain
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.surface,
        .minImageCount = SWAP_COUNT,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageExtent = {DEFAULT_WIDTH, DEFAULT_HEIGHT},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    CHECK_RESULT(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, VK_ALLOC, &context.swapchain));
    uint32_t swapCount;
    CHECK_RESULT(vkGetSwapchainImagesKHR(context.device, context.swapchain, &swapCount, NULL));
    assert(swapCount == SWAP_COUNT && "Resulting swap image count does not match requested swap count!");
    CHECK_RESULT(vkGetSwapchainImagesKHR(context.device, context.swapchain, &swapCount, context.swapImages));

    for (int i = 0; i < SWAP_COUNT; ++i) {
      VkImageViewCreateInfo swapImageViewCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = context.swapImages[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_B8G8R8A8_SRGB,
          .subresourceRange = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .levelCount = 1,
              .layerCount = 1,
          },
      };
      CHECK_RESULT(vkCreateImageView(context.device, &swapImageViewCreateInfo, VK_ALLOC, &context.swapImageViews[i]));
      CHECK_RESULT(TransitionImageLayoutImmediate(&UndefinedImageBarrier, &PresentImageBarrier, VK_IMAGE_ASPECT_COLOR_BIT, context.swapImages[i]));
    }
  }

  {  // Global Descriptor
    CHECK_RESULT(CreateGlobalSetLayout());

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.globalSetLayout,
    };
    CHECK_RESULT(vkAllocateDescriptorSets(context.device, &allocInfo, &context.globalSet));

    const VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(GlobalSetState),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkMemoryPropertyFlags memoryPropertyFlags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    CHECK_RESULT(vkCreateBuffer(context.device, &bufferCreateInfo, NULL, &context.globalSetBuffer));
    CHECK_RESULT(AllocateBufferMemory(context.globalSetBuffer, memoryPropertyFlags, &context.globalSetMemory));
    CHECK_RESULT(vkBindBufferMemory(context.device, context.globalSetBuffer, context.globalSetMemory, 0));
    CHECK_RESULT(vkMapMemory(context.device, context.globalSetMemory, 0, sizeof(GlobalSetState), 0, (void**)&context.globalSetMapped));

    VkWriteDescriptorSet writeSet = SET_WRITE_GLOBAL;
    vkUpdateDescriptorSets(context.device, 1, &writeSet, 0, NULL);
  }

  {  // Standard Pipeline
    CHECK_RESULT(CreateStandardMaterialSetLayout());
    CHECK_RESULT(CreateStandardObjectSetLayout());

    CHECK_RESULT(CreateStandardPipelineLayout());
    CHECK_RESULT(CreateStandardPipeline());
  }
}

int vkInitStandardPipeline() {
}

int mxRenderInitNode() {
}