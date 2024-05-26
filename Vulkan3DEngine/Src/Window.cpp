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

Window* Window::m_window = nullptr;
Window::Mode Window::s_mode = Mode::Windowed;
Window::Resolution Window::s_resolution = Resolution::R_1280x720;

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << error << " - " << description << std::endl;
}

Window::Window(const char* windowName)
    : m_windowName(windowName)
{
    glfwSetErrorCallback(glfwErrorCallback);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = nullptr;
    if (s_mode == Mode::Fullscreen || s_mode == Mode::Borderless) {
        monitor = glfwGetPrimaryMonitor();
    }

    auto resolutionDetails = resolutionMap.at(s_resolution);

    m_glfwWindow = glfwCreateWindow(resolutionDetails.width, resolutionDetails.height, windowName, monitor, nullptr);

    if (s_mode == Mode::Borderless) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_glfwWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }

    glfwSetWindowUserPointer(m_glfwWindow, this);
    glfwSetFramebufferSizeCallback(m_glfwWindow, framebufferResizeCallback);
    glfwSetWindowFocusCallback(m_glfwWindow, windowFocusCallback);
    glfwSetWindowIconifyCallback(m_glfwWindow, windowIconifyCallback);
}

Window::~Window()
{
    if (m_surface) {
        vkDestroySurfaceKHR(Instance::get()->getVkInstance(), m_surface, nullptr);
    }
    glfwDestroyWindow(m_glfwWindow);
    glfwTerminate();
}

Window* Window::get()
{
    return m_window;
}

void Window::create(const char* windowName)
{
    if (Window::m_window) throw std::exception("Window already created");
    Window::m_window = new Window(windowName);
}

void Window::release()
{
    if (!Window::m_window)
    {
        return;
    }
    delete Window::m_window;
}

void Window::createSurface()
{
    if (glfwCreateWindowSurface(Instance::get()->getVkInstance(), m_glfwWindow, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Window::setMode(Mode mode) {
    s_mode = mode;
    // Zastosuj zmiany do okna
    if (mode == Mode::Fullscreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowMonitor(m_glfwWindow, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else if (mode == Mode::Windowed) {
        ResolutionDetails resolutionDetails = resolutionMap.at(s_resolution);
        glfwSetWindowMonitor(m_glfwWindow, nullptr, 0, 0, resolutionDetails.width, resolutionDetails.height, 0);
    }
    else if (mode == Mode::Borderless) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowMonitor(m_glfwWindow, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
    }
}

void Window::setResolution(Resolution resolution) {
    s_resolution = resolution;
    // Zastosuj zmiany do okna
    ResolutionDetails resolutionDetails = resolutionMap.at(s_resolution);
    glfwSetWindowSize(m_glfwWindow, resolutionDetails.width, resolutionDetails.height);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_framebufferResized = true;
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