#include "InputSystem.h"
#include "Window.h"
#include <iostream>

// Struktura pomocnicza do przechowywania danych wejściowych dla callbacków
struct InputCallbackData {
    InputSystem* inputSystem;
    void* originalUserPointer;
};

// Helper function to safely invoke events
template <typename... Args>
static void tryInvoke(std::shared_ptr<Event<Args...>> event, Args... args) {
    if (event && event->isActive()) {
        event->invoke(std::forward<Args>(args)...);
    }
}

InputSystem::InputSystem(Window& window)
    : m_window(window), m_glfwWindow(window.get()),
    m_currentMousePosition(0.0f), m_previousMousePosition(0.0f),
    m_mouseDelta(0.0f), m_mouseScrollDelta(0.0f)
{
    // Ensure window is valid
    if (!m_glfwWindow) {
        throw std::runtime_error("Invalid GLFW window handle in InputSystem constructor");
    }

    // Create events with shared exception handler
    auto exceptionHandler = [](std::exception_ptr e) {
        try {
            if (e) std::rethrow_exception(e);
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception in input event handler: " << ex.what() << std::endl;
        }
        };

    m_keyEvent = Event<int, KeyState>::create();
    m_keyEvent->setExceptionHandler(exceptionHandler);

    m_mouseButtonEvent = Event<MouseButton, bool>::create();
    m_mouseButtonEvent->setExceptionHandler(exceptionHandler);

    m_mouseMoveEvent = Event<glm::vec2>::create();
    m_mouseMoveEvent->setExceptionHandler(exceptionHandler);

    m_mouseScrollEvent = Event<float>::create();
    m_mouseScrollEvent->setExceptionHandler(exceptionHandler);

    // Initialize mouse position
    double xpos, ypos;
    glfwGetCursorPos(m_glfwWindow, &xpos, &ypos);
    m_currentMousePosition = m_previousMousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    // Setup callbacks after initializing everything else
    setupCallbacks();
}

InputSystem::~InputSystem() {
    // Restore original window user pointer
    if (m_glfwWindow && m_callbackDataPtr) {
        auto* callbackData = static_cast<InputCallbackData*>(m_callbackDataPtr);
        // Reset callbacks to prevent them from firing during destruction
        glfwSetKeyCallback(m_glfwWindow, nullptr);
        glfwSetMouseButtonCallback(m_glfwWindow, nullptr);
        glfwSetCursorPosCallback(m_glfwWindow, nullptr);
        glfwSetScrollCallback(m_glfwWindow, nullptr);

        // Restore original user pointer
        glfwSetWindowUserPointer(m_glfwWindow, callbackData->originalUserPointer);

        delete callbackData;
        m_callbackDataPtr = nullptr;
    }
}

void InputSystem::update()
{
    // Update previous states
    m_previousKeyStates = m_currentKeyStates;
    m_previousMouseButtonStates = m_currentMouseButtonStates;
    m_previousMousePosition = m_currentMousePosition;

    // Update mouse delta
    m_mouseDelta = m_currentMousePosition - m_previousMousePosition;

    // Reset scroll delta each frame
    m_mouseScrollDelta = 0.0f;
}

bool InputSystem::isKeyPressed(int key) const
{
    auto it = m_currentKeyStates.find(key);
    auto prevIt = m_previousKeyStates.find(key);

    return (it != m_currentKeyStates.end() && it->second) &&
        (prevIt == m_previousKeyStates.end() || !prevIt->second);
}

bool InputSystem::isKeyReleased(int key) const
{
    auto it = m_currentKeyStates.find(key);
    auto prevIt = m_previousKeyStates.find(key);

    return (it == m_currentKeyStates.end() || !it->second) &&
        (prevIt != m_previousKeyStates.end() && prevIt->second);
}

bool InputSystem::isKeyHeld(int key) const
{
    auto it = m_currentKeyStates.find(key);
    return it != m_currentKeyStates.end() && it->second;
}

bool InputSystem::wasKeyPressed(int key) const
{
    auto prevIt = m_previousKeyStates.find(key);
    return prevIt != m_previousKeyStates.end() && prevIt->second;
}

bool InputSystem::wasKeyReleased(int key) const
{
    auto prevIt = m_previousKeyStates.find(key);
    return prevIt == m_previousKeyStates.end() || !prevIt->second;
}

bool InputSystem::isMouseButtonPressed(MouseButton button) const
{
    int buttonCode = static_cast<int>(button);
    auto it = m_currentMouseButtonStates.find(buttonCode);
    auto prevIt = m_previousMouseButtonStates.find(buttonCode);

    return (it != m_currentMouseButtonStates.end() && it->second) &&
        (prevIt == m_previousMouseButtonStates.end() || !prevIt->second);
}

bool InputSystem::isMouseButtonHeld(MouseButton button) const
{
    int buttonCode = static_cast<int>(button);
    auto it = m_currentMouseButtonStates.find(buttonCode);
    return it != m_currentMouseButtonStates.end() && it->second;
}

bool InputSystem::wasMouseButtonPressed(MouseButton button) const
{
    int buttonCode = static_cast<int>(button);
    auto prevIt = m_previousMouseButtonStates.find(buttonCode);
    return prevIt != m_previousMouseButtonStates.end() && prevIt->second;
}

bool InputSystem::wasMouseButtonReleased(MouseButton button) const
{
    int buttonCode = static_cast<int>(button);
    auto prevIt = m_previousMouseButtonStates.find(buttonCode);
    return prevIt == m_previousMouseButtonStates.end() || !prevIt->second;
}

glm::vec2 InputSystem::getMousePosition() const
{
    return m_currentMousePosition;
}

glm::vec2 InputSystem::getMouseDelta() const
{
    return m_mouseDelta;
}

float InputSystem::getMouseScrollDelta() const
{
    return m_mouseScrollDelta;
}

void InputSystem::setupCallbacks()
{
    if (!m_glfwWindow) {
        std::cerr << "Cannot set up input callbacks: window is null" << std::endl;
        return;
    }

    // Get the current window user pointer (which is a Window*)
    void* windowPtr = glfwGetWindowUserPointer(m_glfwWindow);

    // Create new callback data that keeps track of the Window pointer
    InputCallbackData* callbackData = new InputCallbackData{ this, windowPtr };
    m_callbackDataPtr = callbackData;

    // Replace the user pointer with our callback data
    glfwSetWindowUserPointer(m_glfwWindow, callbackData);

    // Set callbacks
    glfwSetKeyCallback(m_glfwWindow, keyCallback);
    glfwSetMouseButtonCallback(m_glfwWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(m_glfwWindow, cursorPositionCallback);
    glfwSetScrollCallback(m_glfwWindow, scrollCallback);
}

void InputSystem::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* callbackData = static_cast<InputCallbackData*>(glfwGetWindowUserPointer(window));
    if (!callbackData || !callbackData->inputSystem) return;

    auto* input = callbackData->inputSystem;

    // Update key state
    input->m_currentKeyStates[key] = (action != GLFW_RELEASE);

    // Fire event
    KeyState state;
    switch (action) {
    case GLFW_PRESS:   state = KeyState::Pressed; break;
    case GLFW_RELEASE: state = KeyState::Released; break;
    case GLFW_REPEAT:  state = KeyState::Repeated; break;
    default:           return;
    }

    tryInvoke(input->m_keyEvent, key, state);
}

void InputSystem::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* callbackData = static_cast<InputCallbackData*>(glfwGetWindowUserPointer(window));
    if (!callbackData || !callbackData->inputSystem) return;

    auto* input = callbackData->inputSystem;

    // Update button state
    input->m_currentMouseButtonStates[button] = (action == GLFW_PRESS);

    // Fire event
    MouseButton mouseButton = static_cast<MouseButton>(button);
    bool pressed = (action == GLFW_PRESS);
    tryInvoke(input->m_mouseButtonEvent, mouseButton, pressed);
}

void InputSystem::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* callbackData = static_cast<InputCallbackData*>(glfwGetWindowUserPointer(window));
    if (!callbackData || !callbackData->inputSystem) return;

    auto* input = callbackData->inputSystem;

    // Update mouse position
    input->m_currentMousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    // Fire event
    tryInvoke(input->m_mouseMoveEvent, input->m_currentMousePosition);
}

void InputSystem::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* callbackData = static_cast<InputCallbackData*>(glfwGetWindowUserPointer(window));
    if (!callbackData || !callbackData->inputSystem) return;

    auto* input = callbackData->inputSystem;

    // Update scroll delta
    input->m_mouseScrollDelta = static_cast<float>(yoffset);

    // Fire event
    tryInvoke(input->m_mouseScrollEvent, input->m_mouseScrollDelta);
}