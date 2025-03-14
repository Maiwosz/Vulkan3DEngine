#pragma once
#include "Prerequisites.h"
#include "GraphicsEngine.h"
#include "Resource.h"
#include <vector>

class ModelData : public Resource
{
public:
	ModelData(MeshPtr mesh, TexturePtr texture);
	ModelData(const std::filesystem::path& full_path);
	~ModelData();

	void Reload() override;
private:
	void Load(const std::filesystem::path& full_path) override;

	MeshPtr m_mesh;
	TexturePtr m_texture;

	std::string m_name;
	float m_scale = 1.0f;
	float m_shininess = 1.0f;
	float m_kd = 0.8f;
	float m_ks = 0.2f;

	glm::mat4 m_scaleOffset;
	glm::mat4 m_positionOffset;
	glm::mat4 m_rotationOffset;

	friend class Model;
	friend class Renderer;
};