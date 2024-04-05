#pragma once
#include "../Prerequisites.h"
#include "../GraphicsEngine/GraphicsEngine.h"
#include <vector>

class Object
{
public:
	Object(MeshPtr mesh, TexturePtr texture);
	~Object();

	void update();
	void draw();
private:
	MeshPtr m_mesh;
	TexturePtr m_texture;
	friend class Application;
};

