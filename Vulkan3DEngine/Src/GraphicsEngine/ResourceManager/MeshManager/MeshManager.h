#pragma once
#include "../ResourceManager.h"
#include "../../../Prerequisites.h"
#include "Mesh.h"

class MeshManager : public ResourceManager
{
public:
	MeshManager();
	~MeshManager();
	MeshPtr createMeshFromFile(const char* file_path);
protected:
	virtual Resource* createResourceFromFileConcrete(const char* file_path);
};