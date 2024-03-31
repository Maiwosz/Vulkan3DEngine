#pragma once
#include "../Prerequisites.h"
#include "../GraphicsEngine/GraphicsEngine.h"
#include <vector>

class Object
{
public:
	Object(MeshPtr mesh);
	~Object();

	void update();
	void draw();
private:
	MeshPtr m_mesh;

	friend class Application;
};

