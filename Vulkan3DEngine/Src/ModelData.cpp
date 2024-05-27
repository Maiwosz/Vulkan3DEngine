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
    try {
        // Open the file
        std::ifstream file(full_path);
        if (!file.is_open()) {
            throw std::runtime_error(fmt::format("Failed to open file: {}", full_path.string()));
        }

        // Parse the JSON file
        nlohmann::json j;
        try {
            file >> j;
        }
        catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error(fmt::format("Failed to parse JSON file: {}. Error: {}", full_path.string(), e.what()));
        }

        // Load the name from the file path
        m_name = full_path.stem().string();

        // Read the mesh file path
        std::filesystem::path mesh_path = j.at("mesh").get<std::filesystem::path>();

        // Load the mesh
        std::future<MeshPtr> meshFuture;
        meshFuture = ThreadPool::get()->enqueue([mesh_path]()->MeshPtr {
            return GraphicsEngine::get()->getMeshManager()->loadMesh(mesh_path);
            });

        // Read the texture file path
        std::filesystem::path texture_path = j.at("texture").get<std::filesystem::path>();

        // Load the texture
        std::future<TexturePtr> textureFuture;
        textureFuture = ThreadPool::get()->enqueue([texture_path]()->TexturePtr {
            return GraphicsEngine::get()->getTextureManager()->loadTexture(texture_path);
            });

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
        std::vector<float> translationVec = j.at("positionOffset").get<std::vector<float>>();
        glm::vec3 translation(translationVec[0], translationVec[1], translationVec[2]);
        std::vector<float> rotationVec = j.at("rotationOffset").get<std::vector<float>>();
        glm::vec3 rotation(rotationVec[0], rotationVec[1], rotationVec[2]);
        float scale = j.at("scaleOffset").get<float>();

        // Initialize the model matrix to an identity matrix
        m_scaleOffset = glm::mat4(1.0f);
        m_positionOffset = glm::mat4(1.0f);
        m_rotationOffset = glm::mat4(1.0f);

        // Apply the initial transformations
        m_scaleOffset = glm::scale(m_scaleOffset, glm::vec3(scale)); // skalowanie
        m_positionOffset = glm::translate(m_positionOffset, glm::vec3(translation.x, translation.y, translation.z));
        m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        // Close the file
        file.close();

        // Wait for the results of the asynchronous tasks
        m_mesh = meshFuture.get();
        m_texture = textureFuture.get();
    }
    catch (const std::exception& e) {
        fmt::print("Error loading model data: {}\n", e.what());
        // Handle or rethrow the exception as needed
        throw;
    }
}