#include "Camera.h"
#include "InputSystem.h"
#include "Application.h"
#include "Window.h"
#include "GraphicsEngine.h"

Camera::Camera(glm::vec3 startPosition, float startPitch, float startYaw, Scene* scene) :
    SceneObject(startPosition, glm::vec3(startPitch, startYaw, 0.0f), 1.0f , scene),
    m_up(glm::vec3(0.0f, 1.0f, 0.0f)), m_speed(3.0f), m_mouseSensitivity(20.0f), m_zoom(45.0f), m_zoomSpeed(10.0f)
{
    updateCameraVectors(); 
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
    glm::mat4 proj = glm::perspective(glm::radians(m_zoom), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, 0.1f, 100.0f);
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

    if (key == 'W') {
        m_position += m_front * m_speed * Application::s_deltaTime; // Move forward
    }
    else if (key == 'S') {
        m_position -= m_front * m_speed * Application::s_deltaTime; // Move backward
    }
    else if (key == 'A') {
        m_position -= m_right * m_speed * Application::s_deltaTime; // Move left
    }
    else if (key == 'D') {
        m_position += m_right * m_speed * Application::s_deltaTime; // Move right
    }
    else if (key == 32) { // ASCII for Space
        m_position += m_up * m_speed * Application::s_deltaTime; // Move up
    }
    else if (key == 162) { // Key code for Left Control
        m_position -= m_up * m_speed * Application::s_deltaTime; // Move down
    }
    else if (key == 160 && !ctrlPressed) { // Key code for Left Shift
        ctrlPressed = true;
        m_speed = m_speed * 3.0f; // Increase speed
        m_mouseSensitivity = m_mouseSensitivity / 3.0f;
    }
}

void Camera::onKeyUp(int key)
{
    if (Application::s_cursor_mode) return;

    if (key == 160 && ctrlPressed) { // Key code for Left Shift
        ctrlPressed = false;
        m_speed = m_speed / 3.0f; // Reset speed
        m_mouseSensitivity = m_mouseSensitivity * 3.0f;
    }
    else if (key == 9) {//Key for Tab
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
    m_rotation.y += deltaMousePos.x * m_mouseSensitivity * Application::s_deltaTime; // yaw changes with left/right mouse movement
    m_rotation.x -= deltaMousePos.y * m_mouseSensitivity * Application::s_deltaTime; // pitch changes with up/down mouse movement

    while (m_rotation.y > 360) m_rotation.y -= 360;
    while (m_rotation.y < -0) m_rotation.y += 360;

    //Clamp the pitch to avoid flipping the camera upside down
    if (m_rotation.x > 89.0f)
        m_rotation.x = 89.0f;
    if (m_rotation.x < -89.0f)
        m_rotation.x = -89.0f;

    updateCameraVectors(); // Update the camera vectors after changing the yaw and pitch

    // Reset the mouse position to the center of the window
    glm::vec2 centerPosition = glm::vec2(GraphicsEngine::get()->getWindow()->getExtent().width / 2.0f,
        GraphicsEngine::get()->getWindow()->getExtent().height / 2.0f);

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
    m_zoom = 15;
}

void Camera::onRightMouseUp(const glm::vec2& mouse_pos)
{
    if (Application::s_cursor_mode) return;
    // Zoom out
    m_zoom = 45;
}