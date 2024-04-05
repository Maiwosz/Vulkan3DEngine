#pragma once
#include "..\..\..\..\Prerequisites.h"
#include <vector>

class DescriptorSetLayout
{
public:
	DescriptorSetLayout(Renderer* renderer);
	~DescriptorSetLayout();

	VkDescriptorSetLayout& get() { return m_descriptorSetLayout; }
protected:
	void createBinding(uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags);
	void createLayout();
private:
	Renderer* m_renderer;
	VkDescriptorSetLayout m_descriptorSetLayout;
	std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

