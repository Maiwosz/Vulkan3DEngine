#pragma once
#include "..\DescriptorSet.h"

class TransformDescriptorSetLayout : public DescriptorSetLayout
{
public:
	TransformDescriptorSetLayout(Renderer* renderer);
	~TransformDescriptorSetLayout();
private:

};


class TransformDescriptorSet : public DescriptorSet
{
public:
	TransformDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer);
	~TransformDescriptorSet();

	void bind() override;
private:
	Renderer* m_renderer;
};
