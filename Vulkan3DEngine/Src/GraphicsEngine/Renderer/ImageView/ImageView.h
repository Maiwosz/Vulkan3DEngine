#pragma once
#include "..\..\..\Prerequisites.h"

class ImageView
{
public:
	ImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, Renderer* renderer);
	~ImageView();

	VkImageView& get() { return m_imageView; }
private:
	Renderer* m_renderer = nullptr;
	VkImageView m_imageView;
};

