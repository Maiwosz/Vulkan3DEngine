#pragma once
#include "Prerequisites.h"
#include <vector>

class DescriptorPool
{
public:
	DescriptorPool(uint32_t textureCount, Renderer* renderer);
	~DescriptorPool();

	bool allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor);
	void freeDescriptors(std::vector<VkDescriptorSet>& descriptors);
	void resetPool();
private:
	Renderer* m_renderer;
	VkDescriptorPool m_descriptorPool;
};
