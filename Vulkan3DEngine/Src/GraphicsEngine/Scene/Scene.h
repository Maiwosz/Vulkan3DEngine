#pragma once
#include "../../Prerequisites.h"
#include "..\ResourceManager\ModelManager\Model.h"

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

	CameraPtr m_camera;

	ModelInstancePtr m_statue1;
	ModelInstancePtr m_statue2;
	ModelInstancePtr m_statue3;
	ModelInstancePtr m_statue4;
	ModelInstancePtr m_vikingRoom;
	ModelInstancePtr m_castle;
	ModelInstancePtr m_hygieia;

	DirectionalLight m_light;
};

