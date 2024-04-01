#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"

class DescriptorSetLayout
{
public:
	DescriptorSetLayout(Renderer* renderer);
	~DescriptorSetLayout();

	VkDescriptorSetLayout& get() { return m_descriptorSetLayout; }
private:
	Renderer* m_renderer;
	VkDescriptorSetLayout m_descriptorSetLayout;
};

class DescriptorSet
{
public:
	DescriptorSet(Renderer* renderer);
	~DescriptorSet();
private:
	Renderer* m_renderer;

	friend class GraphicsPipeline;
};

