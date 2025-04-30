#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Event.h"
#include "Settings.h"
#include <string>
#include <memory>

class Window
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

    auto onResize(typename Event<int, int>::Callback callback) { return m_resizeEvent->subscribe(callback); }
    auto onFocus(typename Event<bool>::Callback callback) { return m_focusEvent->subscribe(callback); }
    auto onMinimize(typename Event<bool>::Callback callback) { return m_minimizeEvent->subscribe(callback); }
    auto onClose(typename Event<>::Callback callback) { return m_closeEvent->subscribe(callback); }

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

    std::shared_ptr<Event<int, int>> m_resizeEvent;
    std::shared_ptr<Event<bool>> m_focusEvent;
    std::shared_ptr<Event<bool>> m_minimizeEvent;
    std::shared_ptr<Event<>> m_closeEvent;

    std::unique_ptr<Event<Settings::WindowMode>::Subscription> m_windowModeSubscription;
    std::unique_ptr<Event<Settings::Resolution>::Subscription> m_resolutionSubscription;
};