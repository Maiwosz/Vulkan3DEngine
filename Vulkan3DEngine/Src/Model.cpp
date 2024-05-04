#include "Model.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "ModelDescriptorSet.h"
#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"

bool  Model::m_descriptorAllocatorInitialized = false;
DescriptorAllocatorGrowable Model::m_descriptorAllocator;

Model::Model(ModelDataPtr modelData, Scene* scene) : SceneObject(scene), m_modelData(modelData)
{
	if (!m_descriptorAllocatorInitialized) {
		{
			m_descriptorAllocator.init(GraphicsEngine::get()->getRenderer()->getDevice()->get(), 1000, m_sizes);
			m_descriptorAllocatorInitialized = true;
		}
	}

	DescriptorWriter writer;
	//Initialize uniformBuffers and descriptorSets
	for (int i = 0; i < Renderer::s_maxFramesInFlight; i++) {
		m_uniformBuffers.push_back(GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(ModelUBO)));

		VkDescriptorSet modelDescriptorSets = m_descriptorAllocator.allocate(GraphicsEngine::get()->getRenderer()->getDevice()->get(),
			GraphicsEngine::get()->getRenderer()->m_modelDescriptorSetLayout);

		writer.writeBuffer(0, m_uniformBuffers[i]->get(), sizeof(ModelUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.updateSet(GraphicsEngine::get()->getRenderer()->getDevice()->get(), modelDescriptorSets);
		writer.clear();

		m_descriptorSets.push_back(modelDescriptorSets);
	}
}

Model::~Model()
{
	m_descriptorAllocator.destroyPools(GraphicsEngine::get()->getRenderer()->getDevice()->get());
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
	GraphicsEngine::get()->getRenderer()->bindDescriptorSet(m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()], 1);

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
