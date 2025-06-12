#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Event.h"
#include "Settings.h"
#include <string>
#include <memory>
#include <atomic>

// Improved RAII wrapper for GLFW initialization with proper lifetime management
class GLFWContext {
public:
    static std::shared_ptr<GLFWContext> getInstance();
    ~GLFWContext();

    // Check if GLFW is still valid (not being destroyed)
    bool isValid() const { return m_isValid.load(); }

private:
    GLFWContext();
    static void handleGLFWError(int error, const char* description);

private:
    std::atomic<bool> m_isValid{ true };
    static std::weak_ptr<GLFWContext> s_instance;
    static std::mutex s_mutex;
};

class Window {
public:
    // Event types for better type safety
    using ResizeEvent = Event<int, int>;
    using FocusEvent = Event<bool>;
    using MinimizeEvent = Event<bool>;
    using CloseEvent = Event<>;

    struct CreateInfo {
        std::string title = "Window";
        Settings::WindowMode mode = Settings::WindowMode::Windowed;
        Settings::Resolution resolution = Settings::Resolution::R_1280x720;
        bool resizable = true;
    };

public:
    explicit Window(const CreateInfo& createInfo = {});
    ~Window();

    // Non-copyable, movable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    // === Core Interface ===
    GLFWwindow* get() const { return m_window; }
    bool shouldClose() const;
    void pollEvents(); // Remove const - now processes batched events
    VkExtent2D extent() const;

    // Window state
    bool isMinimized() const;
    bool isFocused() const;
    std::pair<int, int> getSize() const;
    std::pair<int, int> getPosition() const;

    // Settings management
    void setWindowMode(Settings::WindowMode mode);
    void setResolution(Settings::Resolution resolution);
    void setTitle(const std::string& title);

    Settings::WindowMode getWindowMode() const { return m_currentMode; }
    Settings::Resolution getResolution() const { return m_currentResolution; }
    const std::string& getTitle() const { return m_title; }

    // === Events ===
    [[nodiscard]] ResizeEvent::Subscription onResize(ResizeEvent::Callback callback) {
        return m_resizeEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] FocusEvent::Subscription onFocus(FocusEvent::Callback callback) {
        return m_focusEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] MinimizeEvent::Subscription onMinimize(MinimizeEvent::Callback callback) {
        return m_minimizeEvent.subscribe(std::move(callback));
    }

    [[nodiscard]] CloseEvent::Subscription onClose(CloseEvent::Callback callback) {
        return m_closeEvent.subscribe(std::move(callback));
    }

    // Utility for automatic Settings integration
    void connectToSettings(Settings& settings);

private:
    void createWindow(const CreateInfo& createInfo);
    void setupCallbacks();
    void destroyWindow();

    void applyWindowMode(Settings::WindowMode mode);
    void applyResolution(Settings::Resolution resolution);

    // Process batched events
    void processBatchedEvents();

    Settings::ResolutionInfo getResolutionInfo(Settings::Resolution resolution) const;

    // Safe GLFW operations - check context validity before operations
    bool isGLFWValid() const;

    // GLFW callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void windowFocusCallback(GLFWwindow* window, int focused);
    static void windowIconifyCallback(GLFWwindow* window, int iconified);
    static void windowCloseCallback(GLFWwindow* window);

private:
    std::shared_ptr<GLFWContext> m_glfwContext; // Keep GLFW alive for this window
    GLFWwindow* m_window = nullptr;
    std::string m_title;
    Settings::WindowMode m_currentMode;
    Settings::Resolution m_currentResolution;
    bool m_resizable;

    // Events - simple direct members, no shared_ptr complexity
    ResizeEvent m_resizeEvent;
    FocusEvent m_focusEvent;
    MinimizeEvent m_minimizeEvent;
    CloseEvent m_closeEvent;

    // Batched resize event handling
    struct {
        std::atomic<bool> pending{ false };
        std::atomic<int> width{ 0 };
        std::atomic<int> height{ 0 };
    } m_pendingResize;

    struct {
        std::atomic<bool> pending{ false };
        std::atomic<bool> focused{ false };
    } m_pendingFocus;

    // Settings integration
    std::optional<Settings::SettingChangedEvent::Subscription> m_settingsSubscription;
};

// Helper functions for resolution mapping
namespace WindowUtils {
    Settings::ResolutionInfo getResolutionInfo(Settings::Resolution resolution);
    const std::unordered_map<Settings::Resolution, Settings::ResolutionInfo>& getResolutionMap();
}