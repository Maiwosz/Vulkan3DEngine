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
	VkImageView getImageView() { return m_imageView; }
	VkSampler getSampler() { return m_sampler; }
	
	void Reload() override;
private:
	void Load(const char* full_path) override;

	ImagePtr m_image;
	VkImageView m_imageView;
	VkSampler m_sampler;

	static bool m_descriptorAllocatorInitialized;
	static DescriptorAllocatorGrowable m_descriptorAllocator;
	std::vector<VkDescriptorSet> m_descriptorSets;//one for every frame in flight;

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> m_sizes =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
	};

	friend class Renderer;
};