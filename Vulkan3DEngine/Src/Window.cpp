#include "Window.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <spdlog/spdlog.h>

// === GLFWContext Implementation ===
std::weak_ptr<GLFWContext> GLFWContext::s_instance;
std::mutex GLFWContext::s_mutex;

std::shared_ptr<GLFWContext> GLFWContext::getInstance() {
    std::lock_guard<std::mutex> lock(s_mutex);

    auto instance = s_instance.lock();
    if (!instance) {
        // Create new instance using private constructor
        instance = std::shared_ptr<GLFWContext>(new GLFWContext());
        s_instance = instance;
    }
    return instance;
}

GLFWContext::GLFWContext() {
    glfwSetErrorCallback(handleGLFWError);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    SPDLOG_INFO("GLFW initialized");
}

GLFWContext::~GLFWContext() {
    SPDLOG_INFO("Destroying GLFWContext...");
    m_isValid.store(false);

    // Give some time for any pending operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    glfwTerminate();
    SPDLOG_INFO("GLFW terminated");
}

void GLFWContext::handleGLFWError(int error, const char* description) {
    SPDLOG_ERROR("GLFW Error {}: {}", error, description);
}

// === WindowUtils Implementation ===
namespace WindowUtils {
    const std::unordered_map<Settings::Resolution, Settings::ResolutionInfo>& getResolutionMap() {
        static const std::unordered_map<Settings::Resolution, Settings::ResolutionInfo> resolutionMap = {
            { Settings::Resolution::R_640x480,   { 640,  480,  "640x480" } },
            { Settings::Resolution::R_800x600,   { 800,  600,  "800x600" } },
            { Settings::Resolution::R_1024x768,  { 1024, 768,  "1024x768" } },
            { Settings::Resolution::R_1280x720,  { 1280, 720,  "1280x720 (HD)" } },
            { Settings::Resolution::R_1366x768,  { 1366, 768,  "1366x768" } },
            { Settings::Resolution::R_1600x900,  { 1600, 900,  "1600x900" } },
            { Settings::Resolution::R_1920x1080, { 1920, 1080, "1920x1080 (Full HD)" } }
        };
        return resolutionMap;
    }

    Settings::ResolutionInfo getResolutionInfo(Settings::Resolution resolution) {
        const auto& map = getResolutionMap();
        auto it = map.find(resolution);
        if (it != map.end()) {
            return it->second;
        }
        return { 1280, 720, "1280x720 (HD)" }; // fallback
    }
}

// === Window Implementation ===
Window::Window(const CreateInfo& createInfo)
    : m_glfwContext(GLFWContext::getInstance()) // Keep GLFW alive
    , m_title(createInfo.title)
    , m_currentMode(createInfo.mode)
    , m_currentResolution(createInfo.resolution)
    , m_resizable(createInfo.resizable)
{
    if (!m_glfwContext || !m_glfwContext->isValid()) {
        throw std::runtime_error("GLFW context is not available");
    }

    createWindow(createInfo);
    setupCallbacks();
}

Window::~Window() {
    SPDLOG_INFO("Destroying Window...");

    // Disconnect from settings first
    disconnectFromSettings();

    // Clean up in correct order - events first, then window
    m_resizeEvent.clear();
    m_focusEvent.clear();
    m_minimizeEvent.clear();
    m_closeEvent.clear();

    destroyWindow();

    // GLFWContext will be automatically destroyed when the last Window is destroyed
    // (assuming no other shared_ptr references exist)
}

Window::Window(Window&& other) noexcept
    : m_glfwContext(std::move(other.m_glfwContext))
    , m_window(other.m_window)
    , m_title(std::move(other.m_title))
    , m_currentMode(other.m_currentMode)
    , m_currentResolution(other.m_currentResolution)
    , m_resizable(other.m_resizable)
    , m_resizeEvent(std::move(other.m_resizeEvent))
    , m_focusEvent(std::move(other.m_focusEvent))
    , m_minimizeEvent(std::move(other.m_minimizeEvent))
    , m_closeEvent(std::move(other.m_closeEvent))
    , m_settingsSubscription(std::move(other.m_settingsSubscription))
    , m_connectedSettings(other.m_connectedSettings)
{
    other.m_window = nullptr;
    other.m_connectedSettings = nullptr;

    // Copy pending resize state
    m_pendingResize.pending.store(other.m_pendingResize.pending.load());
    m_pendingResize.width.store(other.m_pendingResize.width.load());
    m_pendingResize.height.store(other.m_pendingResize.height.load());
    other.m_pendingResize.pending.store(false);

    // Copy pending focus state
    m_pendingFocus.pending.store(other.m_pendingFocus.pending.load());
    m_pendingFocus.focused.store(other.m_pendingFocus.focused.load());
    other.m_pendingFocus.pending.store(false);

    // Update user pointer if window was moved
    if (m_window && isGLFWValid()) {
        glfwSetWindowUserPointer(m_window, this);
    }
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        // Disconnect from current settings
        disconnectFromSettings();
        destroyWindow();

        m_glfwContext = std::move(other.m_glfwContext);
        m_window = other.m_window;
        m_title = std::move(other.m_title);
        m_currentMode = other.m_currentMode;
        m_currentResolution = other.m_currentResolution;
        m_resizable = other.m_resizable;
        m_resizeEvent = std::move(other.m_resizeEvent);
        m_focusEvent = std::move(other.m_focusEvent);
        m_minimizeEvent = std::move(other.m_minimizeEvent);
        m_closeEvent = std::move(other.m_closeEvent);
        m_settingsSubscription = std::move(other.m_settingsSubscription);
        m_connectedSettings = other.m_connectedSettings;

        other.m_window = nullptr;
        other.m_connectedSettings = nullptr;

        // Copy pending resize state
        m_pendingResize.pending.store(other.m_pendingResize.pending.load());
        m_pendingResize.width.store(other.m_pendingResize.width.load());
        m_pendingResize.height.store(other.m_pendingResize.height.load());
        other.m_pendingResize.pending.store(false);

        // Copy pending focus state
        m_pendingFocus.pending.store(other.m_pendingFocus.pending.load());
        m_pendingFocus.focused.store(other.m_pendingFocus.focused.load());
        other.m_pendingFocus.pending.store(false);

        // Update user pointer if window was moved
        if (m_window && isGLFWValid()) {
            glfwSetWindowUserPointer(m_window, this);
        }
    }
    return *this;
}

bool Window::isGLFWValid() const {
    return m_glfwContext && m_glfwContext->isValid();
}

void Window::createWindow(const CreateInfo& createInfo) {
    if (!isGLFWValid()) {
        throw std::runtime_error("GLFW context is not valid");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, createInfo.resizable ? GLFW_TRUE : GLFW_FALSE);

    auto resInfo = getResolutionInfo(createInfo.resolution);
    GLFWmonitor* monitor = nullptr;
    int width = resInfo.width;
    int height = resInfo.height;

    // Determine monitor and dimensions based on window mode
    switch (createInfo.mode) {
    case Settings::WindowMode::Windowed:
        monitor = nullptr;
        break;

    case Settings::WindowMode::Fullscreen:
        monitor = glfwGetPrimaryMonitor();
        break;

    case Settings::WindowMode::Borderless:
        monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
            width = vidmode->width;
            height = vidmode->height;
        }
        break;
    }

    m_window = glfwCreateWindow(width, height, createInfo.title.c_str(), monitor, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Apply borderless decoration
    if (createInfo.mode == Settings::WindowMode::Borderless) {
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
    }

    glfwSetWindowUserPointer(m_window, this);
    SPDLOG_INFO("Window created: {}x{}, mode: {}, resizable: {}",
        width, height, static_cast<int>(createInfo.mode), createInfo.resizable);
}

void Window::setupCallbacks() {
    if (!m_window || !isGLFWValid()) return;

    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetWindowFocusCallback(m_window, windowFocusCallback);
    glfwSetWindowIconifyCallback(m_window, windowIconifyCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);
}

void Window::destroyWindow() {
    if (m_window && isGLFWValid()) {
        // Clear callbacks to prevent issues during destruction
        glfwSetFramebufferSizeCallback(m_window, nullptr);
        glfwSetWindowFocusCallback(m_window, nullptr);
        glfwSetWindowIconifyCallback(m_window, nullptr);
        glfwSetWindowCloseCallback(m_window, nullptr);
        glfwSetWindowUserPointer(m_window, nullptr);

        glfwDestroyWindow(m_window);
        SPDLOG_INFO("GLFW window destroyed");
    }
    m_window = nullptr;
}

bool Window::shouldClose() const {
    return m_window && isGLFWValid() ? glfwWindowShouldClose(m_window) : true;
}

void Window::pollEvents() {
    if (isGLFWValid()) {
        glfwPollEvents();
        processBatchedEvents();
    }
}

void Window::processBatchedEvents() {
    // Process pending resize events
    if (m_pendingResize.pending.load()) {
        int width = m_pendingResize.width.load();
        int height = m_pendingResize.height.load();
        m_pendingResize.pending.store(false);

        SPDLOG_DEBUG("Processing resize event: {}x{}", width, height);
        m_resizeEvent.safeInvoke(width, height);
    }

    // Process pending focus events
    if (m_pendingFocus.pending.load()) {
        bool focused = m_pendingFocus.focused.load();
        m_pendingFocus.pending.store(false);

        SPDLOG_DEBUG("Processing focus event: {}", focused);
        m_focusEvent.safeInvoke(focused);
    }
}

VkExtent2D Window::extent() const {
    if (!m_window || !isGLFWValid()) return { 0, 0 };

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

bool Window::isMinimized() const {
    return m_window && isGLFWValid() ? glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) : false;
}

bool Window::isFocused() const {
    return m_window && isGLFWValid() ? glfwGetWindowAttrib(m_window, GLFW_FOCUSED) : false;
}

std::pair<int, int> Window::getSize() const {
    if (!m_window || !isGLFWValid()) return { 0, 0 };

    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return { width, height };
}

std::pair<int, int> Window::getPosition() const {
    if (!m_window || !isGLFWValid()) return { 0, 0 };

    int x, y;
    glfwGetWindowPos(m_window, &x, &y);
    return { x, y };
}

void Window::setWindowMode(Settings::WindowMode mode) {
    if (m_currentMode == mode) return;

    SPDLOG_INFO("Changing window mode from {} to {}",
        static_cast<int>(m_currentMode), static_cast<int>(mode));

    m_currentMode = mode;
    applyWindowMode(mode);
}

void Window::setResolution(Settings::Resolution resolution) {
    if (m_currentResolution == resolution) return;

    auto oldRes = getResolutionInfo(m_currentResolution);
    auto newRes = getResolutionInfo(resolution);

    SPDLOG_INFO("Changing resolution from {}x{} to {}x{}",
        oldRes.width, oldRes.height, newRes.width, newRes.height);

    m_currentResolution = resolution;
    applyResolution(resolution);
}

void Window::setTitle(const std::string& title) {
    if (m_title == title) return;

    SPDLOG_INFO("Changing window title from '{}' to '{}'", m_title, title);
    m_title = title;
    if (m_window && isGLFWValid()) {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

void Window::setResizable(bool resizable) {
    if (m_resizable == resizable) return;

    SPDLOG_INFO("Changing window resizable from {} to {}", m_resizable, resizable);
    m_resizable = resizable;
    applyResizable(resizable);
}

void Window::applyWindowMode(Settings::WindowMode mode) {
    if (!m_window || !isGLFWValid()) return;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) return;

    const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
    auto [currentX, currentY] = getPosition();
    auto resInfo = getResolutionInfo(m_currentResolution);

    switch (mode) {
    case Settings::WindowMode::Windowed: {
        // Najpierw przywróć dekoracje, zanim zmieniasz monitor
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);

        // Dla trybu okienkowego, ustaw pozycję wyśrodkowaną jeśli okno było w pełnym ekranie
        int windowedX = currentX;
        int windowedY = currentY;

        // Jeśli okno było w trybie pełnoekranowym lub bezramkowym, wyśrodkuj je
        if (m_currentMode == Settings::WindowMode::Fullscreen ||
            m_currentMode == Settings::WindowMode::Borderless) {
            windowedX = (vidmode->width - resInfo.width) / 2;
            windowedY = (vidmode->height - resInfo.height) / 2;
        }

        glfwSetWindowMonitor(m_window, nullptr, windowedX, windowedY,
            resInfo.width, resInfo.height, vidmode->refreshRate);
        break;
    }

    case Settings::WindowMode::Fullscreen:
        // Dla pełnego ekranu, zachowaj dekoracje
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(m_window, monitor, 0, 0,
            resInfo.width, resInfo.height, vidmode->refreshRate);
        break;

    case Settings::WindowMode::Borderless:
        // Dla bezramkowego, usuń dekoracje PO ustawieniu monitora
        glfwSetWindowMonitor(m_window, monitor, 0, 0,
            vidmode->width, vidmode->height, vidmode->refreshRate);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        break;
    }

    // Dodatkowe wymuszenie odświeżenia okna dla pewności
    if (mode == Settings::WindowMode::Windowed) {
        // Wymuszenie ponownego wyświetlenia dekoracji
        glfwShowWindow(m_window);
        glfwFocusWindow(m_window);
    }

    SPDLOG_DEBUG("Applied window mode: {}", static_cast<int>(mode));
}

void Window::applyResolution(Settings::Resolution resolution) {
    if (!m_window || !isGLFWValid()) return;

    auto resInfo = getResolutionInfo(resolution);

    switch (m_currentMode) {
    case Settings::WindowMode::Windowed:
        glfwSetWindowSize(m_window, resInfo.width, resInfo.height);
        SPDLOG_DEBUG("Applied windowed resolution: {}x{}", resInfo.width, resInfo.height);
        break;

    case Settings::WindowMode::Fullscreen: {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(m_window, monitor, 0, 0,
                resInfo.width, resInfo.height, vidmode->refreshRate);
            SPDLOG_DEBUG("Applied fullscreen resolution: {}x{}", resInfo.width, resInfo.height);
        }
        break;
    }

    case Settings::WindowMode::Borderless:
        // Borderless uses native resolution, so no change needed
        SPDLOG_DEBUG("Borderless mode - resolution change ignored");
        break;
    }
}

void Window::applyResizable(bool resizable) {
    if (!m_window || !isGLFWValid()) return;

    glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    SPDLOG_DEBUG("Applied resizable: {}", resizable);
}

Settings::ResolutionInfo Window::getResolutionInfo(Settings::Resolution resolution) const {
    return WindowUtils::getResolutionInfo(resolution);
}

void Window::connectToSettings(Settings& settings) {
    // Disconnect from any existing settings
    disconnectFromSettings();

    m_connectedSettings = &settings;

    // Subscribe to settings changes
    m_settingsSubscription = settings.onSettingChanged().subscribe(
        [this](Settings::SettingType type, const Settings::SettingValue& oldValue, const Settings::SettingValue& newValue) {
            onSettingsChanged(type, oldValue, newValue);
        }
    );

    SPDLOG_INFO("Window connected to Settings");
}

void Window::disconnectFromSettings() {
    if (m_settingsSubscription.has_value()) {
        m_settingsSubscription->unsubscribe();
        m_settingsSubscription.reset();
        SPDLOG_INFO("Window disconnected from Settings");
    }
    m_connectedSettings = nullptr;
}

void Window::onSettingsChanged(Settings::SettingType type, const Settings::SettingValue& oldValue, const Settings::SettingValue& newValue) {
    if (!m_connectedSettings) return;

    SPDLOG_DEBUG("Settings changed - type: {}", Settings::toString(type));

    try {
        switch (type) {
        case Settings::SettingType::WindowMode: {
            auto newMode = std::get<Settings::WindowMode>(newValue);
            setWindowMode(newMode);
            break;
        }
        case Settings::SettingType::Resolution: {
            auto newResolution = std::get<Settings::Resolution>(newValue);
            setResolution(newResolution);
            break;
        }
        default:
            // Other settings don't affect the window directly
            break;
        }
    }
    catch (const std::bad_variant_access& e) {
        SPDLOG_ERROR("Bad variant access in settings change handler: {}", e.what());
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in settings change handler: {}", e.what());
    }
}

// === GLFW Callbacks ===
void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        if (self->isGLFWValid()) {
            // Check if resizing is allowed
            if (!self->m_resizable) {
                SPDLOG_DEBUG("Ignoring resize event - window is not resizable");
                return;
            }

            // Store the latest resize info instead of immediately invoking
            self->m_pendingResize.width.store(width);
            self->m_pendingResize.height.store(height);
            self->m_pendingResize.pending.store(true);

            SPDLOG_DEBUG("Batched resize event: {}x{}", width, height);
        }
    }
}

void Window::windowFocusCallback(GLFWwindow* window, int focused) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        if (self->isGLFWValid()) {
            // Store the latest focus info instead of immediately invoking
            self->m_pendingFocus.focused.store(focused == GLFW_TRUE);
            self->m_pendingFocus.pending.store(true);

            SPDLOG_DEBUG("Batched focus event: {}", focused == GLFW_TRUE);
        }
    }
}

void Window::windowIconifyCallback(GLFWwindow* window, int iconified) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        if (self->isGLFWValid()) {
            SPDLOG_DEBUG("Window minimize event: {}", iconified == GLFW_TRUE);
            self->m_minimizeEvent.safeInvoke(iconified == GLFW_TRUE);
        }
    }
}

void Window::windowCloseCallback(GLFWwindow* window) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        if (self->isGLFWValid()) {
            SPDLOG_DEBUG("Window close event");
            self->m_closeEvent.safeInvoke();
        }
    }
}