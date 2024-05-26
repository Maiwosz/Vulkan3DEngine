#include "Application.h"
#include "InputSystem.h"
#include "Camera.h"
#include "SceneObjectManager.h"
#include "Window.h"
#include <thread>

// Konwersja typów enum na string i odwrotnie
std::string modeToString(Window::Mode mode) {
    switch (mode) {
    case Window::Mode::Windowed: return "Windowed";
    case Window::Mode::Fullscreen: return "Fullscreen";
    case Window::Mode::Borderless: return "Borderless";
    default: return "Windowed";
    }
}

Window::Mode stringToMode(const std::string& str) {
    if (str == "Fullscreen") return Window::Mode::Fullscreen;
    if (str == "Borderless") return Window::Mode::Borderless;
    return Window::Mode::Windowed;
}

std::string resolutionToString(Window::Resolution resolution) {
    switch (resolution) {
    case Window::Resolution::R_640x480: return "640x480";
    case Window::Resolution::R_800x600: return "800x600";
    case Window::Resolution::R_1024x768: return "1024x768";
    case Window::Resolution::R_1280x720: return "1280x720";
    case Window::Resolution::R_1366x768: return "1366x768";
    case Window::Resolution::R_1600x900: return "1600x900";
    case Window::Resolution::R_1920x1080: return "1920x1080";
    default: return "1280x720";
    }
}

Window::Resolution stringToResolution(const std::string& str) {
    if (str == "640x480") return Window::Resolution::R_640x480;
    if (str == "800x600") return Window::Resolution::R_800x600;
    if (str == "1024x768") return Window::Resolution::R_1024x768;
    if (str == "1280x720") return Window::Resolution::R_1280x720;
    if (str == "1366x768") return Window::Resolution::R_1366x768;
    if (str == "1600x900") return Window::Resolution::R_1600x900;
    if (str == "1920x1080") return Window::Resolution::R_1920x1080;
    return Window::Resolution::R_1280x720;
}

// Konwersja typów flag MSAA na integer i odwrotnie
int msaaSamplesToInt(VkSampleCountFlagBits samples) {
    switch (samples) {
    case VK_SAMPLE_COUNT_1_BIT: return 1;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_8_BIT: return 8;
    case VK_SAMPLE_COUNT_16_BIT: return 16;
    case VK_SAMPLE_COUNT_32_BIT: return 32;
    case VK_SAMPLE_COUNT_64_BIT: return 64;
    default: return 1;
    }
}

VkSampleCountFlagBits intToMsaaSamples(int samples) {
    switch (samples) {
    case 2: return VK_SAMPLE_COUNT_2_BIT;
    case 4: return VK_SAMPLE_COUNT_4_BIT;
    case 8: return VK_SAMPLE_COUNT_8_BIT;
    case 16: return VK_SAMPLE_COUNT_16_BIT;
    case 32: return VK_SAMPLE_COUNT_32_BIT;
    case 64: return VK_SAMPLE_COUNT_64_BIT;
    default: return VK_SAMPLE_COUNT_1_BIT;
    }
}

float Application::s_deltaTime = 0.0f;
bool Application::s_cursor_mode = false;

Application::Application() : m_stopCheckingResources(false)
{
    try {
        loadSettings();
    }
    catch (...) {
        throw std::exception("Failed to load settings");
    }
    try {
        Window::create("Vulkan3DEngine");
    }
    catch (...) {
        throw std::exception("Failed to create Window");
    }
    try {
        size_t numThreads = std::thread::hardware_concurrency();
        ThreadPool::create(numThreads);
    }
    catch (...) {
        throw std::exception("Failed to create ThreadPool");
    }
    try {
        InputSystem::create();
    }
    catch (...) {
        throw std::exception("Failed to create InputSystem");
    }
    try {
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

    // Start resource checking thread
    m_checkResourcesThread = std::thread(&Application::checkAndReloadResources, this);
}

Application::~Application()
{
    // Stop the resource checking thread
    m_stopCheckingResources = true;
    if (m_checkResourcesThread.joinable()) {
        m_checkResourcesThread.join();
    }

    GraphicsEngine::release();
    InputSystem::release();
}


void Application::run()
{
    GraphicsEngine* graphicsEngine = GraphicsEngine::get();
    Window* window = Window::get();
    InputSystem* inputSystem = InputSystem::get();
    ScenePtr scene = graphicsEngine->getScene();
    Camera* camera = scene->getCamera().get();
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

        if (recreateSwapchain) {
            GraphicsEngine::get()->getRenderer()->getSwapChain()->recreateSwapChain();
            recreateSwapchain = false;
        }
        if (recreatePipelines) {
            GraphicsEngine::get()->getRenderer()->recreatePipelines();
            recreatePipelines = false;
        }
        if (recreateImgui) {
            GraphicsEngine::get()->getRenderer()->recreateImgui();
            recreateImgui = false;
        }
        if (reloadTextures) {
            GraphicsEngine::get()->getTextureManager()->reloadAllResources();
            reloadTextures = false;
        }

        if (scene->m_loadScene) {
            scene->loadScene(scene->m_path);
            scene->m_loadScene = false;
        }

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

    GraphicsEngine::get()->getScene()->update();
}


void Application::draw()
{
    if (Window::get()->isMinimized()) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    GraphicsEngine::get()->getScene()->draw();

    drawFpsCounter();
    if (s_cursor_mode && !m_showExitPopup) {
        //ImGui::ShowDemoWindow();
        drawInteface();
    }
    if (m_showExitPopup) {
        drawExitPopup();
    }

    GraphicsEngine::get()->getRenderer()->drawFrame();
}

void Application::loadSettings()
{
    std::string settingsFile = "settings.json";

    if (!std::filesystem::exists(settingsFile)) {
        // Inicjalizacja domyœlnych ustawieñ
        Renderer::s_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        Renderer::s_framesInFlight = 2;
        Window::s_mode = Window::Mode::Windowed;
        Window::s_resolution = Window::Resolution::R_1280x720;
        Camera::s_mouseSensitivity = 10.0f;
        Camera::s_fov = 45.0f;

        // Zapisz domyœlne ustawienia do pliku
        saveSettings();
    }
    else {
        // Wczytaj ustawienia z pliku
        std::ifstream i(settingsFile);
        nlohmann::json j;
        i >> j;

        Renderer::s_msaaSamples = intToMsaaSamples(j["msaaSamples"]);
        Renderer::s_framesInFlight = j["framesInFlight"];
        Window::s_mode = stringToMode(j["windowMode"]);
        Window::s_resolution = stringToResolution(j["resolution"]);
        Camera::s_mouseSensitivity = j["mouseSensitivity"];
        Camera::s_fov = j["fov"];
    }
}

void Application::saveSettings()
{
    nlohmann::json j;

    j["msaaSamples"] = msaaSamplesToInt(Renderer::s_msaaSamples);
    j["framesInFlight"] = Renderer::s_framesInFlight;
    j["windowMode"] = modeToString(Window::s_mode);
    j["resolution"] = resolutionToString(Window::s_resolution);
    j["mouseSensitivity"] = Camera::s_mouseSensitivity;
    j["fov"] = Camera::s_fov;

    std::ofstream o("settings.json");
    o << j << std::endl;
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
        ImVec2 window_pos = ImVec2(Window::get()->getExtent().width / 2,
            Window::get()->getExtent().height / 2);
        ImVec2 window_pos_pivot = ImVec2(0.5f, 0.5f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

        ImGui::Text("Are you sure you want to quit?");
        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            glfwSetWindowShouldClose(Window::get()->getWindow(), true);
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

void Application::drawInteface()
{
    if (ImGui::Begin("Window")) {
        if (ImGui::BeginTabBar("TabBar")) {
            createSettingsTab();
            GraphicsEngine::get()->getScene()->drawInterface();
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
}

void Application::createSettingsTab() {
    if (ImGui::BeginTabItem("Settings")) {
        static int framesInFlight = Renderer::s_framesInFlight - 1;
        static int windowMode = static_cast<int>(Window::s_mode);
        static int resolution = static_cast<int>(Window::s_resolution);
        static float mouseSensitivity = Camera::s_mouseSensitivity;
        static float fov = Camera::s_fov;

        static bool MouseSensitivityChanged = false;
        static bool fovChanged = false;
        static bool windowModeChanged = false;
        static bool resolutionChanged = false;
        static bool framesInFlightChanged = false;
        static bool msaaSamplesChanged = false;

        float newMouseSensitivity = mouseSensitivity;
        if (ImGui::SliderFloat("Mouse Sensitivity", &newMouseSensitivity, 1.0f, 100.0f, "%1.0f")) {
            if (static_cast<int>(newMouseSensitivity) != static_cast<int>(mouseSensitivity)) {
                mouseSensitivity = newMouseSensitivity;
                MouseSensitivityChanged = true;
            }
        }

        float newFov = fov;
        if (ImGui::SliderFloat("Field of View", &newFov, 30.0f, 90.0f, "%1.0f")) {
            if (static_cast<int>(newFov) != static_cast<int>(fov)) {
                fov = newFov;
                fovChanged = true;
            }
        }

        int newWindowMode = windowMode;
        const char* windowModeItems[] = { "Windowed", "Fullscreen", "Borderless" };
        if (ImGui::Combo("Window Mode", &newWindowMode, windowModeItems, IM_ARRAYSIZE(windowModeItems))) {
            if (newWindowMode != windowMode) {
                windowMode = newWindowMode;
                windowModeChanged = true;
            }
        }

        int newResolution = resolution;
        const char* resolutionItems[] = {
            "640x480", "800x600", "1024x768",
            "1280x720", "1366x768", "1600x900", "1920x1080"
        };
        if (ImGui::Combo("Resolution", &newResolution, resolutionItems, IM_ARRAYSIZE(resolutionItems))) {
            if (newResolution != resolution) {
                resolution = newResolution;
                resolutionChanged = true;
            }
        }

        int newFramesInFlight = framesInFlight;
        const char* framesInFlightItems[] = { "1", "2", "3" };
        if (ImGui::Combo("Frames in Flight", &newFramesInFlight, framesInFlightItems, IM_ARRAYSIZE(framesInFlightItems))) {
            if (newFramesInFlight != framesInFlight) {
                framesInFlight = newFramesInFlight;
                framesInFlightChanged = true;
            }
        }

        const char* msaaItems[] = { "1x", "2x", "4x", "8x", "16x" };
        VkSampleCountFlagBits msaaValues[] = { VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT };

        // Find the current MSAA index
        int msaaSamples = std::distance(msaaValues, std::find(msaaValues, msaaValues + IM_ARRAYSIZE(msaaValues), Renderer::s_msaaSamples));

        int newMsaaSamples = msaaSamples;
        int maxMsaaIndex = std::min(Renderer::s_maxMsaaSamples == VK_SAMPLE_COUNT_1_BIT ? 1 :
            Renderer::s_maxMsaaSamples == VK_SAMPLE_COUNT_2_BIT ? 2 :
            Renderer::s_maxMsaaSamples == VK_SAMPLE_COUNT_4_BIT ? 3 :
            Renderer::s_maxMsaaSamples == VK_SAMPLE_COUNT_8_BIT ? 4 : 5, IM_ARRAYSIZE(msaaItems));

        if (ImGui::Combo("MSAA Samples", &newMsaaSamples, msaaItems, maxMsaaIndex)) {
            if (newMsaaSamples != msaaSamples) {
                msaaSamples = newMsaaSamples;
                msaaSamplesChanged = true;
            }
        }

        if (ImGui::Button("Apply")) {
            if (MouseSensitivityChanged || fovChanged || windowModeChanged || resolutionChanged || framesInFlightChanged || msaaSamplesChanged) {
                fmt::print("Settings changed!\n");
            }
            if (MouseSensitivityChanged) {
                Camera::s_mouseSensitivity = mouseSensitivity;
                fmt::print("Mouse Sensitivity: {}\n", mouseSensitivity);
                MouseSensitivityChanged = false;
            }
            if (fovChanged) {
                Camera::s_fov = fov;
                fmt::print("FOV: {}\n", fov);
                fovChanged = false;
            }
            if (windowModeChanged) {
                Window::s_mode = static_cast<Window::Mode>(windowMode);
                Window::get()->setMode(static_cast<Window::Mode>(windowMode));
                fmt::print("Window Mode: {}\n", windowMode);
                windowModeChanged = false;
            }
            if (resolutionChanged) {
                Window::s_resolution = static_cast<Window::Resolution>(resolution);
                Window::get()->setResolution(static_cast<Window::Resolution>(resolution));
                fmt::print("Resolution: {}\n", resolution);
                recreateSwapchain = true;
                resolutionChanged = false;
            }
            if (framesInFlightChanged) {
                Renderer::s_framesInFlight = framesInFlight + 1;
                fmt::print("Frames in Flight: {}\n", framesInFlight);
                framesInFlightChanged = false;
            }
            if (msaaSamplesChanged) {
                Renderer::s_msaaSamples = msaaValues[msaaSamples];
                fmt::print("MSAA Samples: {}\n", msaaSamples);
                recreateSwapchain = true;
                recreatePipelines = true;
                recreateImgui = true;
                reloadTextures = true;
                msaaSamplesChanged = false;
            }

            saveSettings();
        }

        ImGui::EndTabItem();
    }
}


void Application::checkAndReloadResources()
{
    while (!m_stopCheckingResources) {
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

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for 1 second before checking again
    }
}


void Application::onKeyDown(int key)
{
}

void Application::onKeyUp(int key)
{
    if (key == GLFW_KEY_F1)
    {
        s_cursor_mode = (s_cursor_mode) ? false : true;
        InputSystem::get()->showCursor(!s_cursor_mode);
    }
    else if (key == GLFW_KEY_ESCAPE)
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
