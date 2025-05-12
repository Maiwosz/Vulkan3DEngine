#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include "Event.h"

class Window;

class InputSystem {
public:
    enum class KeyState {
        Pressed,
        Released,
        Repeated
    };

    enum class MouseButton {
        Left = GLFW_MOUSE_BUTTON_LEFT,
        Right = GLFW_MOUSE_BUTTON_RIGHT,
        Middle = GLFW_MOUSE_BUTTON_MIDDLE
    };

    enum class CursorMode {
        Normal = GLFW_CURSOR_NORMAL,
        Hidden = GLFW_CURSOR_HIDDEN,
        Disabled = GLFW_CURSOR_DISABLED
    };

    InputSystem(Window& window);
    ~InputSystem();

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    void update();

    // Keyboard methods
    bool isKeyPressed(int key) const;  // Key just pressed this frame
    bool isKeyReleased(int key) const; // Key just released this frame
    bool isKeyHeld(int key) const;     // Key being held down
    bool wasKeyPressed(int key) const; // Key was pressed last frame
    bool wasKeyReleased(int key) const; // Key was released last frame

    // Mouse methods
    bool isMouseButtonPressed(MouseButton button) const;  // Button just pressed this frame
    bool isMouseButtonHeld(MouseButton button) const;     // Button being held down
    bool wasMouseButtonPressed(MouseButton button) const; // Button was pressed last frame
    bool wasMouseButtonReleased(MouseButton button) const; // Button was released last frame

    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta() const;
    float getMouseScrollDelta() const;

    void setCursorMode(CursorMode mode);

    // Event subscriptions
    typename Event<int, KeyState>::Subscription onKey(typename Event<int, KeyState>::Callback callback) {
        return m_keyEvent->subscribe(std::move(callback));
    }

    typename Event<MouseButton, bool>::Subscription onMouseButton(typename Event<MouseButton, bool>::Callback callback) {
        return m_mouseButtonEvent->subscribe(std::move(callback));
    }

    typename Event<glm::vec2>::Subscription onMouseMove(typename Event<glm::vec2>::Callback callback) {
        return m_mouseMoveEvent->subscribe(std::move(callback));
    }

    typename Event<float>::Subscription onMouseScroll(typename Event<float>::Callback callback) {
        return m_mouseScrollEvent->subscribe(std::move(callback));
    }

private:
    void setupCallbacks();

    // GLFW callback functions
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    Window& m_window;
    GLFWwindow* m_glfwWindow;
    void* m_callbackDataPtr = nullptr; // Wskaźnik do danych callbacków

    // Input states
    std::unordered_map<int, bool> m_currentKeyStates;
    std::unordered_map<int, bool> m_previousKeyStates;
    std::unordered_map<int, bool> m_currentMouseButtonStates;
    std::unordered_map<int, bool> m_previousMouseButtonStates;

    glm::vec2 m_currentMousePosition;
    glm::vec2 m_previousMousePosition;
    glm::vec2 m_mouseDelta;
    float m_mouseScrollDelta;

    // Events
    std::shared_ptr<Event<int, KeyState>> m_keyEvent;
    std::shared_ptr<Event<MouseButton, bool>> m_mouseButtonEvent;
    std::shared_ptr<Event<glm::vec2>> m_mouseMoveEvent;
    std::shared_ptr<Event<float>> m_mouseScrollEvent;
};