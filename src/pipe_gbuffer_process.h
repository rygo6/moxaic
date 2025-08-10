#pragma once

#include "mid_vulkan.h"

enum {
	SET_BIND_INDEX_GBUFFER_PROCESS_STATE,
	SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
	SET_BIND_INDEX_GBUFFER_PROCESS_DST,
	SET_BIND_INDEX_GBUFFER_PROCESS_COUNT
};

static void CreateGBufferProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_INDEX_GBUFFER_PROCESS_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_STATE,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_SRC_DEPTH,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.nearestSampler,
			},
			{
				SET_BIND_INDEX_GBUFFER_PROCESS_DST,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &createInfo, VK_ALLOC, pLayout));
}

#define BIND_WRITE_GBUFFER_PROCESS_STATE(_view)              \
	(VkWriteDescriptorSet) {                                 \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_STATE,  \
		.descriptorCount = 1,                                \
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
		.pBufferInfo = &(VkDescriptorBufferInfo){            \
			.buffer = _view,                                 \
			.range = sizeof(MxcProcessState),                \
		},                                                   \
	}
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
#define BIND_WRITE_GBUFFER_PROCESS_DST(_view)               \
	(VkWriteDescriptorSet) {                                \
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstBinding = SET_BIND_INDEX_GBUFFER_PROCESS_DST,   \
		.descriptorCount = 1,                               \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
		.pImageInfo = (VkDescriptorImageInfo[]){            \
			{                                               \
				.imageView = _view,                         \
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,     \
			},                                              \
		},                                                  \
	}

enum {
	PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
	PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
};

static void CreateGBufferProcessPipeLayout(VkDescriptorSetLayout layout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = PIPE_SET_INDEX_GBUFFER_PROCESS_COUNT,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT] = layout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}
