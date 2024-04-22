#include "Texture.h"
#include "../../../Prerequisites.h"
#include "../../GraphicsEngine.h"
#include "../../Renderer/Buffers/StagingBuffer/StagingBuffer.h"
#include "../../Renderer/Image/Image.h"
#include "../../Renderer/ImageView/ImageView.h"
#include "../../Renderer/TextureSampler/TextureSampler.h"
#include "../../Renderer/DescriptorSets/DescriptorSet/TextureDescriptorSet/TextureDescriptorSet.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture::Texture(const char* full_path) : Resource(full_path)
{
    Load(full_path);
}

Texture::~Texture()
{

}

void Texture::Load(const char* full_path)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(full_path, &texWidth,
		&texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	StagingBufferPtr stagingBuffer = GraphicsEngine::get()->getRenderer()->createStagingBuffer(imageSize);

	stagingBuffer->map();
	memcpy(stagingBuffer->getMappedMemory(), pixels, (size_t)imageSize);
	stagingBuffer->unmap();

	stbi_image_free(pixels);

	m_image = GraphicsEngine::get()->getRenderer()->createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_image->transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	stagingBuffer->copyBufferToImage(m_image);
	//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	//m_image->transitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_image->generateMipmaps();

	m_imageView = GraphicsEngine::get()->getRenderer()->createImageView(m_image->get(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	m_textureSampler = GraphicsEngine::get()->getRenderer()->createTextureSampler(mipLevels);

	for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
		m_descriptorSets.push_back(GraphicsEngine::get()->getRenderer()->createTextureDescriptorSet(m_imageView->get(), m_textureSampler->get()));
	}
}

void Texture::Reload()
{
    vkDeviceWaitIdle(GraphicsEngine::get()->getRenderer()->getDevice()->get());

    // Free the old resources
    m_descriptorSets.clear();
    m_textureSampler.reset();
    m_imageView.reset();
    m_image.reset();

    // Load the new texture
	Load(m_full_path.c_str());
}

void Texture::draw()
{
	m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();
}

