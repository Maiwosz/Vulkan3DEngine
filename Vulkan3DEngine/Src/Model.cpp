#include "Model.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "ModelData.h"
#include "Mesh.h"
#include "Texture.h"

DescriptorAllocatorGrowable Model::m_descriptorAllocator;
int Model::m_modelCount = 0;

glm::vec3 convertMat4ToVec3(glm::mat4 matrix) {
    return glm::vec3(matrix[3]);
}

float convertScaleMat4ToFloat(glm::mat4 matrix) {
    return glm::length(glm::vec3(matrix[0]));
}

glm::vec3 convertRotationMat4ToEuler(glm::mat4 matrix) {
    glm::quat quaternion = glm::quat_cast(matrix);
    return glm::degrees(glm::eulerAngles(quaternion));
}

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
	SceneObject(scene, modelData->m_name), m_mesh(modelData->m_mesh), m_texture(modelData->m_texture),
	m_shininess(modelData->m_shininess),m_kd(modelData->m_kd), m_ks(modelData->m_ks), m_positionOffset(modelData->m_positionOffset),
	m_rotationOffset(modelData->m_rotationOffset), m_scaleOffset(modelData->m_scaleOffset)
{
    if (m_modelCount++ == 0) {
		m_descriptorAllocator.init(GraphicsEngine::get()->getDevice()->get(), 1000, m_sizes);
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

Model::Model(std::string name, std::string meshName, std::string textureName, float scale, float shininess, float kd, float ks,
    float initialScale, glm::vec3 initialPosition, glm::vec3 initialRotation, Scene* scene) :
    SceneObject(scene, name), m_shininess(shininess), m_kd(kd), m_ks(ks)
{
    // Load the mesh
    std::future<MeshPtr> meshFuture;
    meshFuture = ThreadPool::get()->enqueue([meshName]()->MeshPtr { return GraphicsEngine::get()->getMeshManager()->loadMesh(meshName); });

    // Load the texture
    std::future<TexturePtr> textureFuture;
    textureFuture = ThreadPool::get()->enqueue([textureName]()->TexturePtr { return GraphicsEngine::get()->getTextureManager()->loadTexture(textureName); });

    setScaleOffset(initialScale);
    setPositionOffset(initialPosition);
    setRotationOffset(initialRotation);

    if (m_modelCount++ == 0) {
        m_descriptorAllocator.init(GraphicsEngine::get()->getDevice()->get(), 1000, m_sizes);
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

    // Oczekuj na wyniki zadañ
    m_mesh = meshFuture.get();
    m_texture = textureFuture.get();
}

Model::Model(const nlohmann::json& j, Scene* scene) : SceneObject(j["SceneObjectData"], scene) {
    try {
        from_json(j);
        if (m_modelCount++ == 0) {
            m_descriptorAllocator.init(GraphicsEngine::get()->getDevice()->get(), 1000, m_sizes);
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
        fmt::print("Model initialized successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to initialize Model. Exception: {}\n", e.what());
        throw;
    }
}


Model::~Model()
{
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
    if (--m_modelCount == 0) {
        m_descriptorAllocator.destroyPools(GraphicsEngine::get()->getDevice()->get());
    }
}

void Model::update()
{
	SceneObject::update();

    ubo.model = m_positionOffset * positionMatrix * m_rotationOffset * rotationMatrix * m_scaleOffset * scaleMatrix;

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
        // Open the popup
        ImGui::OpenPopup("File already exists");
    }
    else {
        // The file doesn't exist, so we can just write to it
        writeModelDataToFile(full_path);
    }

    // Create the popup
    if (ImGui::BeginPopupModal("File already exists", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A file with this name already exists. Do you want to overwrite it?");
        if (ImGui::Button("Yes")) {
            // Overwrite the file
            writeModelDataToFile(full_path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No")) {
            // Don't overwrite the file
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


void Model::writeModelDataToFile(const std::filesystem::path& full_path) {
    // Open the file
    std::ofstream file(full_path);

    // Create a JSON object
    nlohmann::json j;

    // Add the model data to the JSON object
    j["mesh"] = m_mesh->getName();
    j["texture"] = m_texture->getName();
    j["scale"] = m_scale;
    j["shininess"] = m_shininess;
    j["kd"] = m_kd;
    j["ks"] = m_ks;
    j["scaleOffset"] = getScaleOffset();
    glm::vec3 initPos = getPositionOffset();
    j["positionOffset"] = { initPos.x, initPos.y, initPos.z };
    glm::vec3 initRot = getRotationOffset();
    j["rotationOffset"] = { initRot.x, initRot.y, initRot.z };


    // Write the JSON object to the file
    file << j.dump(4);

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

float Model::getScaleOffset()
{
    return convertScaleMat4ToFloat(m_scaleOffset);
}

glm::vec3 Model::getPositionOffset()
{
    return convertMat4ToVec3(m_positionOffset);
}

glm::vec3 Model::getRotationOffset()
{
    return convertRotationMat4ToEuler(m_rotationOffset);
}

void Model::setScaleOffset(float scale)
{
    m_scaleOffset = glm::mat4(1.0f);
    m_scaleOffset = glm::scale(m_scaleOffset, glm::vec3(scale)); // skalowanie

}

void Model::setPositionOffset(glm::vec3 position)
{
    m_positionOffset = glm::mat4(1.0f);
    m_positionOffset = glm::translate(m_positionOffset, glm::vec3(position.x, position.y, position.z));
}

void Model::setRotationOffset(glm::vec3 rotation)
{
    m_rotationOffset = glm::mat4(1.0f);
    m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m_rotationOffset = glm::rotate(m_rotationOffset, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
}

void Model::to_json(nlohmann::json& j) {
    try {
        nlohmann::json SceneObjectJson;
        SceneObject::to_json(SceneObjectJson); // Call base class method

        std::filesystem::path full_path = std::filesystem::path("Assets") / "Models" / (m_name + ".json");

        if (std::filesystem::exists(full_path)) {
            // File exists, save reference to model_data
            j = nlohmann::json{
                {"type", "model"},
                {"SceneObjectData", SceneObjectJson},
                {"modelData", m_name + ".json"}
            };
        }
        else {
            // File does not exist, save all necessary data
            nlohmann::json modelDataJson;

            j = nlohmann::json{
                {"type", "model"},
                {"SceneObjectData", SceneObjectJson},
                {"scaleOffset", m_scale},
                {"positionOffset", {getPositionOffset().x, getPositionOffset().y, getPositionOffset().z}},
                {"rotationOffset", {getRotationOffset().x, getRotationOffset().y, getRotationOffset().z}},
                {"mesh", m_mesh->getName()},
                {"texture", m_texture->getName()},
                {"shininess", m_shininess},
                {"kd", m_kd},
                {"ks", m_ks}
            };
        }
    }
    catch (const std::exception& e) {
        std::cerr << "An error occurred while converting the model to JSON format: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "An unknown error occurred while converting the model to JSON format." << std::endl;
    }
}


void Model::from_json(const nlohmann::json& j) {
    try {
        if (j.count("modelData") > 0) {
            // Use the createModel function that takes only modelData
            ModelDataPtr modelData = GraphicsEngine::get()->getModelDataManager()->loadModelData(j["modelData"]);
            m_mesh = modelData->m_mesh;
            m_texture = modelData->m_texture;

            m_scale = modelData->m_scale;
            m_shininess = modelData->m_shininess;
            m_kd = modelData->m_kd;
            m_ks = modelData->m_ks;

            m_scaleOffset = modelData->m_scaleOffset;
            m_positionOffset = modelData->m_positionOffset;
            m_rotationOffset = modelData->m_rotationOffset;
        }
        else {
            // Use the createModel function that takes all attributes
            // Load the mesh
            std::future<MeshPtr> meshFuture;
            meshFuture = ThreadPool::get()->enqueue([j]()->MeshPtr { return GraphicsEngine::get()->getMeshManager()->loadMesh(j["mesh"].get<std::string>()); });

            // Load the texture
            std::future<TexturePtr> textureFuture;
            textureFuture = ThreadPool::get()->enqueue([j]()->TexturePtr { return GraphicsEngine::get()->getTextureManager()->loadTexture(j["texture"].get<std::string>()); });

            m_shininess = j["shininess"];
            m_kd = j["kd"];
            m_ks = j["ks"];

            setScaleOffset(j["scaleOffset"]);
            setPositionOffset(glm::vec3(j["positionOffset"][0], j["positionOffset"][1], j["positionOffset"][2]));
            setRotationOffset(glm::vec3(j["rotationOffset"][0], j["rotationOffset"][1], j["rotationOffset"][2]));

            // Oczekuj na wyniki zadañ
            m_mesh = meshFuture.get();
            m_texture = textureFuture.get();
        }
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to deserialize Model. Exception: {}\n", e.what());
        throw; // Rzuæmy wyj¹tek dalej, aby ³atwiej by³o œledziæ b³¹d
    }
}


