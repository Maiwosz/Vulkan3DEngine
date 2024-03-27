#include "GraphicsEngine.h"

GraphicsEngine* GraphicsEngine::m_engine = nullptr;

GraphicsEngine::GraphicsEngine(WindowPtr window): m_window(window)
{
    try
    {
        m_device = std::make_shared<Device>(window);
    }
    catch (...) { throw std::exception("Device not created successfully"); }
    try
    {
        m_renderer = std::make_shared<Renderer>(m_device);
    }
    catch (...) { throw std::exception("Renderer not created successfully"); }
}

GraphicsEngine::~GraphicsEngine()
{
    GraphicsEngine::m_engine = nullptr;
}

GraphicsEngine* GraphicsEngine::get()
{
    return m_engine;
}

void GraphicsEngine::create(WindowPtr window)
{
    if (GraphicsEngine::m_engine) throw std::exception("Graphics Engine already created");
    GraphicsEngine::m_engine = new GraphicsEngine(window);
}

void GraphicsEngine::release()
{
    if (!GraphicsEngine::m_engine)
    {
        return;
    }
    delete GraphicsEngine::m_engine;
}

