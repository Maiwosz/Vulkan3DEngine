#include "WindowSystem.h"

WindowSystem::WindowSystem(int width, int height, const char* title)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

WindowSystem::~WindowSystem()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

bool WindowSystem::shouldClose()
{
    return glfwWindowShouldClose(window);
}
