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

	glm::mat4 initialOrientation;

	friend class ModelInstance;
};

class ModelInstance
{
public:
	ModelInstance(ModelDataPtr modelData);
	~ModelInstance();

	void setTranslation(float x, float y, float z);
	void setRotationX(float angle);
	void setRotationY(float angle);
	void setRotationZ(float angle);
	void setScale(float scale);

	// Getter functions
	glm::vec3 getTranslation() const { return m_translation; }
	glm::vec3 getRotation() const { return m_rotation; }
	float getScale() const { return m_scale; }

	void update();
	void draw();

	float m_scale = 1.0f;
	float m_shininess = 1.0f;
private:
	ModelDataPtr m_modelData;
	std::vector<UniformBufferPtr> m_uniformBuffers;
	std::vector<TransformDescriptorSetPtr> m_descriptorSets;

	ModelUBO ubo{};

	glm::mat4 transformMatrix = glm::mat4(1.0f);
	glm::mat4 rotationMatrix = glm::mat4(1.0f);
	glm::mat4 scaleMatrix = glm::mat4(1.0f);

	glm::vec3 m_translation = glm::vec3(0.0f);
	glm::vec3 m_rotation = glm::vec3(0.0f);
	
	friend class Application;
};
