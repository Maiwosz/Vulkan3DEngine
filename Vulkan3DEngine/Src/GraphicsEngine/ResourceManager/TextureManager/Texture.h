#pragma once
#include "../Resource.h"
#include "../../../Prerequisites.h"
#include <vector>

class Texture : public Resource
{
public:
	Texture(const char* full_path);
	~Texture();

	ImagePtr getImage() { return m_image; }
	ImageViewPtr getImageView() { return m_imageView; }
	TextureSamplerPtr getTextureSampler() { return m_textureSampler; }

	void draw();
private:
	ImagePtr m_image;
	ImageViewPtr m_imageView;
	TextureSamplerPtr m_textureSampler;
	std::vector<TextureDescriptorSetPtr> m_descriptorSets;//one for every frame;
};