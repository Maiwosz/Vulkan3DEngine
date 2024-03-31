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
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f},  {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f},   {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f},  {1.0f, 1.0f, 1.0f}}
    };

    const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

    rect_mesh = std::make_shared<Mesh>(vertices, indices);

    rect = std::make_shared<Object>(rect_mesh);

    while (!m_window->shouldClose()) {
        glfwPollEvents();

        update();
        draw();
    }
    vkDeviceWaitIdle(GraphicsEngine::get()->getRenderer()->getDevice()->get());
}

void Application::update()
{
}

void Application::draw()
{
    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    rect->draw();
    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}
