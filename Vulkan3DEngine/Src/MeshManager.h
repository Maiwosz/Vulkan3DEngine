#pragma once
#include "ResourceManager.h"
#include "Mesh.h"

class MeshManager : public ResourceManager
{
public:
    MeshManager();
    ~MeshManager();

    MeshPtr loadMesh(const std::string& name);

protected:
    virtual Resource* createResourceFromFileConcrete(const char* file_path) override;
};
