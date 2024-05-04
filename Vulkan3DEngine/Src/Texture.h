#pragma once
#include "Resource.h"
#include "Prerequisites.h"
#include <vector>
#include "Descriptors.h"

class Texture : public Resource
{
public:
	Texture(const char* full_path);
	~Texture();

	ImagePtr getImage() { return m_image; }
	ImageViewPtr getImageView() { return m_imageView; }
	TextureSamplerPtr getTextureSampler() { return m_textureSampler; }

	
	void Reload() override;

	void draw();
private:
	void Load(const char* full_path) override;

	ImagePtr m_image;
	ImageViewPtr m_imageView;
	TextureSamplerPtr m_textureSampler;

	static bool m_descriptorAllocatorInitialized;
	static DescriptorAllocatorGrowable m_descriptorAllocator;
	std::vector<VkDescriptorSet> m_descriptorSets;//one for every frame in flight;

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> m_sizes =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
	};
};