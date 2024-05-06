#include "Window.h"
#include "Instance.h"

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << error << " - " << description << std::endl;
}

Window::Window(int width, int height, const char* windowName): 
    m_width(width), m_height(height), m_windowName(windowName)
{
    glfwSetErrorCallback(glfwErrorCallback);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);

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

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_framebufferResized = true;
    m_window->m_width = width;
    m_window->m_height = height;
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
