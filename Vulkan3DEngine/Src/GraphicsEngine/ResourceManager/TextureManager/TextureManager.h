#pragma once
#include "../ResourceManager.h"
#include "../../../Prerequisites.h"
//#include "Texture.h"

class TextureManager : public ResourceManager
{
public:
	TextureManager();
	~TextureManager();
	TexturePtr createTextureFromFile(const char* file_path);
	std::vector<VkImageView> getImageViews();
protected:
	virtual Resource* createResourceFromFileConcrete(const char* file_path);
};