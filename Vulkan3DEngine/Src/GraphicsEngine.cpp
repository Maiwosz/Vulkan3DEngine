#include "GraphicsEngine.h"

GraphicsEngine* GraphicsEngine::m_engine = nullptr;

GraphicsEngine::GraphicsEngine(WindowPtr window): m_window(window)
{
    try
    {
        m_renderer = std::make_shared<Renderer>(window);
    }
    catch (...) { throw std::exception("Renderer not created successfully"); }
    try
    {
        m_meshManager = std::make_shared<MeshManager>();
    }
    catch (...) { throw std::exception("MeshManager not created successfully"); }
    try
    {
        m_textureManager = std::make_shared<TextureManager>();
    }
    catch (...) { throw std::exception("TextureManager not created successfully"); }
    try
    {
        m_modelDataManager = std::make_shared<ModelDataManager>();
    }
    catch (...) { throw std::exception("ModelManager not created successfully"); }
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

