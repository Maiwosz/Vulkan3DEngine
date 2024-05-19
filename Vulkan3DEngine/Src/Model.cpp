#include "Model.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"

bool  Model::m_descriptorAllocatorInitialized = false;
DescriptorAllocatorGrowable Model::m_descriptorAllocator;

std::string mat4ToString(const glm::mat4& mat) {
    std::stringstream ss;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            ss << mat[i][j] << ' ';
        }
        ss << '\n';
    }
    return ss.str();
}

Model::Model(ModelDataPtr modelData, Scene* scene): 
	SceneObject(scene, modelData->m_name), m_mesh(modelData->m_mesh), m_texture(modelData->m_texture), m_scale(modelData->m_scale),
	m_shininess(modelData->m_shininess),m_kd(modelData->m_kd), m_ks(modelData->m_ks), m_initialPosition(modelData->m_initialPosition),
	m_initialRotation(modelData->m_initialRotation), m_initialScale(modelData->m_initialScale)
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

	ubo.model = m_initialPosition * positionMatrix * m_initialRotation * rotationMatrix * m_initialScale * scaleMatrix;

	ubo.shininess = m_shininess;

	ubo.kd = m_kd;
	ubo.ks = m_ks;

	memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Model::draw()
{
	GraphicsEngine::get()->getRenderer()->drawModel(this);
}

void Model::saveModelData() {
    // Construct the full path for the new file
    std::filesystem::path full_path = std::filesystem::path("Assets") / "Models" / (m_name + ".json");

    // Check if the file already exists
    if (std::filesystem::exists(full_path)) {
        // Ask the user if they want to overwrite the existing file
        bool open = true;
        if (ImGui::Begin("File already exists", &open)) {
            ImGui::Text("A file with this name already exists. Do you want to overwrite it?");
            if (ImGui::Button("Yes")) {
                // Overwrite the file
                writeModelDataToFile(full_path);
                ImGui::End();
                return;
            }
            if (ImGui::Button("No")) {
                // Don't overwrite the file
                ImGui::End();
                return;
            }
            ImGui::End();
        }
    }
    else {
        // The file doesn't exist, so we can just write to it
        writeModelDataToFile(full_path);
    }
}

void Model::writeModelDataToFile(const std::filesystem::path& full_path) {
    // Open the file
    std::ofstream file(full_path);

    // Create a JSON object
    nlohmann::json j;

    // Add the model data to the JSON object
    j["mesh_path"] = m_mesh->getName();
    j["texture_path"] = m_texture->getName();
    j["scale"] = m_scale;
    j["shininess"] = m_shininess;
    j["kd"] = m_kd;
    j["ks"] = m_ks;
    j["initial_scale"] = mat4ToString(m_initialScale);
    j["initial_position"] = mat4ToString(m_initialPosition);
    j["initial_rotation"] = mat4ToString(m_initialRotation);

    // Write the JSON object to the file
    file << j;

    // Close the file
    file.close();
}

void Model::setMesh(MeshPtr mesh)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mesh = mesh;
}

void Model::setTexture(TexturePtr texture)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_texture = texture;
}
