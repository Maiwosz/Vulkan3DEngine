#include "MeshManager.h"

MeshManager::MeshManager()
    : ResourceManager("Assets/Meshes")
{
}

MeshManager::~MeshManager()
{
}

MeshPtr MeshManager::loadMesh(const std::filesystem::path& name)
{
    return std::static_pointer_cast<Mesh>(loadResource(name));
}

Resource* MeshManager::createResourceFromFileConcrete(const std::filesystem::path& file_path)
{
    Mesh* mesh = nullptr;
    try
    {
        mesh = new Mesh(file_path);
    }
    catch (...) {}

    return mesh;
}