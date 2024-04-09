#include "Object.h"
#include "../Application/Application.h"
#include "../GraphicsEngine/ResourceManager/MeshManager/Mesh.h"
#include "../GraphicsEngine/ResourceManager/TextureManager/Texture.h"
#include "../GraphicsEngine/Renderer/Buffers/UniformBuffer/UniformBuffer.h"
#include "../GraphicsEngine/Renderer/DescriptorSets/DescriptorSet/TransformDescriptorSet/\TransformDescriptorSet.h"

Object::Object(MeshPtr mesh, TexturePtr texture):m_mesh(mesh), m_texture(texture)
{
	for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
		m_uniformBuffers.push_back(GraphicsEngine::get()->getRenderer()->createUniformBuffer());
		m_descriptorSets.push_back(GraphicsEngine::get()->getRenderer()->createTransformDescriptorSet(m_uniformBuffers[i]->get()));
	}

	// Initialize the model matrix to an identity matrix
	ubo.model = glm::mat4(1.0f);
	initialOrientation = ubo.model;
}

Object::~Object()
{
}

void Object::setTranslation(float x, float y, float z)
{
	ubo.model = glm::translate(ubo.model, glm::vec3(x, y, z)); // przemieszczanie
	initialOrientation = ubo.model;
}

void Object::setRotationX(float angle)
{
	ubo.model = glm::rotate(ubo.model, glm::radians(angle), glm::vec3(1.0f, 0.0f, 0.0f)); // obracanie
	initialOrientation = ubo.model;
}

void Object::setRotationY(float angle)
{
	ubo.model = glm::rotate(ubo.model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f)); // obracanie
	initialOrientation = ubo.model;
}

void Object::setRotationZ(float angle)
{
	ubo.model = glm::rotate(ubo.model, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)); // obracanie
	initialOrientation = ubo.model;
}

void Object::setScale(float scale)
{
	ubo.model = glm::scale(ubo.model, glm::vec3(scale)); // skalowanie
	initialOrientation = ubo.model;
}

void Object::update()
{
	// Define the rotation speed
	float rotationSpeed = glm::radians(45.0f); // 45 degrees per second

	// Compute the rotation matrix
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), Application::s_deltaTime * rotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f));

	// Apply the rotation to the initial orientation of the object
	ubo.model = rotation * initialOrientation;

	memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Object::draw()
{
	GraphicsEngine::get()->getRenderer()->clearCurrentDescriptorSets();
	GraphicsEngine::get()->getRenderer()->bindGlobalDescriptorSet();

	m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();

	m_mesh->draw();
	m_texture->draw();

	GraphicsEngine::get()->getRenderer()->bindDescriptorSets();

	if (m_mesh->m_hasIndexedBuffer) {
		vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), static_cast<uint32_t>(m_mesh->m_indices.size()), 1, 0, 0, 0);
	}
	else {
		vkCmdDraw(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 3, 1, 0, 0);
	}
}
