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
    m_statue1 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue1->setTranslation(3.0f, 0.0f, 0.0f);
    m_statue1->setRotationY(0.0f);

    m_statue2 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue2->setTranslation(1.0f, 0.0f, 0.0f);
    m_statue2->setRotationY(90.0f);

    m_statue3 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue3->setTranslation(-1.0f, 0.0f, 0.0f);
    m_statue3->setRotationY(180.0f);

    m_statue4 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue4->setTranslation(-3.0f, 0.0f, 0.0f);
    m_statue4->setRotationY(270.0f);

    //m_hygieia = GraphicsEngine::get()->getModelManager()->createModelInstance("Hygieia.JSON");
    //
    //m_hygieia->setTranslation(-1.0f, 0.0f, 0.0f);
    //m_hygieia->setRotationY(-90.0f);

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

    GraphicsEngine::get()->getRenderer()->updateUniformBuffer();

    // Define the rotation speed
    float rotationSpeed = 45.0f;
    m_statue1->setRotationY(Application::s_deltaTime * rotationSpeed);
    m_statue2->setRotationY(Application::s_deltaTime * rotationSpeed + 90);
    m_statue3->setRotationY(Application::s_deltaTime * rotationSpeed + 180);
    m_statue4->setRotationY(Application::s_deltaTime * rotationSpeed + 270);
    //m_hygieia->setRotationY(Application::s_deltaTime * rotationSpeed);

    //rect->update();
    m_statue1->update();
    m_statue2->update();
    m_statue3->update();
    m_statue4->update();
    //m_vikingRoom->update();
    //m_castle->update();
    //m_hygieia->update();
}

void Application::draw()
{
    GraphicsEngine::get()->getRenderer()->drawFrameBegin();
    //rect->draw();
    m_statue1->draw();
    m_statue2->draw();
    m_statue3->draw();
    m_statue4->draw();
    //m_vikingRoom->draw();
    //m_castle->draw();
    //m_hygieia->draw();
    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}
