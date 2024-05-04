#pragma once
#include "Prerequisites.h"
#include "Model.h"
#include "Descriptors.h"
#include <string>

class Scene {
public:
    Scene();
    ~Scene();

    void update();
    void draw();

	CameraPtr getCamera() { return m_camera; }
private:
	std::vector<UniformBufferPtr> m_uniformBuffers;
	DescriptorAllocator m_globalDescriptorAllocator;
	std::vector<VkDescriptorSet> m_globalDescriptorSets;
	SceneObjectManagerPtr m_sceneObjectManager;

	GlobalUBO ubo{};

	CameraPtr m_camera;

	ModelPtr m_floor;

	ModelPtr m_statue1;
	ModelPtr m_statue2;
	ModelPtr m_statue3;
	ModelPtr m_statue4;

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
	friend class SceneObjectManager;
};

