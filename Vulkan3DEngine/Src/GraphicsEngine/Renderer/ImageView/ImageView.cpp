#include "ImageView.h"
#include "../Renderer.h"

ImageView::ImageView(VkImage image, VkFormat format, Renderer* renderer): m_renderer(renderer)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(m_renderer->m_device->get(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}
}

ImageView::~ImageView()
{
	vkDestroyImageView(m_renderer->m_device->get(), m_imageView, nullptr);
}
