#pragma once
#include "DescriptorSet.h"

class TextureDescriptorSetLayout : public DescriptorSetLayout
{
public:
	TextureDescriptorSetLayout(Renderer* renderer);
	~TextureDescriptorSetLayout();


private:

};

class TextureDescriptorSet : public DescriptorSet
{
public:
	TextureDescriptorSet(VkImageView imageView, VkSampler sampler, Renderer* renderer);
	~TextureDescriptorSet();

	void bind() override;
private:
	Renderer* m_renderer;
};
