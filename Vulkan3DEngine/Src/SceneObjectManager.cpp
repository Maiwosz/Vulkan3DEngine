#include "SceneObjectManager.h"
#include "Animation.h"
#include "AnimationSequence.h"
#include "AnimationBuilder.h"
#include <semaphore>
#include <future>

SceneObjectManager::SceneObjectManager(Scene* scene)
    : p_scene(scene) 
{

}

std::shared_ptr<Model> SceneObjectManager::createModel(const std::string& modelName)
{
    ModelPtr model = std::make_shared<Model>(GraphicsEngine::get()->getModelDataManager()->loadModelData(modelName), p_scene);
    m_objects.push_back(model);
    return model;
}

std::shared_ptr<Model> SceneObjectManager::createModel(const std::string name, const std::string meshName, const std::string textureName, const float scale, const float shininess, const float kd, const float ks, const float initialScale, const glm::vec3 initialPosition, const glm::vec3 initialRotation)
{
    ModelPtr model = std::make_shared<Model>(name, meshName, textureName, scale, shininess, kd, ks, initialScale, initialPosition, initialRotation, p_scene);
    m_objects.push_back(model);
    return model;
}

std::shared_ptr<PointLightObject> SceneObjectManager::createPointLight()
{
    PointLightObjectPtr light = std::make_shared<PointLightObject>(p_scene);
    m_objects.push_back(light);
    return light;
}

std::shared_ptr<PointLightObject> SceneObjectManager::createPointLight(glm::vec3 color, float intensity)
{
    PointLightObjectPtr light = std::make_shared<PointLightObject>(color, intensity, p_scene);
    m_objects.push_back(light);
    return light;
}

std::shared_ptr<PointLightObject> SceneObjectManager::createPointLight(float radius, glm::vec3 color, float intensity)
{
    PointLightObjectPtr light = std::make_shared<PointLightObject>(radius, color, intensity, p_scene);
    m_objects.push_back(light);
    return light;
}

std::shared_ptr<PointLightObject> SceneObjectManager::createPointLight(glm::vec3 position, float radius, glm::vec3 color, float intensity)
{
    PointLightObjectPtr light = std::make_shared<PointLightObject>(position, radius, color, intensity, p_scene);
    m_objects.push_back(light);
    return light;
}

std::shared_ptr<Camera> SceneObjectManager::createCamera(glm::vec3 startPosition, float startPitch, float startYaw)
{
    std::shared_ptr<Camera> camera = std::make_shared<Camera>(startPosition, startPitch, startYaw, p_scene);
    m_objects.push_back(camera);
    return camera;
}

void SceneObjectManager::updateObjects()
{
    auto it = m_objects.begin();
    while (it != m_objects.end()) {
        if ((*it)->isActive) {
            (*it)->update();
            ++it;
        }
        else {
            it = m_objects.erase(it);
        }
    }
}

void SceneObjectManager::drawObjects()
{
    for (auto& object : m_objects) {
        object->draw();
    }
}

void SceneObjectManager::removeObject(std::shared_ptr<SceneObject> object)
{
    object->isActive = false;
}

void SceneObjectManager::addObject(std::shared_ptr<SceneObject> object)
{
    m_objects.push_back(object);
}

void SceneObjectManager::resetAllObjects()
{
    for (auto& object : m_objects) {
        if (auto sequence = object->getAnimationSequence()) {
            sequence->resetAllAnimations();
        }
        object->setPosition(object->getStartPosition());
        object->setRotation(object->getStartRotation());
        object->setScale(object->getStartScale());
    }
}

void SceneObjectManager::drawInterface()
{
    if (ImGui::BeginTabBar("Scene Object Manager"))
    {
        if (ImGui::BeginTabItem("Manage Objects"))
        {
            drawManageObjectsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Create New Object"))
        {
            drawCreateNewObjectTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void SceneObjectManager::drawManageObjectsTab()
{
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        auto& object = m_objects[i];
        std::string headerLabel = object->getName() + "##" + std::to_string(i);

        // Ensure only one header is open at a time
        if (m_openedObjectIndex != static_cast<int>(i))
        {
            ImGui::SetNextItemOpen(false);
        }
        else
        {
            ImGui::SetNextItemOpen(true);
        }

        if (ImGui::CollapsingHeader(headerLabel.c_str()))
        {
            // Update the opened index
            if (m_openedObjectIndex != static_cast<int>(i))
            {
                m_openedObjectIndex = static_cast<int>(i);
            }

            drawObjectCommonProperties(object);
            if (Model* model = dynamic_cast<Model*>(object.get()))
            {
                drawModelSpecificProperties(model);
            }
            else if (PointLightObject* light = dynamic_cast<PointLightObject*>(object.get()))
            {
                drawLightSpecificProperties(light);
            }

            // Add button to manage animations
            if (ImGui::Button("Manage Animations"))
            {
                showAnimationSequenceWindow = true;
                m_selectedObject = object.get();
            }

            // Add button to remove object
            if (ImGui::Button("Remove Object"))
            {
                removeObject(object);
                //m_objects.erase(m_objects.begin() + i);  // Remove from the list
                //--i;  // Adjust index since we removed an element
                continue;  // Skip remaining code in this iteration
            }

            ImGui::Separator();
        }
        else
        {
            // Close previously opened header
            if (m_openedObjectIndex == static_cast<int>(i))
            {
                m_openedObjectIndex = -1;
            }
        }
    }

    if (showAnimationSequenceWindow && m_selectedObject)
    {
        drawManageAnimationsWindow();
    }
}


void SceneObjectManager::drawObjectCommonProperties(const std::shared_ptr<SceneObject>& object)
{
    char name[256];
    std::string objectName = object->getName();
    strncpy_s(name, objectName.c_str(), sizeof(name));
    name[sizeof(name) - 1] = 0;

    glm::vec3 position = object->getPosition();
    glm::vec3 rotation = object->getRotation();
    float scale = object->getScale();
    glm::vec3 startPosition = object->getStartPosition();
    glm::vec3 startRotation = object->getStartRotation();
    float startScale = object->getStartScale();

    if (ImGui::InputText("Name", name, IM_ARRAYSIZE(name))) {
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            object->setName(std::string(name));
        }
    }

    if (ImGui::DragFloat3("Position", &position[0])) object->setPosition(position);
    if (ImGui::Button("Set as Start##Position")) object->setStartPosition(position);

    if (ImGui::DragFloat3("Rotation", &rotation[0])) object->setRotation(rotation);
    if (ImGui::Button("Set as Start##Rotation")) object->setStartRotation(rotation);

    if (ImGui::DragFloat("Scale", &scale)) object->setScale(scale);
    if (ImGui::Button("Set as Start##Scale")) object->setStartScale(scale);

    if (ImGui::DragFloat3("Start Position", &startPosition[0])) object->setStartPosition(startPosition);
    if (ImGui::DragFloat3("Start Rotation", &startRotation[0])) object->setStartRotation(startRotation);
    if (ImGui::DragFloat("Start Scale", &startScale)) object->setStartScale(startScale);
}


void SceneObjectManager::drawModelSpecificProperties(Model* model)
{
    glm::vec3 positionOffset = model->getPositionOffset();
    glm::vec3 rotationOffset = model->getRotationOffset();
    float scaleOffset = model->getScaleOffset();
    float shininess = model->m_shininess;
    float kd = model->m_kd;
    float ks = model->m_ks;

    if (ImGui::DragFloat3("Position Offset", &positionOffset[0])) model->setPositionOffset(positionOffset);
    if (ImGui::DragFloat3("Rotation Offset", &rotationOffset[0])) model->setRotationOffset(rotationOffset);
    if (ImGui::DragFloat("Scale Offset", &scaleOffset)) model->setScaleOffset(scaleOffset);
    if (ImGui::DragFloat("Shininess", &shininess)) model->m_shininess = shininess;
    if (ImGui::DragFloat("Kd", &kd)) model->m_kd = kd;
    if (ImGui::DragFloat("Ks", &ks)) model->m_ks = ks;

    drawComboBox("Mesh", model->m_mesh->getName().string(), GraphicsEngine::get()->getMeshManager(),
        [model](const std::string& meshName) {
            auto futureMesh = ThreadPool::get()->enqueue([meshName]() -> MeshPtr {
                return GraphicsEngine::get()->getMeshManager()->loadMesh(meshName);
                });
            std::thread([model, futureMesh = std::move(futureMesh)]() mutable {
                MeshPtr newMesh = futureMesh.get();
                vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
                model->setMesh(newMesh);
                }).detach();
        });

    drawComboBox("Texture", model->m_texture->getName().string(), GraphicsEngine::get()->getTextureManager(),
        [model](const std::string& textureName) {
            auto futureTexture = ThreadPool::get()->enqueue([textureName]() -> TexturePtr {
                return GraphicsEngine::get()->getTextureManager()->loadTexture(textureName);
                });
            std::thread([model, futureTexture = std::move(futureTexture)]() mutable {
                TexturePtr newTexture = futureTexture.get();
                vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
                model->setTexture(newTexture);
                }).detach();
        });

    drawSaveModelDataButton(model);
}

void SceneObjectManager::drawLightSpecificProperties(PointLightObject* light)
{
    glm::vec3 color = glm::vec3(light->m_light->color);
    float intensity = light->m_light->color.w;

    if (ImGui::ColorEdit3("Color", &color[0])) light->m_light->color = glm::vec4(color, 1.0f);
    if (ImGui::DragFloat("Intensity", &intensity)) light->m_light->color.w = intensity;
}

template <typename ResourceManager>
void SceneObjectManager::drawComboBox(const char* label, const std::string& currentItem, std::shared_ptr<ResourceManager> manager, std::function<void(const std::string&)> onSelect)
{
    if (ImGui::BeginCombo(label, currentItem.c_str()))
    {
        for (const auto& itemName : manager->getAllResources())
        {
            bool isSelected = (currentItem == itemName.string());
            if (ImGui::Selectable(itemName.string().c_str(), isSelected))
            {
                onSelect(itemName.string());
            }
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void SceneObjectManager::drawManageAnimationsWindow()
{
    if (ImGui::Begin("Manage Animations", &showAnimationSequenceWindow))
    {
        if (!m_selectedObject) {
            ImGui::Text("No object selected.");
            ImGui::End();
            return;
        }

        auto sequence = m_selectedObject->getAnimationSequence();
        if (!sequence)
        {
            if (ImGui::Button("Create New Animation Sequence"))
            {
                sequence = std::make_shared<AnimationSequence>();
                m_selectedObject->setAnimationSequence(sequence);
            }
        }
        else
        {
            ImGui::Text("Animation Sequence:");

            if (ImGui::Checkbox("Loop", &sequence->m_loop)) {
                sequence->setLoop(sequence->m_loop);
            }

            for (size_t i = 0; i < sequence->m_animations.size(); ++i)
            {
                Animation& animation = sequence->m_animations[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("Animation %zu", i);

                ImGui::DragFloat("Duration", &animation.m_duration, 0.1f, 0.0f, 100.0f);

                auto data = animation.getData();
                if (auto moveData = std::dynamic_pointer_cast<MoveAnimationData>(data)) {
                    ImGui::DragFloat3("Start Position", &moveData->startPosition[0]);
                    ImGui::DragFloat3("End Position", &moveData->endPosition[0]);
                }
                else if (auto rotateData = std::dynamic_pointer_cast<RotateAnimationData>(data)) {
                    ImGui::DragFloat3("Start Rotation", &rotateData->startRotation[0]);
                    ImGui::DragFloat3("End Rotation", &rotateData->endRotation[0]);
                }
                else if (auto orbitData = std::dynamic_pointer_cast<OrbitAnimationData>(data)) {
                    ImGui::DragFloat3("Center", &orbitData->center[0]);
                    ImGui::DragFloat("Radius", &orbitData->radius);
                    ImGui::DragFloat("Angular Speed", &orbitData->angularSpeed);
                    ImGui::DragFloat3("Axis", &orbitData->axis[0]);
                    ImGui::DragFloat("Phase Shift", &orbitData->phaseShift);
                }
                else if (std::dynamic_pointer_cast<WaitAnimationData>(data)) {
                    ImGui::Text("Wait Animation");
                    // No parameters to change for wait animation
                }

                if (ImGui::Button("Remove Animation"))
                {
                    sequence->m_animations.erase(sequence->m_animations.begin() + i);
                    ImGui::PopID();
                    continue;
                }

                if (i > 0 && ImGui::Button("Move Up")) {
                    std::swap(sequence->m_animations[i], sequence->m_animations[i - 1]);
                }

                if (i < sequence->m_animations.size() - 1 && ImGui::Button("Move Down")) {
                    std::swap(sequence->m_animations[i], sequence->m_animations[i + 1]);
                }

                ImGui::PopID();
                ImGui::Separator();
            }

            if (ImGui::Button("Add New Animation"))
            {
                ImGui::OpenPopup("Add Animation Popup");
            }

            if (ImGui::BeginPopup("Add Animation Popup"))
            {
                static int animationType = 0;
                static glm::vec3 startPosition, endPosition, startRotation, endRotation, center, axis;
                static float duration, radius, angularSpeed, phaseShift;

                ImGui::RadioButton("Move", &animationType, 0);
                if (animationType == 0) {
                    ImGui::InputFloat3("Start Position", &startPosition[0]);
                    ImGui::InputFloat3("End Position", &endPosition[0]);
                    ImGui::InputFloat("Duration", &duration);
                }

                ImGui::RadioButton("Rotate", &animationType, 1);
                if (animationType == 1) {
                    ImGui::InputFloat3("Start Rotation", &startRotation[0]);
                    ImGui::InputFloat3("End Rotation", &endRotation[0]);
                    ImGui::InputFloat("Duration", &duration);
                }

                ImGui::RadioButton("Orbit", &animationType, 2);
                if (animationType == 2) {
                    ImGui::InputFloat3("Center", &center[0]);
                    ImGui::InputFloat("Radius", &radius);
                    ImGui::InputFloat("Angular Speed", &angularSpeed);
                    ImGui::InputFloat3("Axis", &axis[0]);
                    ImGui::InputFloat("Phase Shift", &phaseShift);
                    ImGui::InputFloat("Duration", &duration);
                }

                ImGui::RadioButton("Wait", &animationType, 3);
                if (animationType == 3) {
                    ImGui::InputFloat("Duration", &duration);
                }

                if (ImGui::Button("Add Animation")) {
                    AnimationBuilder builder;
                    switch (animationType) {
                    case 0:
                        builder.move(m_selectedObject, startPosition, endPosition, duration);
                        break;
                    case 1:
                        builder.rotate(m_selectedObject, startRotation, endRotation, duration);
                        break;
                    case 2:
                        builder.orbit(m_selectedObject, center, radius, angularSpeed, duration, axis, phaseShift);
                        break;
                    case 3:
                        builder.wait(m_selectedObject, duration);
                        break;
                    }
                    builder.build(*sequence);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

        }

        ImGui::End();
    }
}



void SceneObjectManager::drawSaveModelDataButton(Model* model)
{
    static std::filesystem::path fileToSave;
    static bool showModal = false;

    if (ImGui::Button("Save Model Data"))
    {
        fileToSave = std::filesystem::path("Assets") / "Models" / (model->m_name + ".json");

        if (std::filesystem::exists(fileToSave))
        {
            showModal = true;
            ImGui::OpenPopup("File already exists");
        }
        else
        {
            model->writeModelDataToFile(fileToSave);
        }
    }

    if (showModal)
    {
        if (ImGui::BeginPopupModal("File already exists", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("A file with this name already exists. Do you want to overwrite it?");
            if (ImGui::Button("Yes"))
            {
                model->writeModelDataToFile(fileToSave);
                showModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No"))
            {
                showModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void SceneObjectManager::drawCreateNewObjectTab()
{
    const char* items[] = { "Model", "PointLightObject" };
    static int item_current = 0;
    ImGui::Combo("Object Type", &item_current, items, IM_ARRAYSIZE(items));

    if (item_current == 0) // Model
    {
        drawCreateModelUI();
    }
    else if (item_current == 1) // PointLightObject
    {
        drawCreatePointLightUI();
    }
}

void SceneObjectManager::drawCreateModelUI()
{
    static glm::vec3 position(0.0f);
    static glm::vec3 rotation(0.0f);
    static float scale = 1.0f;

    static std::string selectedModelData = "Custom";
    if (ImGui::BeginCombo("Model Data", selectedModelData.c_str()))
    {
        if (ImGui::Selectable("Custom", selectedModelData == "Custom"))
        {
            selectedModelData = "Custom";
        }
        for (const auto& modelDataName : GraphicsEngine::get()->getModelDataManager()->getAllResources())
        {
            if (ImGui::Selectable(modelDataName.string().c_str(), selectedModelData == modelDataName))
            {
                selectedModelData = modelDataName.string().c_str();
            }
        }
        ImGui::EndCombo();
    }

    if (selectedModelData == "Custom")
    {
        drawCustomModelCreationUI(position, rotation, scale);
    }
    else
    {
        drawModelCreationUI(selectedModelData, position, rotation, scale);
    }
}

void SceneObjectManager::drawCustomModelCreationUI(glm::vec3& position, glm::vec3& rotation, float& scale)
{
    static char customName[256] = "";
    static std::string customMeshName;
    static std::string customTextureName;
    static glm::vec3 initialPosition(0.0f);
    static glm::vec3 initialRotation(0.0f);
    static float initialScale = 1.0f;
    static float shininess = 1.0f;
    static float kd = 0.8f;
    static float ks = 0.2f;

    ImGui::InputText("Name", customName, IM_ARRAYSIZE(customName));
    ImGui::InputFloat3("Start Position", &initialPosition[0]);
    ImGui::InputFloat3("Start Rotation", &initialRotation[0]);
    ImGui::InputFloat("Start Scale", &initialScale);
    ImGui::InputFloat("Shininess", &shininess);
    ImGui::InputFloat("Kd", &kd);
    ImGui::InputFloat("Ks", &ks);

    drawComboBox("Mesh", customMeshName, GraphicsEngine::get()->getMeshManager(),
        [](const std::string& meshName) { customMeshName = meshName; });

    drawComboBox("Texture", customTextureName, GraphicsEngine::get()->getTextureManager(),
        [](const std::string& textureName) { customTextureName = textureName; });

    ImGui::InputFloat3("Position", &position[0]);
    ImGui::InputFloat3("Rotation", &rotation[0]);
    ImGui::InputFloat("Scale", &scale);

    if (ImGui::Button("Create Model"))
    {
        if (!customMeshName.empty() && !customTextureName.empty())
        {
            ModelPtr model = createModel(customName, customMeshName, customTextureName, scale, shininess, kd, ks, scale, position, rotation);
            model->setPosition(position);
            model->setRotation(rotation);
            model->setScale(scale);
        }
    }
}


void SceneObjectManager::drawModelCreationUI(const std::string& modelDataName, glm::vec3& position, glm::vec3& rotation, float& scale)
{
    ImGui::InputFloat3("Position", &position[0]);
    ImGui::InputFloat3("Rotation", &rotation[0]);
    ImGui::InputFloat("Scale", &scale);

    if (ImGui::Button("Create Model"))
    {
        ModelPtr model = createModel(modelDataName);
        model->setPosition(position);
        model->setRotation(rotation);
        model->setScale(scale);
    }
}

void SceneObjectManager::drawCreatePointLightUI()
{
    static glm::vec3 position(0.0f);
    static float radius = 0.2f;
    static glm::vec3 color(1.0f);
    static float intensity = 1.0f;

    ImGui::InputFloat3("Position", &position[0]);
    ImGui::InputFloat("Radius", &radius);
    ImGui::ColorEdit3("Color", &color[0]);
    ImGui::InputFloat("Intensity", &intensity);

    if (ImGui::Button("Create PointLightObject"))
    {
        createPointLight(position, radius, color, intensity);
    }
}

void SceneObjectManager::to_json(nlohmann::json& j) {
    nlohmann::json sceneObjectsJson;
    std::unordered_map<std::string, int> nameCount;
    for (const auto& object : m_objects) {
        try
        {
            nlohmann::json objJson;
            object->to_json(objJson);
            std::string name = object->getName();
            if (nameCount.find(name) != nameCount.end()) {
                nameCount[name]++;
            }
            else {
                nameCount[name] = 1;
            }
            name += "_" + std::to_string(nameCount[name]);
            sceneObjectsJson[name] = objJson;
        }
        catch (const std::exception& e)
        {
            fmt::print(stderr, "Error: Failed to serialize scene object. Exception: {}\n", e.what());
        }
    }
    j["sceneObjects"] = sceneObjectsJson;
}

void SceneObjectManager::from_json(const nlohmann::json& j) {
    std::vector<std::future<SceneObjectPtr>> futures;
    bool cameraInitialized = false;
    const int max_concurrent_objects = 8; // Ustalony limit liczby jednoczeœnie tworzonych obiektów
    std::counting_semaphore<max_concurrent_objects> semaphore(max_concurrent_objects);

    for (const auto& objJson : j) {
        if (!objJson.contains("type")) {
            fmt::print(stderr, "Error: Missing 'type' field in JSON object.\n");
            continue;
        }

        std::string type = objJson["type"];
        std::shared_ptr<SceneObject> object;
        bool multithreaded = true;

        try {
            if (multithreaded) {
                if (type == "model") {
                    semaphore.acquire();
                    futures.push_back(ThreadPool::get()->enqueue([this, objJson, &semaphore]()->SceneObjectPtr {
                        auto obj = std::make_shared<Model>(objJson, p_scene);
                        semaphore.release();
                        return obj;
                        }));
                }
                else if (type == "pointLight") {
                    semaphore.acquire();
                    futures.push_back(ThreadPool::get()->enqueue([this, objJson, &semaphore]()->SceneObjectPtr {
                        auto obj = std::make_shared<PointLightObject>(objJson, p_scene);
                        semaphore.release();
                        return obj;
                        }));
                }
                else if (type == "camera") {
                    if (cameraInitialized) {
                        fmt::print(stderr, "Error: Camera has already been initialized. Only one camera can exist.\n");
                        continue;
                    }
                    cameraInitialized = true;
                    semaphore.acquire();
                    futures.push_back(ThreadPool::get()->enqueue([this, objJson, &semaphore]()->SceneObjectPtr {
                        p_scene->m_camera->SceneObject::from_json(objJson["SceneObjectData"]);
                        p_scene->m_camera->updateCameraVectors();
                        auto obj = std::static_pointer_cast<SceneObject>(p_scene->m_camera);
                        semaphore.release();
                        return obj;
                        }));
                }
                else {
                    fmt::print(stderr, "Error: Unknown object type: {}\n", type);
                    continue;
                }
            }
            else {
                if (type == "model") {
                    object = std::make_shared<Model>(objJson, p_scene);
                }
                else if (type == "pointLight") {
                    object = std::make_shared<PointLightObject>(objJson, p_scene);
                }
                else if (type == "camera") {
                    if (cameraInitialized) {
                        fmt::print(stderr, "Error: Camera has already been initialized. Only one camera can exist.\n");
                        continue;
                    }
                    cameraInitialized = true;
                    p_scene->m_camera->SceneObject::from_json(objJson["SceneObjectData"]);
                    p_scene->m_camera->updateCameraVectors();
                    object = std::static_pointer_cast<SceneObject>(p_scene->m_camera);
                }
                else {
                    fmt::print(stderr, "Error: Unknown object type: {}\n", type);
                    continue;
                }

                if (object) {
                    m_objects.push_back(object);
                }
            }
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize scene object of type {}. Exception: {}\n", type, e.what());
        }
    }

    for (auto& future : futures) {
        try {
            auto object = future.get();
            if (object) {
                m_objects.push_back(object);
            }
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Exception while getting result from future: {}\n", e.what());
        }
    }
}






