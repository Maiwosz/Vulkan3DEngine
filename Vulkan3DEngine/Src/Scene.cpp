#include "Scene.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "LightComponent.h"
#include "CameraComponent.h"
#include "Engine.h"
#include "InputSystem.h"
#include <spdlog/spdlog.h>

Scene::Scene()
    : m_cameraRotationSpeed(20.0f),
    m_cameraYawOffset(0.0f),
    m_cameraPitchOffset(0.0f),
    m_cameraMovementSpeed(5.0f),
    m_movingForward(false),
    m_movingBackward(false),
    m_movingLeft(false),
    m_movingRight(false),
    m_movingUp(false),
    m_movingDown(false),
    m_mouseRightButtonDown(false),
    m_lastMousePos(0.0f),
    m_mouseSensitivity(0.1f)
{
    m_registry = std::make_unique<Registry>();
    m_registry->getSystemManager().registerSystem<AssetCollectionSystem>();
    m_registry->getSystemManager().registerSystem<MeshRenderSystem>();
    m_registry->getSystemManager().registerSystem<LightSystem>();
    m_registry->getSystemManager().registerSystem<CameraSystem>();

    // Creating test object with mesh
    testEntity = m_registry->create();
    {
        // Transform
        auto& transform = m_registry->addComponent<TransformComponent>(testEntity);
        transform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.setScale(glm::vec3(1.0f));

        // Mesh and material
        auto& mesh = m_registry->addComponent<MeshComponent>(testEntity);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, "FLora_C"));

        auto& material = m_registry->addComponent<MaterialComponent>(testEntity);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, "FLora_C"));
    }

    // Camera
    testCamera = m_registry->create();
    {
        auto& cameraTransform = m_registry->addComponent<TransformComponent>(testCamera);
        // Position camera further from object to ensure everything is visible
        cameraTransform.setPosition(glm::vec3(0.0f, 0.0f, 100.0f)); // Increased distance to 10 units
        cameraTransform.setRotation(glm::vec3(0.0f)); // Initial rotation

        auto& camera = m_registry->addComponent<CameraComponent>(testCamera);
        camera.setAspectRatio(16.0f / 9.0f);
        camera.setClippingPlanes(0.1f, 1000.0f);
        // Set wider field of view for better scene coverage
        camera.setVerticalFOV(70.0f);
    }

    // Directional light
    testDirectionalLight = m_registry->create();
    {
        auto& transform = m_registry->addComponent<TransformComponent>(testDirectionalLight);
        auto& light = m_registry->addComponent<LightComponent>(testDirectionalLight, LightComponent::Type::Directional);
        light.setDirection(glm::vec3(0.5f, -1.0f, 0.5f)); // Angled light
        light.setColor(glm::vec4(1.0f, 1.0f, 0.9f, 1.0f)); // Slightly warm light
    }

    // Point light (moved further for better lighting of the entire scene)
    testPointLight = m_registry->create();
    {
        auto& lightTransform = m_registry->addComponent<TransformComponent>(testPointLight);
        lightTransform.setPosition(glm::vec3(3.0f, 3.0f, 3.0f)); // Position further from center

        auto& light = m_registry->addComponent<LightComponent>(testPointLight, LightComponent::Type::Point);
        light.setColor(glm::vec4(0.8f, 0.8f, 1.0f, 1.0f)); // Cool light
        light.setRadius(25.0f); // Increased light range for covering larger scene
    }

    // Set up input handlers
    setupInputHandlers();

    // Log initial camera position and rotation
    if (m_registry->hasComponent<TransformComponent>(testCamera)) {
        auto& transform = m_registry->getComponent<TransformComponent>(testCamera);
        glm::vec3 pos = transform.getPosition();
        glm::vec3 rot = transform.getRotation();
        SPDLOG_ERROR("Initial Camera: Position[{:.2f}, {:.2f}, {:.2f}] Rotation[{:.2f}, {:.2f}, {:.2f}]",
            pos.x, pos.y, pos.z, rot.x, rot.y, rot.z);
    }
}

Scene::~Scene()
{
    // The Subscription objects will automatically unsubscribe in their destructors
    // No need to manually unsubscribe anymore
}

void Scene::setupInputHandlers()
{
    auto& inputSystem = Engine::get().inputSystem();

    // Subscribe to input events with the new Event system
    // The returned Subscription objects will automatically manage the lifetime of our subscriptions
    m_keyEventSub = inputSystem.onKey([this](int key, InputSystem::KeyState state) {
        this->handleKeyInput(key, state);
        });

    m_mouseButtonEventSub = inputSystem.onMouseButton([this](InputSystem::MouseButton button, bool pressed) {
        this->handleMouseButton(button, pressed);
        });

    m_mouseMoveEventSub = inputSystem.onMouseMove([this](glm::vec2 position) {
        this->handleMouseMove(position);
        });

    m_mouseScrollEventSub = inputSystem.onMouseScroll([this](float delta) {
        this->handleMouseScroll(delta);
        });
}

void Scene::handleKeyInput(int key, InputSystem::KeyState state)
{
    bool pressed = (state == InputSystem::KeyState::Pressed || state == InputSystem::KeyState::Repeated);

    // Camera movement keys
    switch (key) {
    case GLFW_KEY_W:
        m_movingForward = pressed;
        break;
    case GLFW_KEY_S:
        m_movingBackward = pressed;
        break;
    case GLFW_KEY_A:
        m_movingLeft = pressed;
        break;
    case GLFW_KEY_D:
        m_movingRight = pressed;
        break;
    case GLFW_KEY_SPACE:
        m_movingUp = pressed;
        break;
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
        m_movingDown = pressed;
        break;
    }

    // Log key state changes for debugging
    if (state == InputSystem::KeyState::Pressed || state == InputSystem::KeyState::Released) {
        SPDLOG_ERROR("Key {} {}", key, pressed ? "pressed" : "released");
    }
}

void Scene::handleMouseMove(glm::vec2 position)
{
    if (m_mouseRightButtonDown) {
        // Calculate mouse delta
        glm::vec2 delta = position - m_lastMousePos;

        // Update camera rotation
        m_cameraYawOffset -= delta.x * m_mouseSensitivity;
        m_cameraPitchOffset -= delta.y * m_mouseSensitivity;

        // Clamp pitch to avoid gimbal lock
        m_cameraPitchOffset = glm::clamp(m_cameraPitchOffset, -89.0f, 89.0f);

        // Wrap yaw to 0-360 degrees
        if (m_cameraYawOffset > 360.0f) {
            m_cameraYawOffset -= 360.0f;
        }
        else if (m_cameraYawOffset < 0.0f) {
            m_cameraYawOffset += 360.0f;
        }

        // Log rotation changes for significant movements
        if (glm::length(delta) > 5.0f) {
            SPDLOG_ERROR("Camera rotation: Yaw={:.2f}, Pitch={:.2f}", m_cameraYawOffset, m_cameraPitchOffset);
        }
    }

    // Update last mouse position
    m_lastMousePos = position;
}

void Scene::handleMouseButton(InputSystem::MouseButton button, bool pressed)
{
    if (button == InputSystem::MouseButton::Right) {
        m_mouseRightButtonDown = pressed;

        // Capture or release cursor
        GLFWwindow* window = Engine::get().window().get();
        if (pressed) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        SPDLOG_ERROR("Right mouse button {}", pressed ? "pressed" : "released");
    }
}

void Scene::handleMouseScroll(float delta)
{
    // Adjust movement speed with mouse wheel
    m_cameraMovementSpeed += delta * 0.5f;
    m_cameraMovementSpeed = glm::clamp(m_cameraMovementSpeed, 1.0f, 20.0f);

    SPDLOG_ERROR("Camera movement speed: {:.2f}", m_cameraMovementSpeed);
}

void Scene::updateCameraMovement(float deltaTime)
{
    if (!m_registry->hasComponent<TransformComponent>(testCamera)) {
        return;
    }

    auto& transform = m_registry->getComponent<TransformComponent>(testCamera);

    // Update camera rotation
    transform.setRotation(glm::vec3(m_cameraPitchOffset, m_cameraYawOffset, 0.0f));

    // Convert Euler angles to direction vectors
    float yawRad = glm::radians(m_cameraYawOffset);
    float pitchRad = glm::radians(m_cameraPitchOffset);

    glm::vec3 forward;
    forward.x = cos(yawRad) * cos(pitchRad);
    forward.y = sin(pitchRad);
    forward.z = sin(yawRad) * cos(pitchRad);
    forward = glm::normalize(forward);

    // Calculate right and up vectors
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Calculate movement direction
    glm::vec3 movement(0.0f);

    if (m_movingForward)  movement += forward;
    if (m_movingBackward) movement -= forward;
    if (m_movingRight)    movement += right;
    if (m_movingLeft)     movement -= right;
    if (m_movingUp)       movement += up;
    if (m_movingDown)     movement -= up;

    // Normalize if moving in multiple directions
    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement);
    }

    // Update camera position
    glm::vec3 currentPos = transform.getPosition();
    glm::vec3 newPos = currentPos + movement * m_cameraMovementSpeed * deltaTime;
    transform.setPosition(newPos);

    // Log position and rotation changes only when camera is actually moving
    if (glm::length(movement) > 0.0f || m_mouseRightButtonDown) {
        SPDLOG_ERROR("Camera: Pos[{:.2f}, {:.2f}, {:.2f}] Rot[{:.2f}, {:.2f}, {:.2f}] Speed: {:.2f}",
            newPos.x, newPos.y, newPos.z,
            m_cameraPitchOffset, m_cameraYawOffset, 0.0f,
            m_cameraMovementSpeed);
    }
}

void Scene::update()
{
    float deltaTime = Engine::get().getDeltaTime();

    // Update camera based on input
    updateCameraMovement(deltaTime);

    // Update all systems
    m_registry->getSystemManager().updateAll();
}