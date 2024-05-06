#include "ImageView.h"
#include "Renderer.h"
#include "GraphicsEngine.h"

ImageView::ImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, Renderer* renderer): m_renderer(renderer)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(GraphicsEngine::get()->getDevice()->get(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}
}

ImageView::~ImageView()
{
	vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), m_imageView, nullptr);
}
