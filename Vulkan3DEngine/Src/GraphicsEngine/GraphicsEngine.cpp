#include "GraphicsEngine.h"

GraphicsEngine* GraphicsEngine::m_engine = nullptr;

GraphicsEngine::GraphicsEngine(WindowPtr window)
{
    try
    {
        m_device = new Device(window);
    }
    catch (...) { throw std::exception("Device not created successfully"); }
    try
    {
        m_renderer = new Renderer(*m_device);
    }
    catch (...) { throw std::exception("Renderer not created successfully"); }
}

GraphicsEngine::~GraphicsEngine()
{
    GraphicsEngine::m_engine = nullptr;
    delete m_renderer;
    delete m_device;
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

