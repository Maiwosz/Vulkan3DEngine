#pragma once
#include "Prerequisites.h"
#include <string>
#include "MeshRenderSystem.h"
#include "Entity.h"
#include "Registry.h"
#include "CameraSystem.h"
#include "LightSystem.h"
#include "AssetCollectionSystem.h"
#include "Event.h"
#include "InputSystem.h"

class Scene {
public:
    Scene();
    ~Scene();

    void update();
    Registry& registry() { return *m_registry; }

    // Set up input handlers
    void setupInputHandlers();

private:
    // Input handling methods
    void handleKeyInput(int key, InputSystem::KeyState state);
    void handleMouseMove(glm::vec2 position);
    void handleMouseButton(InputSystem::MouseButton button, bool pressed);
    void handleMouseScroll(float delta);

    // Camera movement state
    void updateCameraMovement(float deltaTime);

    Entity testEntity;
    Entity testDirectionalLight;
    Entity testPointLight;
    Entity testCamera;
    std::unique_ptr<Registry> m_registry;

    // Camera control variables
    float m_cameraRotationSpeed;          // Rotation speed in degrees per second
    float m_cameraYawOffset;              // Current yaw rotation angle
    float m_cameraPitchOffset;            // Current pitch rotation angle
    float m_cameraMovementSpeed;          // Movement speed units per second

    // Input tracking
    bool m_movingForward;
    bool m_movingBackward;
    bool m_movingLeft;
    bool m_movingRight;
    bool m_movingUp;
    bool m_movingDown;

    // Mouse control
    bool m_mouseRightButtonDown;
    glm::vec2 m_lastMousePos;
    float m_mouseSensitivity;

    // Event subscriptions using the new Subscription class
    Event<int, InputSystem::KeyState>::Subscription m_keyEventSub;
    Event<InputSystem::MouseButton, bool>::Subscription m_mouseButtonEventSub;
    Event<glm::vec2>::Subscription m_mouseMoveEventSub;
    Event<float>::Subscription m_mouseScrollEventSub;
};