#include "TextureManager.h"

TextureManager::TextureManager()
    : ResourceManager("Assets\\Textures")
{
}

TextureManager::~TextureManager()
{
}

TexturePtr TextureManager::loadTexture(const std::string& name)
{
    return std::static_pointer_cast<Texture>(loadResource(name));
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
