#pragma once
#include "Prerequisites.h"
#include "InputListener.h"
#include "SceneObject.h"

class Camera : public SceneObject, public InputListener
{
public:
    Camera(glm::vec3 startPosition, float startRotY, float startRotX, Scene* scene);
    ~Camera();

    const glm::mat4 getViewMatrix();
    const glm::mat4 getProjectionMatrix();

    void update() override;
    void draw() override;

    glm::vec3 m_up;
    glm::vec3 m_front;
    glm::vec3 m_right;

private:
    // Inherited via InputListener
    virtual void onKeyDown(int key) override;
    virtual void onKeyUp(int key) override;

    virtual void onMouseMove(const glm::vec2& mouse_pos) override;
    virtual void onLeftMouseDown(const glm::vec2& mouse_pos) override;
    virtual void onLeftMouseUp(const glm::vec2& mouse_pos) override;
    virtual void onRightMouseDown(const glm::vec2& mouse_pos) override;
    virtual void onRightMouseUp(const glm::vec2& mouse_pos) override;

    void updateCameraVectors();

    float m_speed;
    float m_mouseSensitivity;
    float m_zoom;
    float m_zoomSpeed;

    bool ctrlPressed = false;
};

