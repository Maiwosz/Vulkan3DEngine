#pragma once
#include "Prerequisites.h"
#include "SceneObject.h"
#include "Descriptors.h"

class PointLightObject : public SceneObject
{
public:
	PointLightObject(Scene* scene);
	PointLightObject(glm::vec3 color, float intensity, Scene* scene);
	PointLightObject(float radius, glm::vec3 color, float intensity, Scene* scene);
	PointLightObject(glm::vec3 position, float radius, glm::vec3 color, float intensity, Scene* scene);
	PointLightObject(const nlohmann::json& j, Scene* scene);
	~PointLightObject();

	void update() override;
	void draw() override;

	void to_json(nlohmann::json& j) override;
	void from_json(const nlohmann::json& j) override;

	PointLightPtr m_light;
private:
	friend class Renderer;
};

