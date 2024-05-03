#include "Model.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "ModelDescriptorSet.h"
#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"

Model::Model(ModelDataPtr modelData, Scene* scene) : SceneObject(scene), m_modelData(modelData)
{
	//Initialize uniformBuffers and descriptorSets
	for (int i = 0; i < Renderer::s_maxFramesInFlight; i++) {
		m_uniformBuffers.push_back(GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(ModelUBO)));
		m_descriptorSets.push_back(GraphicsEngine::get()->getRenderer()->createModelDescriptorSet(m_uniformBuffers[i]->get()));
	}
}

Model::~Model()
{
}

void Model::update()
{
	SceneObject::update();

	ubo.model = m_modelData->initialPosition * positionMatrix * m_modelData->initialRotation * rotationMatrix * m_modelData->initialScale * scaleMatrix;

	ubo.shininess = m_shininess;

	ubo.kd = 0.8f;//kd;
	ubo.ks = 0.2f;//ks;

	memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Model::draw()
{
	m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();

	m_modelData->m_mesh->draw();
	if (m_modelData->m_texture) {
		m_modelData->m_texture->draw();
	}

	GraphicsEngine::get()->getRenderer()->bindDescriptorSets();

	if (m_modelData->m_mesh->hasIndexBuffer()) {
		vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(),
			static_cast<uint32_t>(m_modelData->m_mesh->getIndicesSize()), 1, 0, 0, 0);
	}
	else {
		vkCmdDraw(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 3, 1, 0, 0);
	}
}
