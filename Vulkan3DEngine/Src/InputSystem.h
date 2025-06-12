#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include "Event.h"

class Window;

class InputSystem {
public:
    enum class KeyState {
        Pressed,    // Key was just pressed this frame
        Released,   // Key was just released this frame
        Held,       // Key is being held down
        Repeated    // Key repeat event
    };

    enum class MouseButton {
        Left = GLFW_MOUSE_BUTTON_LEFT,
        Right = GLFW_MOUSE_BUTTON_RIGHT,
        Middle = GLFW_MOUSE_BUTTON_MIDDLE,
        Button4 = GLFW_MOUSE_BUTTON_4,
        Button5 = GLFW_MOUSE_BUTTON_5
    };

    enum class CursorMode {
        Normal = GLFW_CURSOR_NORMAL,
        Hidden = GLFW_CURSOR_HIDDEN,
        Disabled = GLFW_CURSOR_DISABLED
    };

    // Event types for better type safety and clarity
    using KeyEvent = Event<int, KeyState>;
    using MouseButtonEvent = Event<MouseButton, bool>;
    using MouseMoveEvent = Event<glm::vec2>;
    using MouseScrollEvent = Event<glm::vec2>; // x and y scroll
    using CharEvent = Event<unsigned int>; // For text input

    struct MouseState {
        glm::vec2 position{ 0.0f };        // Aktualna pozycja
        glm::vec2 previousPosition{ 0.0f }; // Pozycja z poprzedniej klatki
        glm::vec2 scroll{ 0.0f };           // Aktualna wartość scroll
        glm::vec2 previousScroll{ 0.0f };   // Scroll z poprzedniej klatki
        glm::vec2 delta{ 0.0f };            // Obliczona delta pozycji
        glm::vec2 scrollDelta{ 0.0f };      // Obliczona delta scroll
    };

public:
    explicit InputSystem(Window& window);
    ~InputSystem();

    // Non-copyable, non-movable (due to GLFW callback complexity)
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;
    InputSystem(InputSystem&&) = delete;
    InputSystem& operator=(InputSystem&&) = delete;

    // === Update Method ===
    void update();

    // === Keyboard Query Interface ===
    bool isKeyPressed(int key) const;   // Just pressed this frame
    bool isKeyReleased(int key) const;  // Just released this frame  
    bool isKeyHeld(int key) const;      // Currently being held
    KeyState getKeyState(int key) const;

    // === Mouse Query Interface ===
    bool isMousePressed(MouseButton button) const;   // Just pressed this frame
    bool isMouseReleased(MouseButton button) const;  // Just released this frame
    bool isMouseHeld(MouseButton button) const;      // Currently being held

    // === Mouse State ===
    const MouseState& getMouseState() const { return m_mouseState; }
    glm::vec2 getMousePosition() const { return m_mouseState.position; }
    glm::vec2 getMouseDelta() const {return m_mouseState.delta;}
    glm::vec2 getScrollDelta() const { return m_mouseState.scrollDelta; }

    // === Cursor Control ===
    void setCursorMode(CursorMode mode);
    CursorMode getCursorMode() const { return m_cursorMode; }

    // === Event Subscriptions ===
    [[nodiscard]] KeyEvent::Subscription onKey(KeyEvent::Callback callback) {
        return m_keyEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] MouseButtonEvent::Subscription onMouseButton(MouseButtonEvent::Callback callback) {
        return m_mouseButtonEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] MouseMoveEvent::Subscription onMouseMove(MouseMoveEvent::Callback callback) {
        return m_mouseMoveEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] MouseScrollEvent::Subscription onMouseScroll(MouseScrollEvent::Callback callback) {
        return m_mouseScrollEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] CharEvent::Subscription onChar(CharEvent::Callback callback) {
        return m_charEvent.subscribe(std::move(callback));
    }

    // === Utility Methods ===
    void clearStates(); // Clear all input states
    size_t getActiveKeyCount() const;
    size_t getActiveMouseButtonCount() const;

private:
    // RAII helper for managing GLFW callbacks
    class CallbackManager {
    public:
        explicit CallbackManager(GLFWwindow* window, InputSystem* inputSystem);
        ~CallbackManager();

        CallbackManager(const CallbackManager&) = delete;
        CallbackManager& operator=(const CallbackManager&) = delete;

    private:
        GLFWwindow* m_window;
        void* m_originalUserPointer;
    };

    struct KeyInfo {
        bool currentState = false;
        bool previousState = false;
        KeyState lastEventState = KeyState::Released;
    };

    struct MouseButtonInfo {
        bool currentState = false;
        bool previousState = false;
    };

    void setupExceptionHandlers();
    void initializeMousePosition();

    // State update helpers
    void updateKeyStates();
    void updateMouseStates();

    // GLFW callback functions
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);

    // Helper to get InputSystem from GLFW window
    static InputSystem* getInputSystem(GLFWwindow* window);

private:
    Window& m_window;
    GLFWwindow* m_glfwWindow;

    // Input state tracking
    std::unordered_map<int, KeyInfo> m_keyStates;
    std::unordered_map<int, MouseButtonInfo> m_mouseButtonStates;
    MouseState m_mouseState;
    CursorMode m_cursorMode = CursorMode::Normal;

    // Events
    KeyEvent m_keyEvent;
    MouseButtonEvent m_mouseButtonEvent;
    MouseMoveEvent m_mouseMoveEvent;
    MouseScrollEvent m_mouseScrollEvent;
    CharEvent m_charEvent;

    // RAII callback management
    std::unique_ptr<CallbackManager> m_callbackManager;
};