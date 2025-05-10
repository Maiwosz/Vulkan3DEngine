#include "Window.h"
#include "Instance.h"
#include <iostream>
#include "Engine.h"
#include "Settings.h"

Window::Window(const char* title, Settings::WindowMode initialMode, Settings::Resolution initialResolution)
    : m_title(title),
    m_currentMode(initialMode),
    m_currentResolution(initialResolution),
    m_resizeEvent(Event<int, int>::create()),
    m_focusEvent(Event<bool>::create()),
    m_minimizeEvent(Event<bool>::create()),
    m_closeEvent(Event<>::create()),
    m_isDestroying(false)
{
    initGLFW();
    createWindow();
    setupCallbacks();
}

Window::~Window() {
    // Set the destroying flag to prevent callbacks from using the events
    m_isDestroying.store(true);

    if (m_window) {
        // Reset callbacks first to prevent new events from being triggered
        glfwSetFramebufferSizeCallback(m_window, nullptr);
        glfwSetWindowFocusCallback(m_window, nullptr);
        glfwSetWindowIconifyCallback(m_window, nullptr);
        glfwSetWindowCloseCallback(m_window, nullptr);
        glfwSetWindowUserPointer(m_window, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

void Window::initGLFW() {
    glfwSetErrorCallback(handleGLFWError);
    if (!glfwInit()) {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void Window::createWindow() {
    const auto& res = Settings::RESOLUTION_MAP.at(m_currentResolution);
    GLFWmonitor* monitor = nullptr;
    int width = res.width;
    int height = res.height;

    if (m_currentMode != Settings::WindowMode::Windowed) {
        monitor = glfwGetPrimaryMonitor();
        if (m_currentMode == Settings::WindowMode::Borderless) {
            const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
            width = vidmode->width;
            height = vidmode->height;
        }
    }

    m_window = glfwCreateWindow(width, height, m_title.c_str(), monitor, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    if (m_currentMode == Settings::WindowMode::Borderless) {
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
    }

    glfwSetWindowUserPointer(m_window, this);
}

void Window::setupCallbacks() {
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        // Check if window is valid and not being destroyed
        if (!self || self->m_isDestroying.load() || !self->m_resizeEvent || !self->m_resizeEvent->isActive()) {
            return;
        }
        self->m_resizeEvent->invoke(w, h);
        });

    glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
        auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        // Check if window is valid and not being destroyed
        if (!self || self->m_isDestroying.load() || !self->m_focusEvent || !self->m_focusEvent->isActive()) {
            return;
        }
        self->m_focusEvent->invoke(focused);
        });

    glfwSetWindowIconifyCallback(m_window, [](GLFWwindow* window, int iconified) {
        auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        // Check if window is valid and not being destroyed
        if (!self || self->m_isDestroying.load() || !self->m_minimizeEvent || !self->m_minimizeEvent->isActive()) {
            return;
        }
        self->m_minimizeEvent->invoke(iconified);
        });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
        auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        // Check if window is valid and not being destroyed
        if (!self || self->m_isDestroying.load() || !self->m_closeEvent || !self->m_closeEvent->isActive()) {
            return;
        }
        self->m_closeEvent->invoke();
        });
}

void Window::handleGLFWError(int error, const char* description) {
    std::cerr << "GLFW Error: " << error << " - " << description << std::endl;
}

VkExtent2D Window::extent() const {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

void Window::onWindowModeChanged(Settings::WindowMode newMode) {
    if (m_currentMode != newMode) {
        m_currentMode = newMode;
        applyWindowMode(newMode);
    }
}

void Window::onResolutionChanged(Settings::Resolution newRes) {
    if (m_currentResolution != newRes) {
        m_currentResolution = newRes;
        applyResolution(newRes);
    }
}

void Window::applyWindowMode(Settings::WindowMode newMode) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
    int xpos, ypos;
    glfwGetWindowPos(m_window, &xpos, &ypos);

    switch (newMode) {
    case Settings::WindowMode::Windowed: {
        const auto& res = Settings::RESOLUTION_MAP.at(m_currentResolution);
        glfwSetWindowMonitor(m_window, nullptr, xpos, ypos, res.width, res.height, vidmode->refreshRate);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        break;
    }
    case Settings::WindowMode::Fullscreen: {
        const auto& res = Settings::RESOLUTION_MAP.at(m_currentResolution);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, res.width, res.height, vidmode->refreshRate);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        break;
    }
    case Settings::WindowMode::Borderless: {
        glfwSetWindowMonitor(m_window, monitor, 0, 0, vidmode->width, vidmode->height, vidmode->refreshRate);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        break;
    }
    }
}

void Window::applyResolution(Settings::Resolution newRes) {
    const auto& res = Settings::RESOLUTION_MAP.at(newRes);
    if (m_currentMode == Settings::WindowMode::Windowed) {
        glfwSetWindowSize(m_window, res.width, res.height);
    }
    else if (m_currentMode == Settings::WindowMode::Fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, res.width, res.height, vidmode->refreshRate);
    }
}