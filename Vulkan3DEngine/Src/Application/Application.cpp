#include "Application.h"

float Application::s_deltaTime = 0.0f;

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
    try
    {
        InputSystem::create();
    }
    catch (...) {
        throw std::exception("Failed to create InputSystem");
    }
}

Application::~Application()
{
    GraphicsEngine::release();
}

void Application::run()
{
    m_scene = std::make_shared<Scene>();


    while (!m_window->shouldClose()) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        
        glfwPollEvents();

        update();
        draw();

        auto currentTime = std::chrono::high_resolution_clock::now();
        Application::s_deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    }
    vkDeviceWaitIdle(GraphicsEngine::get()->getRenderer()->getDevice()->get());
}

void Application::update()
{
    GraphicsEngine::get()->getTextureManager()->updateResources();
    GraphicsEngine::get()->getMeshManager()->updateResources();

    m_scene->update();
    
}

void Application::draw()
{
    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    
    m_scene->draw();

    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}

void Application::onKeyDown(int key)
{
}

void Application::onKeyUp(int key)
{
}

void Application::onMouseMove(const glm::vec2& mouse_pos)
{
}

void Application::onLeftMouseDown(const glm::vec2& mouse_pos)
{
}

void Application::onLeftMouseUp(const glm::vec2& mouse_pos)
{
}

void Application::onRightMouseDown(const glm::vec2& mouse_pos)
{
}

void Application::onRightMouseUp(const glm::vec2& mouse_pos)
{
}
