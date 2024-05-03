#pragma once
#include "Prerequisites.h"

namespace RendererInits {

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

	VkImageViewCreateInfo imageviewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
		VkShaderModule shaderModule, const char* entry = "main");
}