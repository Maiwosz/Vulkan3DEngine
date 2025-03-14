#include "Texture.h"
#include "Prerequisites.h"
#include "GraphicsEngine.h"
#include "StagingBuffer.h"
#include "Image.h"
#include "RendererInits.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool Texture::m_descriptorAllocatorInitialized = false;
DescriptorAllocatorGrowable Texture::m_descriptorAllocator;

Texture::Texture(const std::filesystem::path& full_path) : Resource(full_path)
{
	Load(full_path);
}

Texture::~Texture()
{
	m_descriptorAllocator.destroyPools(GraphicsEngine::get()->getDevice()->get());
	m_descriptorSets.clear();
	vkDestroySampler(GraphicsEngine::get()->getDevice()->get(), m_sampler, nullptr);
	vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), m_imageView, nullptr);
	m_image.reset();
}

void Texture::Load(const std::filesystem::path& full_path)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(full_path.string().c_str(), &texWidth,
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

	VkFormat imageFormat;
	if (Renderer::s_msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
		imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	}
	else {
		imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	}
	
	m_image = GraphicsEngine::get()->getRenderer()->createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_image->transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	stagingBuffer->copyBufferToImage(m_image);
	//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	//m_image->transitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_image->generateMipmaps();

	VkImageViewCreateInfo viewInfo = RendererInits::imageviewCreateInfo(m_image->get(), imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

	if (vkCreateImageView(GraphicsEngine::get()->getDevice()->get(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	VkSamplerCreateInfo samplerInfo = RendererInits::samplerCreateInfo(mipLevels, GraphicsEngine::get()->getDevice()->getPhysicalDevice());

	if (vkCreateSampler(GraphicsEngine::get()->getDevice()->get(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}

	if (!m_descriptorAllocatorInitialized) {
		{
			m_descriptorAllocator.init(GraphicsEngine::get()->getDevice()->get(), 8000, m_sizes);
			m_descriptorAllocatorInitialized = true;
		}
	}

	DescriptorWriter writer;
	//Initialize uniformBuffers and descriptorSets
	for (int i = 0; i < Renderer::s_maxFramesInFlight; i++) {

		VkDescriptorSet textureDescriptorSets = m_descriptorAllocator.allocate(GraphicsEngine::get()->getDevice()->get(),
			GraphicsEngine::get()->getRenderer()->m_textureDescriptorSetLayout);

		writer.writeImage(0, m_imageView, m_sampler, m_image->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.updateSet(GraphicsEngine::get()->getDevice()->get(), textureDescriptorSets);
		writer.clear();

		m_descriptorSets.push_back(textureDescriptorSets);
	}
}

void Texture::Reload()
{
	vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());

	// Free the old resources
	m_descriptorSets.clear();
	vkDestroySampler(GraphicsEngine::get()->getDevice()->get(), m_sampler, nullptr);
	vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), m_imageView, nullptr);
	m_image.reset();

	// Load the new texture
	Load(m_full_path.c_str());
}