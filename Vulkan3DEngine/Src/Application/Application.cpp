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
    delete triangle;
    GraphicsEngine::release();
}

void Application::run()
{
    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    triangle = new Object(vertices);

    while (!m_window->shouldClose()) {
        glfwPollEvents();

        update();
        draw();
    }
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
}

void Application::update()
{
}

void Application::draw()
{
    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    triangle->draw();
    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}
