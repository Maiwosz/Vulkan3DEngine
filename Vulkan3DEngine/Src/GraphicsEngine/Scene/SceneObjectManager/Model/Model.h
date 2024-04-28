#pragma once
#include "../../../../Prerequisites.h"
#include "../SceneObject.h"

class Model : public SceneObject
{
public:
	Model(ModelDataPtr modelData, Scene* scene);
	~Model();

	void update() override;
	void draw() override;

	float m_scale = 1.0f;
	float m_shininess = 1.0f;
	float kd = 0.8f;
	float ks = 0.2f;
private:
	ModelDataPtr m_modelData;
	std::vector<UniformBufferPtr> m_uniformBuffers;
	std::vector<ModelDescriptorSetPtr> m_descriptorSets;

	ModelUBO ubo{};

	friend class Application;
};

