#include "Model.h"
#include "../../../Application/Application.h"
#include "../MeshManager/Mesh.h"
#include "../TextureManager/Texture.h"
#include "../../Renderer/Buffers/UniformBuffer/UniformBuffer.h"
#include "../../Renderer/DescriptorSets/DescriptorSet/TransformDescriptorSet/TransformDescriptorSet.h"

#include <fstream>
#include <json.hpp>

ModelData::ModelData(MeshPtr mesh, TexturePtr texture):m_mesh(mesh), m_texture(texture)
{
	// Initialize the model matrix to an identity matrix
	initialOrientation = glm::mat4(1.0f);
}

ModelData::ModelData(const char* full_path) : Resource(full_path)
{
	Load(full_path);
}

ModelData::~ModelData()
{
}

void ModelData::Reload()
{
	vkDeviceWaitIdle(GraphicsEngine::get()->getRenderer()->getDevice()->get());

	// Free the old resources
	m_mesh.reset();
	m_texture.reset();

	// Load the new model
	Load(m_full_path.c_str());
}

void ModelData::Load(const char* full_path)
{
	// Open the file
	std::ifstream file(full_path);

	// Parse the JSON file
	nlohmann::json j;
	file >> j;

	// Read the mesh file path
	std::string mesh_path = j["mesh_path"];

	// Load the mesh
	m_mesh = GraphicsEngine::get()->getMeshManager()->loadMesh(mesh_path);

	// Load the texture if it exists
	if (j.count("texture_path") > 0 && !j["texture_path"].empty()) {
		std::string texture_path = j["texture_path"];
		m_texture = GraphicsEngine::get()->getTextureManager()->loadTexture(texture_path);
	}

	// Read the initial transformations
	std::vector<float> translationVec = j["translation"].get<std::vector<float>>();
	glm::vec3 translation(translationVec[0], translationVec[1], translationVec[2]);
	std::vector<float> rotationVec = j["rotation"].get<std::vector<float>>();
	glm::vec3 rotation(rotationVec[0], rotationVec[1], rotationVec[2]);
	float scale = j["scale"];

	// Apply the initial transformations
	initialOrientation = glm::mat4(1.0f);
	initialOrientation = glm::translate(initialOrientation, glm::vec3(translation.x, translation.y, translation.z));
	initialOrientation = glm::rotate(initialOrientation, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	initialOrientation = glm::rotate(initialOrientation, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	initialOrientation = glm::rotate(initialOrientation, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	initialOrientation = glm::scale(initialOrientation, glm::vec3(scale)); // skalowanie

	// Close the file
	file.close();
}

ModelInstance::ModelInstance(ModelDataPtr modelData) : m_modelData(modelData)
{
	//Initialize uniformBuffers and descriptorSets
	for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
		m_uniformBuffers.push_back(GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(ModelUBO)));
		m_descriptorSets.push_back(GraphicsEngine::get()->getRenderer()->createTransformDescriptorSet(m_uniformBuffers[i]->get()));
	}
}

ModelInstance::~ModelInstance()
{
}

void ModelInstance::setTranslation(float x, float y, float z)
{
	m_translation = glm::vec3(x, y, z);
}

void ModelInstance::setRotationX(float angle)
{
	m_rotation.x = angle;
}

void ModelInstance::setRotationY(float angle)
{
	m_rotation.y = angle;
}

void ModelInstance::setRotationZ(float angle)
{
	m_rotation.z = angle;
}

void ModelInstance::setScale(float scale)
{
	m_scale = scale;
}

void ModelInstance::update()
{
	glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), m_translation);
	glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));

	ubo.model = m_modelData->initialOrientation * translationMatrix * rotationMatrix * scaleMatrix;
	ubo.shininess = m_shininess;
	memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void ModelInstance::draw()
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
