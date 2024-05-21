#include "SceneObjectManager.h"
#include "Animation.h"
#include "AnimationSequence.h"
#include <future>

SceneObjectManager::SceneObjectManager(Scene* scene)
    : p_scene(scene) 
{

}

std::shared_ptr<Model> SceneObjectManager::createModel(const std::string& name)
{
    ModelPtr model = std::make_shared<Model>(GraphicsEngine::get()->getModelDataManager()->loadModelData(name), p_scene);
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
                std::string headerLabel = object->m_name + "##" + std::to_string(i);

                // Create a collapsible header for each object with a unique ID
                if (ImGui::CollapsingHeader(headerLabel.c_str()))
                {
                    // Allow the user to adjust the position, rotation, and scale of the object
                    glm::vec3 position = object->getPosition();
                    glm::vec3 rotation = object->getRotation();
                    float scale = object->getScale();

                    if (ImGui::DragFloat3("Position", &position[0])) object->setPosition(position);
                    if (ImGui::DragFloat3("Rotation", &rotation[0])) object->setRotation(rotation);
                    if (ImGui::DragFloat("Scale", &scale)) object->setScale(scale);

                    // If the object is a Model, allow the user to adjust model-specific properties
                    if (Model* model = dynamic_cast<Model*>(object.get()))
                    {
                        // Allow the user to adjust the shininess, kd, and ks of the model
                        float shininess = model->m_shininess;
                        float kd = model->m_kd;
                        float ks = model->m_ks;

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

                        // Allow the user to save the model data to a file
                        if (ImGui::Button("Save Model Data")) model->saveModelData();
                    }

                    // If the object is a PointLightObject, allow the user to adjust light-specific properties
                    if (PointLightObject* light = dynamic_cast<PointLightObject*>(object.get()))
                    {
                        // Allow the user to adjust the color and intensity of the light
                        glm::vec3 color = glm::vec3(light->m_light->color);
                        float intensity = light->m_light->color.w;

                        if (ImGui::ColorEdit3("Color", &color[0])) {
                            light->m_light->color = glm::vec4(color, light->m_light->color.w);
                        }
                        if (ImGui::DragFloat("Intensity", &intensity)) {
                            light->m_light->color.w = intensity;
                        }
                    }

                    // Add button to manage animations
                    std::string animationButtonLabel = "Manage Animations##" + std::to_string(i);
                    if (ImGui::Button(animationButtonLabel.c_str()))
                    {
                        ImGui::OpenPopup("Animation Manager");
                        m_selectedObject = object.get(); // Store the selected object for animation management
                    }

                    // Create the modal window for managing animations
                    if (ImGui::BeginPopupModal("Animation Manager", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        renderAnimationManager(m_selectedObject);
                        if (ImGui::Button("Close")) {
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }

                    std::string buttonLabel = "Remove Object";
                    if (ImGui::Button(buttonLabel.c_str()))
                    {
                        removeObject(object);
                    }
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
                static float shininess = 1.0f;
                static float kd = 1.0f;
                static float ks = 1.0f;

                ImGui::InputFloat3("Position", &position[0]);
                ImGui::InputFloat3("Rotation", &rotation[0]);
                ImGui::InputFloat("Scale", &scale);
                ImGui::InputFloat("Shininess", &shininess);
                ImGui::InputFloat("Kd", &kd);
                ImGui::InputFloat("Ks", &ks);

                if (ImGui::Button("Create Model"))
                {
                    // Create a new Model object with the specified parameters
                    // Add the new object to m_objects
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

void SceneObjectManager::renderAnimationManager(SceneObject* object)
{
    if (!object) return;

    // Display existing animations
    auto animationSequence = object->getAnimationSequence();
    if (animationSequence)
    {
        for (size_t i = 0; i < animationSequence->m_animations.size(); ++i)
        {
            auto& animation = animationSequence->m_animations[i];
            std::string animationLabel = "Animation " + std::to_string(i + 1);

            if (ImGui::CollapsingHeader(animationLabel.c_str()))
            {
                ImGui::Text("Duration: %.2f seconds", animation.m_duration);
                //ImGui::Checkbox("Loop", &animation.m_loop);
                if (ImGui::Button(("Remove##" + std::to_string(i)).c_str()))
                {
                    animationSequence->m_animations.erase(animationSequence->m_animations.begin() + i);
                }
            }
        }
    }

    // Button to add a new animation
    if (ImGui::Button("Add New Animation"))
    {
        ImGui::OpenPopup("New Animation");
    }

    // Modal window to add a new animation
    if (ImGui::BeginPopupModal("New Animation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        glm::vec3 startPos(0.0f, 0.0f, 0.0f);
        glm::vec3 endPos(0.0f, 0.0f, 0.0f);

        glm::vec3 startRot(0.0f, 0.0f, 0.0f);
        glm::vec3 endRot(0.0f, 0.0f, 0.0f);

        static float duration = 1.0f;
        static bool loop = false;

        if (ImGui::DragFloat3("Start Position", &startPos[0]) &&
            ImGui::DragFloat3("End Position", &endPos[0]) &&
            ImGui::DragFloat3("Start Rotation", &startRot[0]) &&
            ImGui::DragFloat3("End Rotation", &endRot[0]) &&
            ImGui::DragFloat("Duration", &duration) &&
            ImGui::Checkbox("Loop", &loop))
        {
            if (ImGui::Button("Create"))
            {
                auto moveFunc = [object, startPos, endPos](float t) {
                    glm::vec3 newPos = glm::mix(startPos, endPos, t);
                    object->setPosition(newPos);
                    };

                auto rotateFunc = [object, startRot, endRot](float t) {
                    glm::vec3 newRot = glm::mix(startRot, endRot, t);
                    object->setRotation(newRot);
                    };

                Animation moveAnimation(moveFunc, duration);
                Animation rotateAnimation(rotateFunc, duration);

                auto newSequence = std::make_shared<AnimationSequence>();
                newSequence->addAnimation(moveAnimation);
                newSequence->addAnimation(rotateAnimation);

                object->setAnimationSequence(newSequence);

                ImGui::CloseCurrentPopup();
            }
        }
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}



