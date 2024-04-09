#pragma once
#include "../Prerequisites.h"
#include "../GraphicsEngine/GraphicsEngine.h"
#include <vector>

class Object
{
public:
	Object(MeshPtr mesh, TexturePtr texture);
	~Object();

	void setTranslation(float x, float y, float z);
	void setRotationX(float angle);
	void setRotationY(float angle);
	void setRotationZ(float angle);
	void setScale(float scale);

	void update();
	void draw();
private:
	MeshPtr m_mesh;
	TexturePtr m_texture;
	std::vector<UniformBufferPtr> m_uniformBuffers;
	std::vector<TransformDescriptorSetPtr> m_descriptorSets;

	glm::mat4 initialOrientation;

	ObjectUBO ubo{};

	friend class Application;
};

