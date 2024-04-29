#include "ModelData.h"
#include "../MeshManager/Mesh.h"
#include "../TextureManager/Texture.h"
#include "../../../ThreadPool/ThreadPool.h"


#include <fstream>
#include <json.hpp>
#include <future>

ModelData::ModelData(MeshPtr mesh, TexturePtr texture):m_mesh(mesh), m_texture(texture)
{
	
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
	//m_mesh = GraphicsEngine::get()->getMeshManager()->loadMesh(mesh_path);

	std::future<MeshPtr> meshFuture;
	meshFuture = ThreadPool::get()->enqueue([mesh_path]()->MeshPtr { return GraphicsEngine::get()->getMeshManager()->loadMesh(mesh_path); });

	// Read the texture file path
	std::string texture_path = j["texture_path"];

	// Load the texture
	//m_texture = GraphicsEngine::get()->getTextureManager()->loadTexture(texture_path);

	std::future<TexturePtr> textureFuture;
	textureFuture = ThreadPool::get()->enqueue([texture_path]()->TexturePtr { return GraphicsEngine::get()->getTextureManager()->loadTexture(texture_path); });

	// Read the initial transformations
	std::vector<float> translationVec = j["translation"].get<std::vector<float>>();
	glm::vec3 translation(translationVec[0], translationVec[1], translationVec[2]);
	std::vector<float> rotationVec = j["rotation"].get<std::vector<float>>();
	glm::vec3 rotation(rotationVec[0], rotationVec[1], rotationVec[2]);
	float scale = j["scale"];

	// Initialize the model matrix to an identity matrix
	initialScale = glm::mat4(1.0f);
	initialPosition = glm::mat4(1.0f);
	initialRotation = glm::mat4(1.0f);

	// Apply the initial transformations
	initialScale = glm::scale(initialScale, glm::vec3(scale)); // skalowanie
	initialPosition = glm::translate(initialPosition, glm::vec3(translation.x, translation.y, translation.z));
	initialRotation = glm::rotate(initialRotation, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	initialRotation = glm::rotate(initialRotation, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	initialRotation = glm::rotate(initialRotation, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	// Close the file
	file.close();

	// Oczekuj na wyniki zadañ
	m_mesh = meshFuture.get();
	m_texture = textureFuture.get();
}