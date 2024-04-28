#pragma once
#include "../../Prerequisites.h"
#include "SceneObjectManager/Model/Model.h"

class Scene {
public:
    Scene();
    ~Scene();

    void update();
    void draw();

	CameraPtr getCamera() { return m_camera; }
private:
	std::vector<UniformBufferPtr> m_uniformBuffers;
	std::vector<GlobalDescriptorSetPtr> m_globalDescriptorSets;
	SceneObjectManagerPtr m_sceneObjectManager;

	GlobalUBO ubo{};

	CameraPtr m_camera;

	ModelPtr m_floor;

	ModelPtr m_statue1;
	ModelPtr m_statue2;
	ModelPtr m_statue3;
	ModelPtr m_statue4;
	ModelPtr m_vikingRoom;
	ModelPtr m_castle;
	ModelPtr m_hygieia;

	ModelPtr m_pointLight1Sphere;
	ModelPtr m_pointLight2Sphere;
	ModelPtr m_pointLight3Sphere;

	DirectionalLight m_light;
	PointLight m_pointLight1;
	PointLight m_pointLight2;
	PointLight m_pointLight3;
	PointLight m_pointLight4;
	PointLight m_pointLight5;
	PointLight m_pointLight6;

	float m_lightAngle = 0.0f;

	friend class Camera;
};

