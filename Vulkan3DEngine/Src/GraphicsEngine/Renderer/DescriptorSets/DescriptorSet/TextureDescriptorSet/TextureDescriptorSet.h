#pragma once
#include "..\DescriptorSet.h"

class TextureDescriptorSet : public DescriptorSet
{
public:
	TextureDescriptorSet(VkImageView imageView, VkSampler sampler, Renderer* renderer);
	~TextureDescriptorSet();
private:
	Renderer* m_renderer;
};
