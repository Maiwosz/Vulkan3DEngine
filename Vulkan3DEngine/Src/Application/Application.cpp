#include "Application.h"

Application::Application()
{
    m_window = std::make_unique<WindowSystem>(m_window_width, m_window_height, "Vulkan3DEngine");
}

Application::~Application()
{
}

void Application::run()
{
    while (!m_window->shouldClose()) {
        glfwPollEvents();
    }
}
