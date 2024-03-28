#include "Window.h"

Window::Window(int width, int height, const char* windowName): 
    m_width(width), m_height(height), m_windowName(windowName)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

Window::~Window()
{
    glfwDestroyWindow(m_window);

    glfwTerminate();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(m_window);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto m_window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    m_window->m_framebufferResized = true;
    m_window->m_width = width;
    m_window->m_height = height;
}