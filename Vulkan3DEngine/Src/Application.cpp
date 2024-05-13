#include "Application.h"
#include "InputSystem.h"
#include "Camera.h"
#include <thread>

float Application::s_deltaTime = 0.0f;
bool Application::s_cursor_mode = false;

Application::Application()
{
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
    }
    catch (...) {
        throw std::exception("Failed to create GraphicsEngine");
    }
    GraphicsEngine::get()->init();

    InputSystem::get()->addListener(this);
    InputSystem::get()->showCursor(false);
    s_cursor_mode = false;
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


        if (GraphicsEngine::get()->getWindow()->isFocused()) {
            InputSystem::get()->addListener(this);
            InputSystem::get()->addListener(m_scene->getCamera().get());
        }
        else {
            InputSystem::get()->removeListener(this);
            InputSystem::get()->removeListener(m_scene->getCamera().get());
        }

        InputSystem::get()->update();

        InputSystem::get()->showCursor(s_cursor_mode);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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

    drawUI();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 0);

    GraphicsEngine::get()->getRenderer()->drawFrameEnd();
}

void Application::drawUI()
{

    // Ustaw pozycjê i rozmiar okna ImGui
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    //ImGui::SetNextWindowSize(ImVec2(180, 120), ImGuiCond_Always);

    // Utwórz okno ImGui
    ImGui::Begin("FPS Counter", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);

    // Oblicz FPS
    static float fps = 0.0f;
    fps = 1.0f / s_deltaTime;

    // Wyœwietl FPS
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.3f ms", s_deltaTime * 1000.0f); // przelicz na milisekundy

    // Dodaj wykres FPS
    static float fpsHistory[60] = { 0 };
    static int historyIndex = 0;
    fpsHistory[historyIndex] = fps;
    historyIndex = (historyIndex + 1) % 60;

    ImGui::Text("FPS Chart");
    ImGui::PlotLines("", fpsHistory, 60, 0, NULL, 0.0f, 60.0f, ImVec2(160, 30)); 

    // Zakoñcz okno ImGui
    ImGui::End();

    if (s_cursor_mode) {
        ImGui::ShowDemoWindow();
    }
}

void Application::onKeyDown(int key)
{
}

void Application::onKeyUp(int key)
{
    if (key == 112)
    {
        s_cursor_mode = (s_cursor_mode) ? false : true;
        InputSystem::get()->showCursor(s_cursor_mode);
        fmt::print("F1 wcisniete");
    }
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
