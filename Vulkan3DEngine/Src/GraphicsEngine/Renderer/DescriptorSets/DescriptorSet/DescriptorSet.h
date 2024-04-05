#pragma once
#include "..\..\..\..\Prerequisites.h"
#include <vector>

class DescriptorSet
{
public:
	DescriptorSet(const VkDescriptorSetLayout descriptorSetLayout, Renderer* renderer);
	~DescriptorSet();
	void update();
	void bind();
protected:
	VkDescriptorSet m_descriptorSet;
	std::vector<VkWriteDescriptorSet> m_writes;
private:
	Renderer* m_renderer;
};
