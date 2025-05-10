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

    void updatePointLightOrbit(float deltaTime);

    Entity testEntity;
    Entity testEntity2;
    Entity testEntity3;
    Entity testDirectionalLight;
    Entity testPointLight;
    Entity testCamera;
    Entity testFloor;
    std::unique_ptr<Registry> m_registry;

    // Point light orbit parameters
    float m_lightOrbitRadius;             // Radius of orbit in units
    float m_lightOrbitSpeed;              // Orbit speed in radians per second
    float m_lightOrbitHeight;             // Height of orbit above floor
    float m_lightOrbitAngle;              // Current angle of orbit in radians

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