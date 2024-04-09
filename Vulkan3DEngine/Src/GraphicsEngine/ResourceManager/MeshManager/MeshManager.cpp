#include "MeshManager.h"

MeshManager::MeshManager()
    : ResourceManager("Assets\\Meshes")
{
}

MeshManager::~MeshManager()
{
}

MeshPtr MeshManager::loadMesh(const std::string& name)
{
    return std::static_pointer_cast<Mesh>(loadResource(name));
}

Resource* MeshManager::createResourceFromFileConcrete(const char* file_path)
{
    Mesh* mesh = nullptr;
    try
    {
        mesh = new Mesh(file_path);
    }
    catch (...) {}

    return mesh;
}
