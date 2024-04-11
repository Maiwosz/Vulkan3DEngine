#pragma once
#include "..\DescriptorSet.h"

class GlobalDescriptorSetLayout : public DescriptorSetLayout
{
public:
	GlobalDescriptorSetLayout(Renderer* renderer);
	~GlobalDescriptorSetLayout();


private:

};

class GlobalDescriptorSet: public DescriptorSet
{
public:
	GlobalDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer);
	~GlobalDescriptorSet();

	void bind() override;
private:
	Renderer* m_renderer;
};
