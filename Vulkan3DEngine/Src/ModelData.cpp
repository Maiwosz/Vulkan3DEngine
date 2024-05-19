#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"
#include "ThreadPool.h"

#include <fstream>
#include <future>
#include <filesystem> // do pobrania nazwy pliku bez rozszerzenia

ModelData::ModelData(MeshPtr mesh, TexturePtr texture) :m_mesh(mesh), m_texture(texture)
{

}

ModelData::ModelData(const std::filesystem::path& full_path) : Resource(full_path)
{
	Load(full_path);
}

ModelData::~ModelData()
{
}

void ModelData::Reload()
{
	vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());

	// Free the old resources
	m_mesh.reset();
	m_texture.reset();

	// Load the new model
	Load(m_full_path);
}

void ModelData::Load(const std::filesystem::path& full_path)
{
	// Open the file
	std::ifstream file(full_path);

	// Parse the JSON file
	nlohmann::json j;
	file >> j;

	// Load the name from the file path
	m_name = full_path.stem().string();

	// Read the mesh file path
	std::filesystem::path mesh_path = j["mesh_path"];

	// Load the mesh
	std::future<MeshPtr> meshFuture;
	meshFuture = ThreadPool::get()->enqueue([mesh_path]()->MeshPtr { return GraphicsEngine::get()->getMeshManager()->loadMesh(mesh_path); });

	// Read the texture file path
	std::filesystem::path texture_path = j["texture_path"];

	// Load the texture
	std::future<TexturePtr> textureFuture;
	textureFuture = ThreadPool::get()->enqueue([texture_path]()->TexturePtr { return GraphicsEngine::get()->getTextureManager()->loadTexture(texture_path); });

	// Load the variables if they exist in the JSON file
	if (j.find("scale") != j.end()) {
		m_scale = j["scale"];
	}
	if (j.find("shininess") != j.end()) {
		m_shininess = j["shininess"];
	}
	if (j.find("kd") != j.end()) {
		m_kd = j["kd"];
	}
	if (j.find("ks") != j.end()) {
		m_ks = j["ks"];
	}

	// Read the initial transformations
	std::vector<float> translationVec = j["translation"].get<std::vector<float>>();
	glm::vec3 translation(translationVec[0], translationVec[1], translationVec[2]);
	std::vector<float> rotationVec = j["rotation"].get<std::vector<float>>();
	glm::vec3 rotation(rotationVec[0], rotationVec[1], rotationVec[2]);
	float scale = j["scale"];

	// Initialize the model matrix to an identity matrix
	m_initialScale = glm::mat4(1.0f);
	m_initialPosition = glm::mat4(1.0f);
	m_initialRotation = glm::mat4(1.0f);

	// Apply the initial transformations
	m_initialScale = glm::scale(m_initialScale, glm::vec3(scale)); // skalowanie
	m_initialPosition = glm::translate(m_initialPosition, glm::vec3(translation.x, translation.y, translation.z));
	m_initialRotation = glm::rotate(m_initialRotation, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	m_initialRotation = glm::rotate(m_initialRotation, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m_initialRotation = glm::rotate(m_initialRotation, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	// Close the file
	file.close();

	// Oczekuj na wyniki zadañ
	m_mesh = meshFuture.get();
	m_texture = textureFuture.get();
}