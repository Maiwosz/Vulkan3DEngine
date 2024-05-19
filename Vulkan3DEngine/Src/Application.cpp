#include "Application.h"
#include "InputSystem.h"
#include "Camera.h"
#include "SceneObjectManager.h"
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
    auto pool = ThreadPool::get();
    std::vector<std::future<void>> futures;
    
    futures.push_back(pool->enqueue([] {
        GraphicsEngine::get()->getTextureManager()->updateResources();
        }));
    
    futures.push_back(pool->enqueue([] {
        GraphicsEngine::get()->getMeshManager()->updateResources();
        }));
    
    futures.push_back(pool->enqueue([] {
        GraphicsEngine::get()->getModelDataManager()->updateResources();
        }));
    
    for (auto& future : futures) {
        future.get(); // Synchronize tasks
    }

    GraphicsEngine::get()->getScene()->update();
}

void Application::draw()
{
    if (GraphicsEngine::get()->getWindow()->isMinimized()) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    GraphicsEngine::get()->getScene()->draw();

    drawFpsCounter();
    if (s_cursor_mode && !m_showExitPopup) {
        //ImGui::ShowDemoWindow();
        GraphicsEngine::get()->getScene()->getSceneObjectManager()->drawInterface();
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

    // Utwórz okno ImGui
    ImGui::Begin("FPS Counter", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);

    // Oblicz FPS
    static float fps = 0.0f;
    static float frameTime = 0.0f;
    static int frameCount = 0;
    static float maxFps = FLT_MIN;
    static float lastTime = 0.0f;
    static std::vector<float> fpsList; // Lista do przechowywania historii FPS

    float currentTime = ImGui::GetTime();
    frameCount++;
    if (currentTime - lastTime >= 1.0f) { // Aktualizuj dane co sekundê
        fps = frameCount;
        frameTime = 1000.0f / fps;

        frameCount = 0;
        lastTime += 1.0f;

        maxFps = std::max(maxFps, fps);

        // Dodaj aktualne FPS do listy
        fpsList.push_back(fps);
        if (fpsList.size() > 1000) { // Ogranicz rozmiar listy do 1000
            fpsList.erase(fpsList.begin());
        }
    }

    // Wyœwietl FPS
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.3f ms", frameTime); // przelicz na milisekundy

    // Dodaj wykres FPS
    static float fpsHistory[60] = { 0 };
    static int historyIndex = 0;
    fpsHistory[historyIndex] = fps;
    historyIndex = (historyIndex + 1) % 60;

    ImGui::Text("FPS Chart");
    ImGui::PlotLines("", fpsHistory, 60, 0, NULL, 0, maxFps, ImVec2(160, 30));

    // Oblicz i wyœwietl 1% i 0.1% najni¿szych wartoœci FPS
    if (!fpsList.empty()) {
        std::sort(fpsList.begin(), fpsList.end());
        int onePercentIndex = std::max(0, (int)(fpsList.size() * 0.01f));
        int pointOnePercentIndex = std::max(0, (int)(fpsList.size() * 0.001f));
        ImGui::Text("1%% Lows: %.1f FPS", fpsList[onePercentIndex]);
        ImGui::Text("0.1%% Lows: %.1f FPS", fpsList[pointOnePercentIndex]);
    }

    // Zakoñcz okno ImGui
    ImGui::End();
}


void Application::drawExitPopup()
{
    ImGui::OpenPopup("Exit Popup");
    if (ImGui::BeginPopupModal("Exit Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImVec2 window_pos = ImVec2(GraphicsEngine::get()->getWindow()->getExtent().width / 2,
            GraphicsEngine::get()->getWindow()->getExtent().height / 2);
        ImVec2 window_pos_pivot = ImVec2(0.5f, 0.5f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

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
