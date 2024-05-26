#include "Camera.h"
#include "InputSystem.h"
#include "Application.h"
#include "Window.h"
#include "GraphicsEngine.h"

float Camera::s_mouseSensitivity = 20.0f;
float Camera::s_fov = 45.0f;

Camera::Camera(glm::vec3 startPosition, float startPitch, float startYaw, Scene* scene) :
    SceneObject(startPosition, glm::vec3(startPitch, startYaw, 0.0f), 1.0f , scene, "Camera"),
    m_up(glm::vec3(0.0f, 1.0f, 0.0f)), m_speed(3.0f), m_zoomSpeed(10.0f)
{
    updateCameraVectors(); 
}

Camera::Camera(const nlohmann::json& j, Scene* scene) :
    SceneObject(j["SceneObjectData"], scene), m_up(glm::vec3(0.0f, 1.0f, 0.0f)), m_speed(3.0f), m_zoomSpeed(10.0f) {
    try {
        updateCameraVectors();
        fmt::print("Camera initialized successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to initialize Camera. Exception: {}\n", e.what());
        throw;
    }
}


Camera::~Camera()
{
}

const glm::mat4 Camera::getViewMatrix()
{
    // Create a rotation matrix
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));

    // Create a translation matrix
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), -m_position);

    // Combine the rotation and translation to create a view matrix
    glm::mat4 view = rotation * translation;

    return view;
}

const glm::mat4 Camera::getProjectionMatrix()
{
    VkExtent2D swapChainExtent = GraphicsEngine::get()->getRenderer()->getSwapChain()->getSwapChainExtent();
    glm::mat4 proj = glm::perspective(glm::radians(s_fov), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, 0.1f, 100.0f);
    proj[1][1] *= -1;
    return proj;
}

void Camera::update()
{
    SceneObject::update();
    p_scene->ubo.view = getViewMatrix();
    p_scene->ubo.proj = getProjectionMatrix();
    p_scene->ubo.cameraPosition = m_position;
}

void Camera::draw()
{
}

void Camera::drawInterface()
{
    if (ImGui::CollapsingHeader("Camera Settings"))
    {
        ImGui::SliderFloat("Speed", &m_speed, 0.0f, 10.0f); // Slider from 0.0 to 10.0
        ImGui::SliderFloat("Mouse Sensitivity", &s_mouseSensitivity, 0.0f, 10.0f); // Slider from 0.0 to 10.0
        ImGui::SliderFloat("FOV", &s_fov, 1.0f, 180.0f); // Slider from 1.0 to 180.0
        ImGui::SliderFloat("Zoom Speed", &m_zoomSpeed, 0.0f, 10.0f); // Slider from 0.0 to 10.0
    }
}


void Camera::to_json(nlohmann::json& j)
{
    nlohmann::json SceneObjectJson;
    SceneObject::to_json(SceneObjectJson); // Call base class method

    j = nlohmann::json{
        {"type", "camera"},
        {"SceneObjectData", SceneObjectJson},
    };
}

void Camera::from_json(const nlohmann::json& j)
{
    try {

    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to deserialize Camera. Exception: {}\n", e.what());
        throw; // Rzuæmy wyj¹tek dalej, aby ³atwiej by³o œledziæ b³¹d
    }
}

void Camera::updateCameraVectors()
{
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = sin(glm::radians(m_rotation.y));
    front.y = 0.0f;
    front.z = -cos(glm::radians(m_rotation.y));
    m_front = glm::normalize(front);

    // Also re-calculate the Right and Up vector
    m_right = glm::normalize(glm::cross(m_front, m_up));  // Normalize the vectors
}

void Camera::onKeyDown(int key)
{
    if (Application::s_cursor_mode) return;

    if (key == GLFW_KEY_W) {
        m_position += m_front * m_speed * Application::s_deltaTime; // Move forward
    }
    else if (key == GLFW_KEY_S) {
        m_position -= m_front * m_speed * Application::s_deltaTime; // Move backward
    }
    else if (key == GLFW_KEY_A) {
        m_position -= m_right * m_speed * Application::s_deltaTime; // Move left
    }
    else if (key == GLFW_KEY_D) {
        m_position += m_right * m_speed * Application::s_deltaTime; // Move right
    }
    else if (key == GLFW_KEY_SPACE) {
        m_position += m_up * m_speed * Application::s_deltaTime; // Move up
    }
    else if (key == GLFW_KEY_LEFT_CONTROL) {
        m_position -= m_up * m_speed * Application::s_deltaTime; // Move down
    }
    else if (key == GLFW_KEY_LEFT_SHIFT && !ctrlPressed) {
        ctrlPressed = true;
        m_speed = m_speed * 3.0f; // Increase speed
    }
}

void Camera::onKeyUp(int key)
{
    if (Application::s_cursor_mode) return;

    if (key == GLFW_KEY_LEFT_SHIFT && ctrlPressed) {
        ctrlPressed = false;
        m_speed = m_speed / 3.0f; // Reset speed
    }
    else if (key == GLFW_KEY_TAB) {
        m_position = glm::vec3(-8.0f, 8.0f, 8.0f);
    }
}

void Camera::onMouseMove(const glm::vec2& mouse_pos)
{
    if (Application::s_cursor_mode) return;

    static glm::vec2 lastMousePos = mouse_pos; // Store the last mouse position

    // Calculate the difference in mouse position
    glm::vec2 deltaMousePos = mouse_pos - lastMousePos;

    // Use the difference to adjust the yaw and pitch
    m_rotation.y += deltaMousePos.x * s_mouseSensitivity * Application::s_deltaTime; // yaw changes with left/right mouse movement
    m_rotation.x -= deltaMousePos.y * s_mouseSensitivity * Application::s_deltaTime; // pitch changes with up/down mouse movement

    while (m_rotation.y > 360) m_rotation.y -= 360;
    while (m_rotation.y < -0) m_rotation.y += 360;

    //Clamp the pitch to avoid flipping the camera upside down
    if (m_rotation.x > 89.0f)
        m_rotation.x = 89.0f;
    if (m_rotation.x < -89.0f)
        m_rotation.x = -89.0f;

    updateCameraVectors(); // Update the camera vectors after changing the yaw and pitch

    // Reset the mouse position to the center of the window
    glm::vec2 centerPosition = glm::vec2(Window::get()->getExtent().width / 2.0f, Window::get()->getExtent().height / 2.0f);

    InputSystem::get()->setCursorPosition(centerPosition);

    // Update the last mouse position
    lastMousePos = centerPosition;
}

void Camera::onLeftMouseDown(const glm::vec2& mouse_pos)
{
}

void Camera::onLeftMouseUp(const glm::vec2& mouse_pos)
{
}

void Camera::onRightMouseDown(const glm::vec2& mouse_pos)
{
    if (Application::s_cursor_mode) return;
    // Zoom in
    s_fov = 15;
}

void Camera::onRightMouseUp(const glm::vec2& mouse_pos)
{
    if (Application::s_cursor_mode) return;
    // Zoom out
    s_fov = 45;
}