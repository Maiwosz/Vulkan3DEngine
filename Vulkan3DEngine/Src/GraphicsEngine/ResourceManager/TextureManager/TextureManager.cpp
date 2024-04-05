#include "TextureManager.h"
#include "../../Renderer/ImageView/ImageView.h"
#include "Texture.h"

TextureManager::TextureManager() : ResourceManager()
{
}

TextureManager::~TextureManager()
{
}

TexturePtr TextureManager::createTextureFromFile(const char* file_path)
{
	return std::static_pointer_cast<Texture>(createResourceFromFile(file_path));
}

std::vector<VkImageView> TextureManager::getImageViews()
{
	std::vector<VkImageView> imageViews;
	for (const auto& pair : m_map_resources) {
		Texture* texture = dynamic_cast<Texture*>(pair.second.get());
		if (texture != nullptr) {
			imageViews.push_back(texture->getImageView()->get());
		}
	}
	return imageViews;
}

Resource* TextureManager::createResourceFromFileConcrete(const char* file_path)
{
	Texture* texture = nullptr;
	try
	{
		texture = new Texture(file_path);
	}
	catch (...) {}

	return texture;
}