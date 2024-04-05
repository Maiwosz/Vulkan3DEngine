#pragma once
#include "..\DescriptorSet.h"

class GlobalDescriptorSet: public DescriptorSet
{
public:
	GlobalDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer);
	~GlobalDescriptorSet();
private:
	Renderer* m_renderer;
};
