#pragma once
#include "..\..\..\Prerequisites.h"

class Image
{
public:
	Image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, Renderer* renderer);
	~Image();

	VkImage& get() { return m_image; }
	const uint32_t getWidth() { return m_width; }
	const uint32_t getHeight() { return m_height; }

	void transitionImageLayout(VkImageLayout newLayout);
	void updateLayout(VkImageLayout newLayout);
	const VkImageLayout& getLayout() { return m_layout; }
private:
	Renderer* m_renderer = nullptr;

	VkImage m_image;
	VkDeviceMemory m_imageMemory;
	uint32_t m_width;
	uint32_t m_height;
	VkFormat m_format;
	VkImageTiling m_tiling;
	VkImageUsageFlags m_usage;
	VkMemoryPropertyFlags m_properties;
	VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

