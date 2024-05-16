#include "Model.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"

bool  Model::m_descriptorAllocatorInitialized = false;
DescriptorAllocatorGrowable Model::m_descriptorAllocator;

Model::Model(ModelDataPtr modelData, Scene* scene) : SceneObject(scene), m_modelData(modelData)
{
	if (!m_descriptorAllocatorInitialized) {
		{
			m_descriptorAllocator.init(GraphicsEngine::get()->getDevice()->get(), 1000, m_sizes);
			m_descriptorAllocatorInitialized = true;
		}
	}

	DescriptorWriter writer;
	//Initialize uniformBuffers and descriptorSets
	for (int i = 0; i < Renderer::s_maxFramesInFlight; i++) {
		m_uniformBuffers.push_back(GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(ModelUBO)));

		VkDescriptorSet modelDescriptorSets = m_descriptorAllocator.allocate(GraphicsEngine::get()->getDevice()->get(),
			GraphicsEngine::get()->getRenderer()->m_modelDescriptorSetLayout);

		writer.writeBuffer(0, m_uniformBuffers[i]->get(), sizeof(ModelUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.updateSet(GraphicsEngine::get()->getDevice()->get(), modelDescriptorSets);
		writer.clear();

		m_descriptorSets.push_back(modelDescriptorSets);
	}
}

Model::~Model()
{
	m_descriptorAllocator.destroyPools(GraphicsEngine::get()->getDevice()->get());
}

void Model::update()
{
	SceneObject::update();

	ubo.model = m_modelData->initialPosition * positionMatrix * m_modelData->initialRotation * rotationMatrix * m_modelData->initialScale * scaleMatrix;

	ubo.shininess = m_modelData->m_shininess;

	ubo.kd = m_modelData->m_kd;
	ubo.ks = m_modelData->m_ks;

	memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Model::draw()
{
	GraphicsEngine::get()->getRenderer()->drawModel(this);
}
