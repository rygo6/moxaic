#pragma once

#include "mid_vulkan.h"

typedef struct ProcessState{
	float depthNearZ;
	float depthFarZ;
	float cameraNearZ;
	float cameraFarZ;
}ProcessState;

enum {
	SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
	SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER,
	SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER,
	SET_BIND_INDEX_GBUFFER_PROCESS_COUNT
};

#define BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(_view)                  \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,      \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = _view,                                      \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}

#define BIND_WRITE_GBUFFER_PROCESS_SRC_GBUFFER(_view)                \
	(VkWriteDescriptorSet) {                                         \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                      \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER,    \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                      \
			.imageView = _view,                                      \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}

#define BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(_view)             \
	(VkWriteDescriptorSet) {                                      \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                   \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER, \
		.descriptorCount = 1,                                     \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       \
		.pImageInfo = &(VkDescriptorImageInfo){                   \
			.imageView = _view,                                   \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,               \
		},                                                        \
	}

static void CreateGBufferProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &(VkDescriptorSetLayoutCreateInfo){
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_INDEX_GBUFFER_PROCESS_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH] = {
				.binding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER] = {
				.binding = SET_BIND_INDEX_GBUFFER_PROCESS_SRC_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER] = {
				.binding = SET_BIND_INDEX_GBUFFER_PROCESS_DST_GBUFFER,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	}, VK_ALLOC, pLayout));
}

enum {
	PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
	PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
};

static void CreateGBufferProcessPipeLayout(VkDescriptorSetLayout layout, VkPipelineLayout* pPipeLayout)
{
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &(VkPipelineLayoutCreateInfo){
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT] = layout,
		},
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange){
			.offset = 0,
			.size = sizeof(ProcessState),
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		},
	}, VK_ALLOC, pPipeLayout));
}
