#pragma once
#include "Prerequisites.h"

namespace RendererInits {

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

	VkImageViewCreateInfo imageviewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	VkSamplerCreateInfo samplerCreateInfo(uint32_t mipLevels, VkPhysicalDevice physicalDevice);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
		VkShaderModule shaderModule, const char* entry = "main");

	VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet,
		VkDescriptorImageInfo* imageInfo, uint32_t binding);

	VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet,
		VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	VkDescriptorBufferInfo bufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
}