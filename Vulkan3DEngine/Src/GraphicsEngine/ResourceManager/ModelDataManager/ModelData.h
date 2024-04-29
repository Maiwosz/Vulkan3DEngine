#pragma once
#include "../../../Prerequisites.h"
#include "../../GraphicsEngine.h"
#include "../Resource.h"
#include <vector>

class ModelData : public Resource
{
public:
	ModelData(MeshPtr mesh, TexturePtr texture);
	ModelData(const char* full_path);
	~ModelData();

	void Reload() override;
private:
	void Load(const char* full_path) override;

	MeshPtr m_mesh;
	TexturePtr m_texture;

	glm::mat4 initialScale;
	glm::mat4 initialPosition;
	glm::mat4 initialRotation;

	friend class Model;
};
