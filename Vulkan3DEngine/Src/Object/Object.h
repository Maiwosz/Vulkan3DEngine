#pragma once
#include "../Prerequisites.h"
#include "../GraphicsEngine/ResourceManager/MeshManager/Mesh.h"
#include <vector>

class Object
{
public:
	Object(std::vector<Vertex> vertices);
	~Object();

	void update();
	void draw();
private:
	MeshPtr m_mesh;

	friend class Application;
};

