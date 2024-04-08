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
}

Application::~Application()
{
    GraphicsEngine::release();
}

void Application::run()
{
    //const std::vector<Vertex> vertices = {
    //    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    //    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    //    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    //    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    //    
    //    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    //    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    //    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    //    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    //};
    //
    //
    //const std::vector<uint16_t> indices = { 
    //    0, 1, 2, 2, 3, 0,
    //    4, 5, 6, 6, 7, 4 
    //};

    //rect_mesh = std::make_shared<Mesh>(vertices, indices);

    //m_cat_texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile("Assets\\Textures\\cat.jpg");

    //rect = std::make_shared<Object>(rect_mesh, m_cat_texture);

    //m_statue_texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile("Assets\\Textures\\statue.jpg");
    //m_statue_mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile("Assets\\Meshes\\statue.obj");
    //
    //m_statue = std::make_shared<Object>(m_statue_mesh, m_statue_texture);
    
    m_vikingRoom_texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile("Assets\\Textures\\viking_room.png");
    m_vikingRoom_mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile("Assets\\Meshes\\viking_room.obj");
    
    m_vikingRoom = std::make_shared<Object>(m_vikingRoom_mesh, m_vikingRoom_texture);

    //m_castle_texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile("Assets\\Textures\\castle.jpg");
    //m_castle_mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile("Assets\\Meshes\\castle.obj");
    //
    //m_castle = std::make_shared<Object>(m_castle_mesh, m_castle_texture);

    //m_hygieia_texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile("Assets\\Textures\\Hygieia_C.png");
    //m_hygieia_mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile("Assets\\Meshes\\Hygieia_C.obj");
    //
    //m_hygieia = std::make_shared<Object>(m_hygieia_mesh, m_hygieia_texture);

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
}

void Application::draw()
{
    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    //rect->draw();
    //m_statue->draw();
    m_vikingRoom->draw();
    //m_castle->draw();
    //m_hygieia->draw();
    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}
