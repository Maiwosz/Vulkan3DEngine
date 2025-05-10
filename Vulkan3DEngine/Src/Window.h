#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Event.h"
#include "Settings.h"
#include <string>
#include <memory>
#include <atomic>

class Window : public std::enable_shared_from_this<Window>
{
public:
    explicit Window(const char* title, Settings::WindowMode initialMode, Settings::Resolution initialResolution);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* get() const { return m_window; }
    VkExtent2D extent() const;
    void pollEvents() const { glfwPollEvents(); }

    bool shouldClose() const { return glfwWindowShouldClose(m_window); }

    typename Event<int, int>::Subscription onResize(typename Event<int, int>::Callback callback) {
        // Only add callbacks if we're not in the process of destruction
        if (m_isDestroying.load() || !m_resizeEvent || !m_resizeEvent->isActive()) {
            return typename Event<int, int>::Subscription();
        }
        return m_resizeEvent->subscribe(std::move(callback));
    }

    typename Event<bool>::Subscription onFocus(typename Event<bool>::Callback callback) {
        // Only add callbacks if we're not in the process of destruction
        if (m_isDestroying.load() || !m_focusEvent || !m_focusEvent->isActive()) {
            return typename Event<bool>::Subscription();
        }
        return m_focusEvent->subscribe(std::move(callback));
    }

    typename Event<bool>::Subscription onMinimize(typename Event<bool>::Callback callback) {
        // Only add callbacks if we're not in the process of destruction
        if (m_isDestroying.load() || !m_minimizeEvent || !m_minimizeEvent->isActive()) {
            return typename Event<bool>::Subscription();
        }
        return m_minimizeEvent->subscribe(std::move(callback));
    }

    typename Event<>::Subscription onClose(typename Event<>::Callback callback) {
        // Only add callbacks if we're not in the process of destruction
        if (m_isDestroying.load() || !m_closeEvent || !m_closeEvent->isActive()) {
            return typename Event<>::Subscription();
        }
        return m_closeEvent->subscribe(std::move(callback));
    }

private:
    void initGLFW();
    void createWindow();
    void setupCallbacks();
    static void handleGLFWError(int error, const char* description);

    void onWindowModeChanged(Settings::WindowMode newMode);
    void onResolutionChanged(Settings::Resolution newRes);
    void applyWindowMode(Settings::WindowMode newMode);
    void applyResolution(Settings::Resolution newRes);

    GLFWwindow* m_window = nullptr;
    std::string m_title;
    Settings::WindowMode m_currentMode;
    Settings::Resolution m_currentResolution;

    // Flag to indicate the window is being destroyed
    std::atomic<bool> m_isDestroying{ false };

    std::shared_ptr<Event<int, int>> m_resizeEvent;
    std::shared_ptr<Event<bool>> m_focusEvent;
    std::shared_ptr<Event<bool>> m_minimizeEvent;
    std::shared_ptr<Event<>> m_closeEvent;
};