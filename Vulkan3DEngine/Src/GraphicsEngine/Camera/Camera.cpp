#include "Camera.h"
#include "../../InputSystem/InputSystem.h"
#include "../../Application/Application.h"
#include "../../WindowSytem/Window.h"

Camera::Camera(glm::vec3 startPosition, float startYaw, float startPitch) :
    m_position(startPosition), m_yaw(startYaw), m_pitch(startPitch), m_up(glm::vec3(0.0f, 0.0f, 1.0f)),
    m_speed(2.0f), m_mouseSensitivity(0.1f), m_zoom(45.0f), m_zoomSpeed(1.0f)
{
    updateCameraVectors(); 
}

Camera::~Camera()
{
}

const glm::mat4 Camera::getViewMatrix()
{
    // Create a rotation matrix
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(-m_yaw), glm::vec3(0.0f, 0.0f, 1.0f));

    // Create a translation matrix
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), -m_position);

    // Combine the rotation and translation to create a view matrix
    glm::mat4 view = rotation * translation;

    return view;
}

const glm::mat4 Camera::getProjectionMatrix(float width, float height)
{
    glm::mat4 proj = glm::perspective(glm::radians(m_zoom), width / height, 0.1f, 100.0f);
    proj[1][1] *= -1;
    return proj;
}

void Camera::updateCameraVectors()
{
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = sin(glm::radians(m_yaw));
    front.y = -cos(glm::radians(m_yaw));
    front.z = cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    // Also re-calculate the Right and Up vector
    m_right = glm::normalize(glm::cross(m_front, m_up));  // Normalize the vectors
}

void Camera::onKeyDown(int key)
{
    if (key == 'W') {
        m_position -= m_front * m_speed * Application::s_deltaTime; // Move forward
    }
    else if (key == 'S') {
        m_position += m_front * m_speed * Application::s_deltaTime; // Move backward
    }
    else if (key == 'A') {
        m_position += m_right * m_speed * Application::s_deltaTime; // Move left
    }
    else if (key == 'D') {
        m_position -= m_right * m_speed * Application::s_deltaTime; // Move right
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
    }
    else if (key == 9 && !tabPressed) {//Key for Tab
        tabPressed = true;
        m_position = glm::vec3(0.0f, 0.0f, 2.0f);
    }
}

void Camera::onKeyUp(int key)
{
    if (key == 160 && ctrlPressed) { // Key code for Left Shift
        ctrlPressed = false;
        m_speed = m_speed / 3.0f; // Reset speed
    }
    else if (key == 9 && tabPressed) {
        tabPressed = false;
    }
}

void Camera::onMouseMove(const glm::vec2& mouse_pos)
{
    static glm::vec2 lastMousePos = mouse_pos; // Store the last mouse position

    // Calculate the difference in mouse position
    glm::vec2 deltaMousePos = mouse_pos - lastMousePos;

    // Use the difference to adjust the yaw and pitch
    m_yaw -= deltaMousePos.x * m_mouseSensitivity; // yaw changes with left/right mouse movement
    m_pitch -= deltaMousePos.y * m_mouseSensitivity; // pitch changes with up/down mouse movement

    while (m_yaw > 360) m_yaw -= 360;
    while (m_yaw < -0) m_yaw += 360;

    //Clamp the pitch to avoid flipping the camera upside down
    if (m_pitch > 180.0f)
        m_pitch = 180.0f;
    if (m_pitch < 0.0f)
        m_pitch = 0.0f;

    updateCameraVectors(); // Update the camera vectors after changing the yaw and pitch

    // Reset the mouse position to the center of the window
    glm::vec2 centerPosition = glm::vec2(Application::s_window_width / 2.0f, Application::s_window_height / 2.0f);
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
    // Zoom in
    m_zoom -= m_zoomSpeed;
    if (m_zoom < 1.0f)
        m_zoom = 1.0f;
    if (m_zoom > 45.0f)
        m_zoom = 45.0f;
}

void Camera::onRightMouseUp(const glm::vec2& mouse_pos)
{
    // Zoom out
    m_zoom += m_zoomSpeed;
    if (m_zoom < 1.0f)
        m_zoom = 1.0f;
    if (m_zoom > 45.0f)
        m_zoom = 45.0f;
}