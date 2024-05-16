#pragma once
#include "Prerequisites.h"
#include "GraphicsEngine.h"
#include "Resource.h"
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

	std::string m_name;
	float m_scale = 1.0f;
	float m_shininess = 1.0f;
	float m_kd = 0.8f;
	float m_ks = 0.2f;

	glm::mat4 initialScale;
	glm::mat4 initialPosition;
	glm::mat4 initialRotation;

	friend class Model;
	friend class Renderer;
};
