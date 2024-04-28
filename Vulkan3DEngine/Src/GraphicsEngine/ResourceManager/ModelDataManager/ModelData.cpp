#include "ModelData.h"
#include "../MeshManager/Mesh.h"
#include "../TextureManager/Texture.h"


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