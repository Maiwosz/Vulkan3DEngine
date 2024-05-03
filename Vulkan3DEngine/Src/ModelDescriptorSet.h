#pragma once
#include "DescriptorSet.h"

class ModelDescriptorSetLayout : public DescriptorSetLayout
{
public:
	ModelDescriptorSetLayout(Renderer* renderer);
	~ModelDescriptorSetLayout();
private:

};


class ModelDescriptorSet : public DescriptorSet
{
public:
	ModelDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer);
	~ModelDescriptorSet();

	void bind() override;
private:
	Renderer* m_renderer;
};
