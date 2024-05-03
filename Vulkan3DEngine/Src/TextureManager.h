#pragma once
#include "ResourceManager.h"
#include "Texture.h"

class TextureManager : public ResourceManager
{
public:
    TextureManager();
    ~TextureManager();

    TexturePtr loadTexture(const std::string& name);

protected:
    virtual Resource* createResourceFromFileConcrete(const char* file_path) override;
};
