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

	SceneObjectManagerPtr getSceneObjectManager() { return m_sceneObjectManager; };
	CameraPtr getCamera() { return m_camera; }

	void saveScene(const std::string& filename);
	void loadScene(const std::string& filename);
	void drawInterface();
private:
	std::vector<UniformBufferPtr> m_uniformBuffers;
	DescriptorAllocator m_globalDescriptorAllocator;
	std::vector<VkDescriptorSet> m_globalDescriptorSets;
	SceneObjectManagerPtr m_sceneObjectManager;

	GlobalUBO ubo{};

	CameraPtr m_camera;
	DirectionalLight m_light;
	float m_ambientCoefficient = 0.005f;

	std::vector<PointLightPtr> m_pointLights;
	std::filesystem::path m_scenesDirectory = "Scenes";

	friend class Renderer;
	friend class Camera;
	friend class SceneObjectManager;
	friend class PointLightObject;
};

