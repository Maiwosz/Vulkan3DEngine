#include "InputSystem.h"
#include "Window.h"
#include <iostream>
#include <stdexcept>

// === CallbackManager Implementation ===
InputSystem::CallbackManager::CallbackManager(GLFWwindow* window, InputSystem* inputSystem)
    : m_window(window)
    , m_originalUserPointer(glfwGetWindowUserPointer(window))
{
    if (!window) {
        throw std::invalid_argument("GLFW window cannot be null");
    }

    // Set InputSystem as user pointer for callbacks
    glfwSetWindowUserPointer(window, inputSystem);

    // Setup all callbacks
    glfwSetKeyCallback(window, InputSystem::keyCallback);
    glfwSetMouseButtonCallback(window, InputSystem::mouseButtonCallback);
    glfwSetCursorPosCallback(window, InputSystem::cursorPositionCallback);
    glfwSetScrollCallback(window, InputSystem::scrollCallback);
    glfwSetCharCallback(window, InputSystem::charCallback);
}

InputSystem::CallbackManager::~CallbackManager() {
    if (m_window) {
        // Clear all callbacks first
        glfwSetKeyCallback(m_window, nullptr);
        glfwSetMouseButtonCallback(m_window, nullptr);
        glfwSetCursorPosCallback(m_window, nullptr);
        glfwSetScrollCallback(m_window, nullptr);
        glfwSetCharCallback(m_window, nullptr);

        // Restore original user pointer
        glfwSetWindowUserPointer(m_window, m_originalUserPointer);
    }
}

// === InputSystem Implementation ===
InputSystem::InputSystem(Window& window)
    : m_window(window)
    , m_glfwWindow(window.get())
{
    if (!m_glfwWindow) {
        throw std::runtime_error("Invalid GLFW window handle provided to InputSystem");
    }

    setupExceptionHandlers();
    initializeMousePosition();

    // Setup callbacks last to ensure everything is initialized
    m_callbackManager = std::make_unique<CallbackManager>(m_glfwWindow, this);
}

InputSystem::~InputSystem() {
    // CallbackManager will handle cleanup automatically via RAII
}

void InputSystem::update() {
    // NAJPIERW oblicz delty na podstawie aktualnych wartości
    m_mouseState.delta = m_mouseState.position - m_mouseState.previousPosition;
    m_mouseState.scrollDelta = m_mouseState.scroll - m_mouseState.previousScroll;

    // Aktualizuj stany klawiszy i myszy
    updateKeyStates();
    updateMouseStates();

    // DOPIERO NA KOŃCU zapisz poprzednie wartości dla następnej klatki
    m_mouseState.previousPosition = m_mouseState.position;
    m_mouseState.previousScroll = m_mouseState.scroll;
}

// === Keyboard Query Methods ===
bool InputSystem::isKeyPressed(int key) const {
    auto it = m_keyStates.find(key);
    if (it == m_keyStates.end()) return false;

    return it->second.currentState && !it->second.previousState;
}

bool InputSystem::isKeyReleased(int key) const {
    auto it = m_keyStates.find(key);
    if (it == m_keyStates.end()) return false;

    return !it->second.currentState && it->second.previousState;
}

bool InputSystem::isKeyHeld(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && it->second.currentState;
}

InputSystem::KeyState InputSystem::getKeyState(int key) const {
    auto it = m_keyStates.find(key);
    if (it == m_keyStates.end()) {
        return KeyState::Released;
    }

    const auto& info = it->second;

    if (info.currentState && !info.previousState) {
        return KeyState::Pressed;
    }
    else if (!info.currentState && info.previousState) {
        return KeyState::Released;
    }
    else if (info.currentState) {
        return KeyState::Held;
    }

    return KeyState::Released;
}

// === Mouse Query Methods ===
bool InputSystem::isMousePressed(MouseButton button) const {
    int buttonCode = static_cast<int>(button);
    auto it = m_mouseButtonStates.find(buttonCode);
    if (it == m_mouseButtonStates.end()) return false;

    return it->second.currentState && !it->second.previousState;
}

bool InputSystem::isMouseReleased(MouseButton button) const {
    int buttonCode = static_cast<int>(button);
    auto it = m_mouseButtonStates.find(buttonCode);
    if (it == m_mouseButtonStates.end()) return false;

    return !it->second.currentState && it->second.previousState;
}

bool InputSystem::isMouseHeld(MouseButton button) const {
    int buttonCode = static_cast<int>(button);
    auto it = m_mouseButtonStates.find(buttonCode);
    return it != m_mouseButtonStates.end() && it->second.currentState;
}

// === Cursor Control ===
void InputSystem::setCursorMode(CursorMode mode) {
    m_cursorMode = mode;
    glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, static_cast<int>(mode));
}

// === Utility Methods ===
void InputSystem::clearStates() {
    m_keyStates.clear();
    m_mouseButtonStates.clear();
    m_mouseState = MouseState{};
}

size_t InputSystem::getActiveKeyCount() const {
    size_t count = 0;
    for (const auto& [key, info] : m_keyStates) {
        if (info.currentState) ++count;
    }
    return count;
}

size_t InputSystem::getActiveMouseButtonCount() const {
    size_t count = 0;
    for (const auto& [button, info] : m_mouseButtonStates) {
        if (info.currentState) ++count;
    }
    return count;
}

// === Private Helper Methods ===
void InputSystem::setupExceptionHandlers() {
    auto exceptionHandler = [](std::exception_ptr e) {
        try {
            if (e) std::rethrow_exception(e);
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception in InputSystem event: " << ex.what() << std::endl;
        }
        };

    m_keyEvent.setExceptionHandler(exceptionHandler);
    m_mouseButtonEvent.setExceptionHandler(exceptionHandler);
    m_mouseMoveEvent.setExceptionHandler(exceptionHandler);
    m_mouseScrollEvent.setExceptionHandler(exceptionHandler);
    m_charEvent.setExceptionHandler(exceptionHandler);
}

void InputSystem::initializeMousePosition() {
    double xpos, ypos;
    glfwGetCursorPos(m_glfwWindow, &xpos, &ypos);
    m_mouseState.position = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
}

void InputSystem::updateKeyStates() {
    for (auto& [key, info] : m_keyStates) {
        info.previousState = info.currentState;
    }
}

void InputSystem::updateMouseStates() {
    for (auto& [button, info] : m_mouseButtonStates) {
        info.previousState = info.currentState;
    }
}

// === GLFW Callback Functions ===
void InputSystem::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* input = getInputSystem(window);
    if (!input) return;

    // Update key state
    auto& keyInfo = input->m_keyStates[key];
    keyInfo.currentState = (action != GLFW_RELEASE);

    // Determine key state for event
    KeyState keyState;
    switch (action) {
    case GLFW_PRESS:   keyState = KeyState::Pressed; break;
    case GLFW_RELEASE: keyState = KeyState::Released; break;
    case GLFW_REPEAT:  keyState = KeyState::Repeated; break;
    default: return;
    }

    keyInfo.lastEventState = keyState;

    // Fire event
    input->m_keyEvent.invoke(key, keyState);
}

void InputSystem::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* input = getInputSystem(window);
    if (!input) return;

    // Update button state
    auto& buttonInfo = input->m_mouseButtonStates[button];
    buttonInfo.currentState = (action == GLFW_PRESS);

    // Fire event
    MouseButton mouseButton = static_cast<MouseButton>(button);
    bool pressed = (action == GLFW_PRESS);
    input->m_mouseButtonEvent.invoke(mouseButton, pressed);
}

void InputSystem::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* input = getInputSystem(window);
    if (!input) return;

    glm::vec2 newPosition(static_cast<float>(xpos), static_cast<float>(ypos));

    // Po prostu aktualizuj pozycję
    input->m_mouseState.position = newPosition;

    // Wyślij event z nową pozycją
    input->m_mouseMoveEvent.invoke(newPosition);
}

void InputSystem::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* input = getInputSystem(window);
    if (!input) return;

    // Aktualizuj scroll (akumuluj w ramach klatki)
    input->m_mouseState.scroll += glm::vec2(
        static_cast<float>(xoffset),
        static_cast<float>(yoffset)
    );

    // Wyślij event z deltą dla tego zdarzenia
    glm::vec2 eventDelta(static_cast<float>(xoffset), static_cast<float>(yoffset));
    input->m_mouseScrollEvent.invoke(eventDelta);
}


void InputSystem::charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* input = getInputSystem(window);
    if (!input) return;

    // Fire character event for text input
    input->m_charEvent.invoke(codepoint);
}

InputSystem* InputSystem::getInputSystem(GLFWwindow* window) {
    if (!window) return nullptr;
    return static_cast<InputSystem*>(glfwGetWindowUserPointer(window));
}