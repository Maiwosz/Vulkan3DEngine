#include "GraphicsEngine.h"

GraphicsEngine* GraphicsEngine::m_engine = nullptr;

GraphicsEngine::GraphicsEngine()
{
    
}

GraphicsEngine::~GraphicsEngine()
{
    m_scene.reset();
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

void GraphicsEngine::create()
{
    if (GraphicsEngine::m_engine) throw std::exception("Graphics Engine already created");
    GraphicsEngine::m_engine = new GraphicsEngine();
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
    //try
    //{
    //    m_window = std::make_shared<Window>(Window::Resolution::R_1280x720, "Vulkan3DEngine", Window::Mode::Windowed);
    //}
    //catch (...) { throw std::exception("Window not created successfully"); }
    try
    {
        Instance::create();
    }
    catch (...) {
        throw std::exception("Failed to create Instance");
    }
    try
    {
        Window::get()->createSurface();
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
    try
    {
        m_scene = std::make_shared<Scene>();
    }
    catch (...) { throw std::exception("Scene not created successfully"); }
}

