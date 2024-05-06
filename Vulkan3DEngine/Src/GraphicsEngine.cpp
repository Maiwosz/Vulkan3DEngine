#include "GraphicsEngine.h"

GraphicsEngine* GraphicsEngine::m_engine = nullptr;

GraphicsEngine::GraphicsEngine()
{
    
}

GraphicsEngine::~GraphicsEngine()
{
    m_modelDataManager.reset();
    m_textureManager.reset();
    m_meshManager.reset();
    m_renderer.reset();
    GraphicsEngine::m_engine = nullptr;
}

GraphicsEngine* GraphicsEngine::get()
{
    return m_engine;
}

void GraphicsEngine::create(/*WindowPtr window*/)
{
    if (GraphicsEngine::m_engine) throw std::exception("Graphics Engine already created");
    GraphicsEngine::m_engine = new GraphicsEngine(/*window*/);
}

void GraphicsEngine::release()
{
    if (!GraphicsEngine::m_engine)
    {
        return;
    }
    delete GraphicsEngine::m_engine;
}

void GraphicsEngine::init()
{
    try
    {
        m_window = std::make_shared<Window>(m_window_width, m_window_height, "Vulkan3DEngine");
    }
    catch (...) { throw std::exception("Window not created successfully"); }
    try
    {
        Instance::create();
    }
    catch (...) {
        throw std::exception("Failed to create Instance");
    }
    try
    {
        m_window->createSurface();
    }
    catch (...) {
        throw std::exception("Failed to create Surface");
    }
    try
    {
        m_device = std::make_shared<Device>();
    }
    catch (...) { throw std::exception("Device not created successfully"); }
    try
    {
        m_renderer = std::make_shared<Renderer>();
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

