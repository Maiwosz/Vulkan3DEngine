#pragma once
#include "ResourceManager.h"
#include "Mesh.h"

class MeshManager : public ResourceManager
{
public:
    MeshManager();
    ~MeshManager();

    MeshPtr loadMesh(const std::filesystem::path& name);

protected:
    virtual Resource* createResourceFromFileConcrete(const std::filesystem::path& file_path) override;
};
