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
    m_showExitPopup = false;
}

Application::~Application()
{
    GraphicsEngine::release();
    InputSystem::release();
}

void Application::run()
{
    GraphicsEngine* graphicsEngine = GraphicsEngine::get();
    WindowPtr window = graphicsEngine->getWindow();
    InputSystem* inputSystem = InputSystem::get();
    ScenePtr scene = graphicsEngine->getScene();
    Camera* camera = scene->getCamera().get();

    graphicsEngine->getTextureManager()->updateResources();
    graphicsEngine->getMeshManager()->updateResources();
    graphicsEngine->getModelDataManager()->updateResources();

    auto startTime = std::chrono::high_resolution_clock::now();

    while (!window->shouldClose()) {
        glfwPollEvents();

        if (window->isFocused()) {
            inputSystem->addListener(this);
            inputSystem->addListener(camera);
        }
        else {
            inputSystem->removeListener(this);
            inputSystem->removeListener(camera);
        }

        inputSystem->update();
        inputSystem->showCursor(s_cursor_mode);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        update();
        draw();

        auto currentTime = std::chrono::high_resolution_clock::now();
        Application::s_deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        startTime = currentTime; // Update startTime for the next frame
    }
    vkDeviceWaitIdle(graphicsEngine->getDevice()->get());
}

void Application::update()
{
    if (m_showExitPopup) {
        return;
    }
    GraphicsEngine::get()->getTextureManager()->updateResources();
    GraphicsEngine::get()->getMeshManager()->updateResources();
    GraphicsEngine::get()->getModelDataManager()->updateResources();

    GraphicsEngine::get()->getScene()->update();
}

void Application::draw()
{
    if (GraphicsEngine::get()->getWindow()->isMinimized()) {
        return;
    }

    GraphicsEngine::get()->getScene()->draw();

    drawFpsCounter();
    if (s_cursor_mode && !m_showExitPopup) {
        ImGui::ShowDemoWindow();
    }
    if (m_showExitPopup) {
        drawExitPopup();
    }

    GraphicsEngine::get()->getRenderer()->drawFrame();
}

void Application::drawFpsCounter()
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
}

void Application::drawExitPopup()
{
    ImGui::OpenPopup("Exit Popup");
    if (ImGui::BeginPopupModal("Exit Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to quit?");
        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            glfwSetWindowShouldClose(GraphicsEngine::get()->getWindow()->get(), true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0)))
        {
            m_showExitPopup = false;
            ImGui::CloseCurrentPopup();
            s_cursor_mode = false;
            InputSystem::get()->showCursor(!s_cursor_mode);
        }
        ImGui::EndPopup();
    }
}

void Application::onKeyDown(int key)
{
}

void Application::onKeyUp(int key)
{
    if (key == 112)//Key code for F1
    {
        s_cursor_mode = (s_cursor_mode) ? false : true;
        InputSystem::get()->showCursor(!s_cursor_mode);
    }
    else if (key == 27)//Key code for ESC
    {
        if (!m_showExitPopup) {
            m_showExitPopup = true;
            s_cursor_mode = true;
            InputSystem::get()->showCursor(!s_cursor_mode);
        }
        else {
            m_showExitPopup = false;
            ImGui::CloseCurrentPopup();
            s_cursor_mode = false;
            InputSystem::get()->showCursor(!s_cursor_mode);
        }
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
