#pragma once
#include "ResourceManager.h"
#include "Texture.h"

class TextureManager : public ResourceManager
{
public:
    TextureManager();
    ~TextureManager();

    TexturePtr loadTexture(const std::filesystem::path& name);

protected:
    Resource* createResourceFromFileConcrete(const std::filesystem::path& file_path) override;
};
