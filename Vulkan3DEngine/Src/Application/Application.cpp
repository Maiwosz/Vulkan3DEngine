#include "Application.h"

Application::Application()
{
    m_window = std::make_unique<Window>(s_window_width, s_window_height, "Vulkan3DEngine");
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
