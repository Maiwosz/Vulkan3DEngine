#include "Application.h"

Application::Application()
{
    m_window = std::make_shared<Window>(s_window_width, s_window_height, "Vulkan3DEngine");
    try 
    {
        GraphicsEngine::create(m_window);
    }
    catch (...) {
        throw std::exception("Failed to create GraphicsEngine");
    }
}

Application::~Application()
{
    GraphicsEngine::release();
}

void Application::run()
{
    while (!m_window->shouldClose()) {
        glfwPollEvents();
        GraphicsEngine::get()->getRenderer()->drawFrame();
    }
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
}