#include "Application.h"
#include "InputSystem.h"
#include "Camera.h"
#include <thread>

float Application::s_deltaTime = 0.0f;
//uint32_t Application::s_window_width = 1280;
//uint32_t Application::s_window_height = 720;

Application::Application()
{
    //m_window = std::make_shared<Window>(s_window_width, s_window_height, "Vulkan3DEngine");
    try
    {
        size_t numThreads = std::thread::hardware_concurrency();
        ThreadPool::create(numThreads);
    }
    catch (...) {
        throw std::exception("Failed to create ThreadPool");
    }
    try
    {
        InputSystem::create();
    }
    catch (...) {
        throw std::exception("Failed to create InputSystem");
    }
    try 
    {
        GraphicsEngine::create();
        GraphicsEngine::get()->init();
    }
    catch (...) {
        throw std::exception("Failed to create GraphicsEngine");
    }
}

Application::~Application()
{
    m_scene.reset();
    GraphicsEngine::release();
    InputSystem::release();
}

void Application::run()
{
    GraphicsEngine::get()->getTextureManager()->updateResources();
    GraphicsEngine::get()->getMeshManager()->updateResources();
    GraphicsEngine::get()->getModelDataManager()->updateResources();

    m_scene = std::make_shared<Scene>();

    auto startTime = std::chrono::high_resolution_clock::now();

    while (!GraphicsEngine::get()->getWindow()->shouldClose()) {
        
        
        glfwPollEvents();

        //if (m_window->wasWindowResized()) {
        //    s_window_width = m_window->getExtent().width;
        //    s_window_height = m_window->getExtent().height;
        //}

        if(GraphicsEngine::get()->getWindow()->isFocused()) {
            InputSystem::get()->addListener(m_scene->getCamera().get());
            InputSystem::get()->showCursor(false);
        }
        else {
            InputSystem::get()->removeListener(m_scene->getCamera().get());
            InputSystem::get()->showCursor(true);
        }

        InputSystem::get()->update();
        update();
        draw();

        auto currentTime = std::chrono::high_resolution_clock::now();
        Application::s_deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        startTime = currentTime; // Update startTime for the next frame
    }
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
}

void Application::update()
{
    GraphicsEngine::get()->getTextureManager()->updateResources();
    GraphicsEngine::get()->getMeshManager()->updateResources();
    GraphicsEngine::get()->getModelDataManager()->updateResources();

    m_scene->update();
}

void Application::draw()
{
    if (GraphicsEngine::get()->getWindow()->isMinimized()) {
        return;
    }

    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    
    m_scene->draw();

    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}
