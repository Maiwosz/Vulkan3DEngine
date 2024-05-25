#include "SceneObjectManager.h"
#include "Animation.h"
#include "AnimationSequence.h"
#include "AnimationBuilder.h"
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

void SceneObjectManager::drawInterface()
{
    // Start a new ImGui window
    ImGui::Begin("Scene Object Manager");

    // Create tabs for Object Management and Object Creation
    if (ImGui::BeginTabBar("TabBar"))
    {
        if (ImGui::BeginTabItem("Manage Objects"))
        {
            // Loop over all objects in the scene
            for (size_t i = 0; i < m_objects.size(); ++i)
            {
                auto& object = m_objects[i];

                // Create a unique ID for each object
                std::string headerLabel = object->getName() + "##" + std::to_string(i);

                // Create a collapsible header for each object with a unique ID
                if (ImGui::CollapsingHeader(headerLabel.c_str()))
                {
                    // Allow the user to adjust the position, rotation, and scale of the object
                    char name[256];
                    std::string objectName = object->getName();
                    strncpy_s(name, objectName.c_str(), sizeof(name));
                    name[sizeof(name) - 1] = 0; // Null-terminate just in case strncpy hit the limit
                    glm::vec3 position = object->getPosition();
                    glm::vec3 rotation = object->getRotation();
                    float scale = object->getScale();

                    if (ImGui::InputText("Name", name, IM_ARRAYSIZE(name))) {
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            object->setName(std::string(name));
                        }
                    }
                    if (ImGui::DragFloat3("Position", &position[0])) object->setPosition(position);
                    if (ImGui::DragFloat3("Rotation", &rotation[0])) object->setRotation(rotation);
                    if (ImGui::DragFloat("Scale", &scale)) object->setScale(scale);

                    


                    // If the object is a Model, allow the user to adjust model-specific properties
                    if (Model* model = dynamic_cast<Model*>(object.get()))
                    {
                        // Allow the user to adjust the position, rotation, and scale of the object
                        glm::vec3 positionOffset = model->getPositionOffset();
                        glm::vec3 rotationOffset = model->getRotationOffset();
                        float scaleOffset = model->getScaleOffset();
                        // Allow the user to adjust the shininess, kd, and ks of the model
                        float shininess = model->m_shininess;
                        float kd = model->m_kd;
                        float ks = model->m_ks;

                        if (ImGui::DragFloat3("Initial Position", &positionOffset[0])) model->setPositionOffset(positionOffset);
                        if (ImGui::DragFloat3("Initial Rotation", &rotationOffset[0])) model->setRotationOffset(rotationOffset);
                        if (ImGui::DragFloat("Initial Scale", &scaleOffset)) model->setScaleOffset(scaleOffset);

                        if (ImGui::DragFloat("Shininess", &shininess)) model->m_shininess = shininess;
                        if (ImGui::DragFloat("Kd", &kd)) model->m_kd = kd;
                        if (ImGui::DragFloat("Ks", &ks)) model->m_ks = ks;

                        // Allow the user to select a mesh for the model
                        if (ImGui::BeginCombo("Mesh", model->m_mesh->getName().string().c_str()))
                        {
                            for (const auto& meshName : GraphicsEngine::get()->getMeshManager()->getAllResources())
                            {
                                bool isSelected = (model->m_mesh->getName() == meshName);
                                if (ImGui::Selectable(meshName.string().c_str(), isSelected))
                                {
                                    auto futureMesh = ThreadPool::get()->enqueue([meshName]() -> MeshPtr {
                                        return GraphicsEngine::get()->getMeshManager()->loadMesh(meshName);
                                        });

                                    std::thread([model, futureMesh = std::move(futureMesh)]() mutable {
                                        MeshPtr newMesh = futureMesh.get();
                                        vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
                                        model->setMesh(newMesh);
                                        }).detach();
                                }
                                if (isSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }

                        // Allow the user to select a texture for the model
                        if (ImGui::BeginCombo("Texture", model->m_texture->getName().string().c_str()))
                        {
                            for (const auto& textureName : GraphicsEngine::get()->getTextureManager()->getAllResources())
                            {
                                bool isSelected = (model->m_texture->getName() == textureName);
                                if (ImGui::Selectable(textureName.string().c_str(), isSelected))
                                {
                                    auto futureTexture = ThreadPool::get()->enqueue([textureName]() -> TexturePtr {
                                        return GraphicsEngine::get()->getTextureManager()->loadTexture(textureName);
                                        });

                                    std::thread([model, futureTexture = std::move(futureTexture)]() mutable {
                                        TexturePtr newTexture = futureTexture.get();
                                        vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
                                        model->setTexture(newTexture);
                                        }).detach();
                                }
                                if (isSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }

                        // Store the file path and modal state
                        static std::filesystem::path fileToSave;
                        static bool showModal = false;

                        // Allow the user to save the model data to a file
                        if (ImGui::Button("Save Model Data")) {

                            // Construct the full path for the new file
                            fileToSave = std::filesystem::path("Assets") / "Models" / (model->m_name + ".json");

                            // Check if the file already exists
                            if (std::filesystem::exists(fileToSave)) {
                                // Open the popup
                                showModal = true;
                                ImGui::OpenPopup("File already exists");
                            }
                            else {
                                // The file doesn't exist, so we can just write to it
                                model->writeModelDataToFile(fileToSave);
                            }
                        }

                        // Create the popup
                        if (showModal) {
                            if (ImGui::BeginPopupModal("File already exists", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                                ImGui::Text("A file with this name already exists. Do you want to overwrite it?");
                                if (ImGui::Button("Yes")) {
                                    // Overwrite the file
                                    model->writeModelDataToFile(fileToSave);
                                    showModal = false;
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("No")) {
                                    // Don't overwrite the file
                                    showModal = false;
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::EndPopup();
                            }
                        }
                    }

                    // If the object is a PointLightObject, allow the user to adjust light-specific properties
                    if (PointLightObject* light = dynamic_cast<PointLightObject*>(object.get()))
                    {
                        // Allow the user to adjust the color and intensity of the light
                        glm::vec3 color = glm::vec3(light->m_light->color);
                        float intensity = light->m_light->color.w;

                        if (ImGui::ColorEdit3("Color", &color[0])) light->m_light->color = glm::vec4(color, 1.0f);
                        if (ImGui::DragFloat("Intensity", &intensity)) light->m_light->color.w = intensity;
                    }

                    // Add a button for opening the animation sequence editor
                    if (ImGui::Button("Edit Animation Sequence"))
                    {
                        ImGui::OpenPopup("Animation Sequence Editor");
                        m_selectedObject = object.get();
                    }

                    // Render the animation sequence editor modal window
                    if (ImGui::BeginPopupModal("Animation Sequence Editor", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        static int selectedAnimationIndex = -1;
                        static int animationType = 0; // 0: Move, 1: Rotate, 2: Orbit, 3: Wait
                        static glm::vec3 startPosition = { 0.0f, 0.0f, 0.0f };
                        static glm::vec3 endPosition = { 0.0f, 0.0f, 0.0f };
                        static glm::vec3 startRotation = { 0.0f, 0.0f, 0.0f };
                        static glm::vec3 endRotation = { 0.0f, 0.0f, 0.0f };
                        static glm::vec3 center = { 0.0f, 0.0f, 0.0f };
                        static float radius = 0.0f;
                        static float angularSpeed = 0.0f;
                        static float duration = 0.0f;
                        static glm::vec3 axis = { 0.0f, 1.0f, 0.0f };
                        static float phaseShift = 0.0f;

                        // Check if the selected object has an animation sequence
                        auto animationSequence = m_selectedObject->getAnimationSequence();
                        if (animationSequence)
                        {
                            // Existing animations section
                            ImGui::Text("Existing Animations");
                            ImGui::Separator();
                            for (size_t j = 0; j < animationSequence->m_animations.size(); ++j)
                            {
                                if (ImGui::Selectable(("Animation " + std::to_string(j)).c_str(), selectedAnimationIndex == j))
                                {
                                    selectedAnimationIndex = j;

                                    // Load the current animation parameters into temporary variables
                                    auto& anim = animationSequence->m_animations[selectedAnimationIndex];
                                    duration = anim.m_duration;
                                }
                            }

                            // Editing selected animation section
                            if (selectedAnimationIndex >= 0 && selectedAnimationIndex < animationSequence->m_animations.size())
                            {
                                ImGui::Text("Editing Animation %d", selectedAnimationIndex);
                                ImGui::Separator();

                                if (ImGui::DragFloat("Duration", &duration))
                                {
                                    // Update the animation duration
                                    animationSequence->m_animations[selectedAnimationIndex].m_duration = duration;
                                }

                                // Options to move selected animation up or down
                                if (selectedAnimationIndex > 0)
                                {
                                    if (ImGui::Button("Move Up"))
                                    {
                                        std::swap(animationSequence->m_animations[selectedAnimationIndex], animationSequence->m_animations[selectedAnimationIndex - 1]);
                                        selectedAnimationIndex--;
                                    }
                                }
                                if (selectedAnimationIndex < animationSequence->m_animations.size() - 1)
                                {
                                    if (ImGui::Button("Move Down"))
                                    {
                                        std::swap(animationSequence->m_animations[selectedAnimationIndex], animationSequence->m_animations[selectedAnimationIndex + 1]);
                                        selectedAnimationIndex++;
                                    }
                                }

                                // Option to remove selected animation
                                if (ImGui::Button("Remove Animation"))
                                {
                                    animationSequence->m_animations.erase(animationSequence->m_animations.begin() + selectedAnimationIndex);
                                    selectedAnimationIndex = -1;
                                }
                            }

                            // Adding new animation section
                            ImGui::Text("Add New Animation");
                            ImGui::Separator();

                            const char* animationTypes[] = { "Move", "Rotate", "Orbit", "Wait" };
                            ImGui::Combo("Animation Type", &animationType, animationTypes, IM_ARRAYSIZE(animationTypes));

                            switch (animationType)
                            {
                            case 0: // Move
                                ImGui::InputFloat3("Start Position", &startPosition[0]);
                                ImGui::InputFloat3("End Position", &endPosition[0]);
                                ImGui::InputFloat("Duration", &duration);
                                if (ImGui::Button("Add Move Animation"))
                                {
                                    AnimationBuilder builder;
                                    builder.move(m_selectedObject, startPosition, endPosition, duration);
                                    animationSequence->addAnimation(builder.build()->m_animations.back());

                                    // Reset input fields after adding
                                    startPosition = { 0.0f, 0.0f, 0.0f };
                                    endPosition = { 0.0f, 0.0f, 0.0f };
                                    duration = 0.0f;
                                }
                                break;
                            case 1: // Rotate
                                ImGui::InputFloat3("Start Rotation", &startRotation[0]);
                                ImGui::InputFloat3("End Rotation", &endRotation[0]);
                                ImGui::InputFloat("Duration", &duration);
                                if (ImGui::Button("Add Rotate Animation"))
                                {
                                    AnimationBuilder builder;
                                    builder.rotate(m_selectedObject, startRotation, endRotation, duration);
                                    animationSequence->addAnimation(builder.build()->m_animations.back());

                                    // Reset input fields after adding
                                    startRotation = { 0.0f, 0.0f, 0.0f };
                                    endRotation = { 0.0f, 0.0f, 0.0f };
                                    duration = 0.0f;
                                }
                                break;
                            case 2: // Orbit
                                ImGui::InputFloat3("Center", &center[0]);
                                ImGui::InputFloat("Radius", &radius);
                                ImGui::InputFloat("Angular Speed", &angularSpeed);
                                ImGui::InputFloat("Duration", &duration);
                                ImGui::InputFloat3("Axis", &axis[0]);
                                ImGui::InputFloat("Phase Shift", &phaseShift);
                                if (ImGui::Button("Add Orbit Animation"))
                                {
                                    AnimationBuilder builder;
                                    builder.orbit(m_selectedObject, center, radius, angularSpeed, duration, axis, phaseShift);
                                    animationSequence->addAnimation(builder.build()->m_animations.back());

                                    // Reset input fields after adding
                                    center = { 0.0f, 0.0f, 0.0f };
                                    radius = 0.0f;
                                    angularSpeed = 0.0f;
                                    duration = 0.0f;
                                    axis = { 0.0f, 1.0f, 0.0f };
                                    phaseShift = 0.0f;
                                }
                                break;
                            case 3: // Wait
                                ImGui::InputFloat("Duration", &duration);
                                if (ImGui::Button("Add Wait Animation"))
                                {
                                    AnimationBuilder builder;
                                    builder.wait(m_selectedObject,duration);
                                    animationSequence->addAnimation(builder.build()->m_animations.back());

                                    // Reset input fields after adding
                                    duration = 0.0f;
                                }
                                break;
                            }
                        }
                        else
                        {
                            if (ImGui::Button("Create New Animation Sequence"))
                            {
                                m_selectedObject->setAnimationSequence(std::make_shared<AnimationSequence>());
                            }
                        }

                        if (ImGui::Button("Close"))
                        {
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::Separator();
                }
            }
            ImGui::EndTabItem();
        }


        if (ImGui::BeginTabItem("Create New Object"))
        {
            // Allow the user to select the type of the object
            const char* items[] = { "Model", "PointLightObject" };
            static int item_current = 0;
            ImGui::Combo("Object Type", &item_current, items, IM_ARRAYSIZE(items));

            // Depending on the selected type, allow the user to define the parameters of the object
            if (item_current == 0) // Model
            {

                static glm::vec3 position(0.0f);
                static glm::vec3 rotation(0.0f);
                static float scale = 1.0f;

                // Allow the user to select ModelData or choose custom
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
                    // User-defined parameters for a new model
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
                    ImGui::InputFloat3("Initial Position", &initialPosition[0]);
                    ImGui::InputFloat3("Initial Rotation", &initialRotation[0]);
                    ImGui::InputFloat("Initial Scale", &initialScale);
                    ImGui::InputFloat("Shininess", &shininess);
                    ImGui::InputFloat("Kd", &kd);
                    ImGui::InputFloat("Ks", &ks);

                    // Allow the user to select a mesh for the model
                    if (ImGui::BeginCombo("Mesh", customMeshName.c_str()))
                    {
                        for (const auto& meshName : GraphicsEngine::get()->getMeshManager()->getAllResources())
                        {
                            if (ImGui::Selectable(meshName.string().c_str(), customMeshName == meshName))
                            {
                                customMeshName = meshName.string().c_str();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // Allow the user to select a texture for the model
                    if (ImGui::BeginCombo("Texture", customTextureName.c_str()))
                    {
                        for (const auto& textureName : GraphicsEngine::get()->getTextureManager()->getAllResources())
                        {
                            if (ImGui::Selectable(textureName.string().c_str(), customTextureName == textureName))
                            {
                                customTextureName = textureName.string().c_str();
                            }
                        }
                        ImGui::EndCombo();
                    }

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
                else
                {
                    ImGui::InputFloat3("Position", &position[0]);
                    ImGui::InputFloat3("Rotation", &rotation[0]);
                    ImGui::InputFloat("Scale", &scale);

                    if (ImGui::Button("Create Model"))
                    {
                        ModelPtr model = createModel(selectedModelData);
                        model->setPosition(position);
                        model->setRotation(rotation);
                        model->setScale(scale);
                    }
                }
            }
            else if (item_current == 1) // PointLightObject
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
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // End the ImGui window
    ImGui::End();
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

    int cameraCounter = 0;

    for (const auto& objJson : j) {
        if (!objJson.contains("type")) {
            fmt::print(stderr, "Error: Missing 'type' field in JSON object.\n");
            continue;
        }

        std::string type = objJson["type"];
        std::shared_ptr<SceneObject> object;

        try {
            if (type == "model") {
                futures.push_back(ThreadPool::get()->enqueue([this, objJson]()->SceneObjectPtr {
                    return std::make_shared<Model>(objJson, p_scene);
                }));
            }
            else if (type == "pointLight") {
                futures.push_back(ThreadPool::get()->enqueue([this, objJson]()->SceneObjectPtr {
                    return std::make_shared<PointLightObject>(objJson, p_scene);
                }));
            }
            else if (type == "camera") {
                if (cameraCounter>0) {
                    fmt::print(stderr, "Error: Camera has already been initialized. Only one camera can exist.\n");
                    continue;
                }
                cameraCounter++;
                futures.push_back(ThreadPool::get()->enqueue([this, objJson]()->SceneObjectPtr {
                    p_scene->m_camera->SceneObject::from_json(objJson["SceneObjectData"]);
                    p_scene->m_camera->updateCameraVectors();
                    SceneObjectPtr object = std::static_pointer_cast<SceneObject>(p_scene->m_camera);
                    return object;
                }));
            }
            else {
                fmt::print(stderr, "Error: Unknown object type: {}\n", type);
                continue;
            }
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize scene object of type {}. Exception: {}\n", type, e.what());
        }
    }

    // Poczekaj na zakoñczenie wszystkich operacji
    for (auto& future : futures) {
        try {
            auto object = future.get(); // Poczekaj na zakoñczenie
            if (object) {
                m_objects.push_back(object);
            }
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Exception while getting result from future: {}\n", e.what());
        }
    }
}






