#pragma once
#include "..\DescriptorSet.h"

class TransformDescriptorSet : public DescriptorSet
{
public:
	TransformDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer);
	~TransformDescriptorSet();
private:
	Renderer* m_renderer;
};
