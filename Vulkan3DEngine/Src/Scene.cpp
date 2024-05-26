#include "Scene.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "Descriptors.h"
#include "SceneObjectManager.h"
#include "Camera.h"
#include "InputSystem.h"
#include "RendererInits.h"
#include "AnimationBuilder.h"
#include "Animation.h"

#include <algorithm>

Scene::Scene()
{
    int maxFramesInFlight = Renderer::s_maxFramesInFlight;

    m_uniformBuffers.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        m_uniformBuffers[i] = GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(GlobalUBO));
    }

    m_globalDescriptorSets.resize(maxFramesInFlight);

    //create a descriptor pool that will hold <maxFramesInFlight> sets with 1 uniform buffer each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    m_globalDescriptorAllocator.initPool(GraphicsEngine::get()->getDevice()->get(), maxFramesInFlight, sizes);

    DescriptorWriter writer;

    for (int i = 0; i < maxFramesInFlight; i++) {
        m_globalDescriptorSets[i] = m_globalDescriptorAllocator.allocate(GraphicsEngine::get()->getDevice()->get(),
            GraphicsEngine::get()->getRenderer()->m_globalDescriptorSetLayout);
        
        
        writer.writeBuffer(0, m_uniformBuffers[i]->get(), sizeof(GlobalUBO),0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.updateSet(GraphicsEngine::get()->getDevice()->get(), m_globalDescriptorSets[i]);
        writer.clear();
    }

    m_sceneObjectManager = std::make_shared<SceneObjectManager>(this);

    m_camera = m_sceneObjectManager->createCamera(glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f);

    m_light.direction = glm::vec3(0.0f);
    m_light.color = glm::vec4(0.0f);

    //loadScene("test1");
}

Scene::~Scene()
{
    m_globalDescriptorAllocator.destroyPool(GraphicsEngine::get()->getDevice()->get());
}

void Scene::update()
{
    ubo = GlobalUBO{};

    ubo.ambientCoefficient = m_ambientCoefficient;
    
    m_sceneObjectManager->updateObjects();

    //Update uniform buffer

    auto distanceToCamera = [&](PointLightPtr light1, PointLightPtr light2) {
        float distance1 = glm::distance(light1->position, m_camera->getPosition());
        float distance2 = glm::distance(light2->position, m_camera->getPosition());
        return distance1 > distance2;
        };

    std::sort(m_pointLights.begin(), m_pointLights.end(), distanceToCamera);

    
    ubo.directionalLight = m_light;
    for (int i = 0; i < m_pointLights.size(); i++) {
        ubo.pointLights[i] = *m_pointLights[i];
        ubo.activePointLightCount = m_pointLights.size();
    }
    
    memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Scene::saveScene(const std::string& filename)
{
    nlohmann::json j;
    nlohmann::json directionalLight = {
        { "direction", { m_light.direction.x, m_light.direction.y, m_light.direction.z } },
        { "color", { m_light.color.x, m_light.color.y, m_light.color.z } },
        { "intensity", m_light.color.w }
    };

    j["directionalLight"] = directionalLight;
    m_sceneObjectManager->to_json(j);

    std::filesystem::path filePath = m_scenesDirectory / (filename + ".json");
    std::ofstream file(filePath);
    if (file.is_open())
    {
        file << j.dump(4);
        file.close();
        fmt::print("Scene saved successfully to file: {}\n", filePath.string());
    }
    else
    {
        fmt::print(stderr, "Error: Unable to open file for saving: {}\n", filePath.string());
    }
}

void Scene::loadScene(const std::string& filename)
{
    std::filesystem::path filePath = m_scenesDirectory / filename;
    std::ifstream file(filePath);
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
    if (file.is_open())
    {
        nlohmann::json j;
        try
        {
            file >> j;
            file.close();
            fmt::print("Scene loaded successfully from file: {}\n", filePath.string());
        }
        catch (const std::exception& e)
        {
            fmt::print(stderr, "Error: Failed to parse JSON from file: {}. Exception: {}\n", filePath.string(), e.what());
            file.close();
            return;
        }

        // Odczytanie directionalLight z pliku JSON
        if (j.contains("directionalLight"))
        {
            nlohmann::json directionalLight = j["directionalLight"];
            try
            {
                m_light.direction.x = directionalLight["direction"][0];
                m_light.direction.y = directionalLight["direction"][1];
                m_light.direction.z = directionalLight["direction"][2];
                m_light.color.x = directionalLight["color"][0];
                m_light.color.y = directionalLight["color"][1];
                m_light.color.z = directionalLight["color"][2];
                m_light.color.w = directionalLight["intensity"];
            }
            catch (const std::exception& e)
            {
                fmt::print(stderr, "Error: Failed to read directionalLight data from JSON. Exception: {}\n", e.what());
                return;
            }
        }
        else
        {
            fmt::print(stderr, "Warning: No directionalLight found in JSON.\n");
        }
        m_sceneObjectManager->removeAllObjects();
        m_sceneObjectManager->from_json(j["sceneObjects"]);
    }
    else
    {
        fmt::print(stderr, "Error: Unable to open file for loading: {}\n", filePath.string());
    }
}

void Scene::drawInterface() {
    //ImGui::Begin("Scene Manager");
    if (ImGui::BeginTabItem("Scene Manager")) {

        static char saveFilename[128] = "scene";
        ImGui::InputText("Save Filename", saveFilename, IM_ARRAYSIZE(saveFilename));

        if (ImGui::Button("Save")) {
            saveScene(saveFilename);
        }

        ImGui::Separator();

        static std::string selectedLoadFilename;
        static std::string selectedLoadFilenameWithExtension;
        bool hasFiles = false;

        // Check for files in the directory and set the first one as the default if none is selected
        for (const auto& entry : std::filesystem::directory_iterator(m_scenesDirectory)) {
            if (entry.is_regular_file()) {
                const std::string filenameWithExtension = entry.path().filename().string();
                const std::string filename = entry.path().stem().string(); // Get filename without extension

                if (!hasFiles) {
                    selectedLoadFilename = filename;
                    selectedLoadFilenameWithExtension = filenameWithExtension;
                    hasFiles = true;
                }
            }
        }

        if (hasFiles) {
            if (ImGui::BeginCombo("Load Scene", selectedLoadFilename.c_str())) {
                for (const auto& entry : std::filesystem::directory_iterator(m_scenesDirectory)) {
                    if (entry.is_regular_file()) {
                        const std::string filenameWithExtension = entry.path().filename().string();
                        const std::string filename = entry.path().stem().string(); // Get filename without extension

                        if (ImGui::Selectable(filename.c_str(), selectedLoadFilename == filename)) {
                            selectedLoadFilename = filename;
                            selectedLoadFilenameWithExtension = filenameWithExtension;
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }
        else {
            ImGui::Text("No scenes available to load.");
        }

        if (!selectedLoadFilename.empty() && ImGui::Button("Load")) {
            m_loadScene = true;
            m_path = selectedLoadFilenameWithExtension;
            //loadScene(selectedLoadFilenameWithExtension); // Load using full filename with extension
        }

        ImGui::Separator();

        // Modify DirectionalLight parameters
        ImGui::Text("Directional Light");
        ImGui::SliderFloat3("Direction", &m_light.direction[0], -1.0f, 1.0f);
        ImGui::ColorEdit4("Color", &m_light.color[0]);

        // Modify ambient coefficient
        ImGui::SliderFloat("Ambient Coefficient", &m_ambientCoefficient, 0.0f, 1.0f);

        if (ImGui::Button("Reset All Objects")) {
            m_sceneObjectManager->resetAllObjects();
        }

        m_sceneObjectManager->drawInterface();

        ImGui::Separator();
        ImGui::EndTabItem();
    }

    //ImGui::End();
}


void Scene::draw()
{
    GraphicsEngine::get()->getRenderer()->bindDescriptorSet(m_globalDescriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()], 0);
    m_sceneObjectManager->drawObjects();
}
