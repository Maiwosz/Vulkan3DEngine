#include "Window.h"
#include "Instance.h"

const std::map<Window::Resolution, Window::ResolutionDetails> Window::resolutionMap = {
    // Format 4:3
    { Window::Resolution::R_640x480, {640, 480} },
    { Window::Resolution::R_800x600, {800, 600} },
    { Window::Resolution::R_1024x768, {1024, 768} },

    // Format 16:9
    { Window::Resolution::R_1280x720, {1280, 720} },
    { Window::Resolution::R_1366x768, {1366, 768} },
    { Window::Resolution::R_1600x900, {1600, 900} },
    { Window::Resolution::R_1920x1080, {1920, 1080} }
};

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << error << " - " << description << std::endl;
}

Window::Window(Resolution resolution, const char* windowName, Mode mode)
    : m_resolution(resolution), m_windowName(windowName), m_mode(mode)
{
    glfwSetErrorCallback(glfwErrorCallback);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = nullptr;
    if (mode == Mode::Fullscreen || mode == Mode::Borderless) {
        monitor = glfwGetPrimaryMonitor();
    }

    // Pobierz wymiary dla wybranej rozdzielczoœci
    auto resolutionDetails = resolutionMap.at(resolution);

    m_window = glfwCreateWindow(resolutionDetails.width, resolutionDetails.height, windowName, monitor, nullptr);

    if (mode == Mode::Borderless) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetWindowFocusCallback(m_window, windowFocusCallback);
}


Window::~Window()
{
    if (m_surface) {
        vkDestroySurfaceKHR(Instance::get()->getVkInstance(), m_surface, nullptr);
    }
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Window::createSurface()
{
    if (glfwCreateWindowSurface(Instance::get()->getVkInstance(), m_window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Window::setMode(Mode mode)
{
    if (mode == m_mode) {
        return;
    }

    m_mode = mode;

    GLFWmonitor* monitor = nullptr;
    if (mode == Mode::Fullscreen || mode == Mode::Borderless) {
        monitor = glfwGetPrimaryMonitor();
    }

    if (mode == Mode::Borderless) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else {
        auto resolutionDetails = resolutionMap.at(m_resolution);
        glfwSetWindowMonitor(m_window, nullptr, 0, 0, resolutionDetails.width, resolutionDetails.height, 0);
    }
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_framebufferResized = true;
    //m_window->m_width = width;
    //m_window->m_height = height;
}

void Window::windowFocusCallback(GLFWwindow* window, int focused)
{
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_focused = focused;
}

void Window::windowIconifyCallback(GLFWwindow* window, int iconified)
{
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_minimized = iconified;
}
