#include "SettingsWindow.h"
#include "imgui.h"
#include <spdlog/spdlog.h>

SettingsWindow::SettingsWindow(Settings& settings)
    : m_settings(settings)
    , m_hasChanges(false)
{
    loadCurrentSettings();
}

void SettingsWindow::render() {
    if (!m_showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &m_showWindow)) {

        if (ImGui::BeginTabBar("SettingsTabs")) {

            if (ImGui::BeginTabItem("General")) {
                renderLogSettings();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Display")) {
                renderDisplaySettings();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Graphics")) {
                renderGraphicsSettings();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        renderActionButtons();
    }
    ImGui::End();
}

void SettingsWindow::renderLogSettings() {
    ImGui::Text("Logging");
    ImGui::Separator();

    std::vector<Settings::LogLevel> logLevels = {
        Settings::LogLevel::Trace,
        Settings::LogLevel::Debug,
        Settings::LogLevel::Info,
        Settings::LogLevel::Warn,
        Settings::LogLevel::Error,
        Settings::LogLevel::Critical,
        Settings::LogLevel::Off
    };

    if (renderComboSetting("Log Level", m_pendingSettings.logLevel, logLevels)) {
        m_hasChanges = true;
    }
}

void SettingsWindow::renderDisplaySettings() {
    ImGui::Text("Display Settings");
    ImGui::Separator();

    std::vector<Settings::WindowMode> windowModes = {
        Settings::WindowMode::Windowed,
        Settings::WindowMode::Fullscreen,
        Settings::WindowMode::Borderless
    };

    if (renderComboSetting("Window Mode", m_pendingSettings.windowMode, windowModes)) {
        m_hasChanges = true;
    }

    std::vector<Settings::Resolution> resolutions = {
        Settings::Resolution::R_640x480,
        Settings::Resolution::R_800x600,
        Settings::Resolution::R_1024x768,
        Settings::Resolution::R_1280x720,
        Settings::Resolution::R_1366x768,
        Settings::Resolution::R_1600x900,
        Settings::Resolution::R_1920x1080
    };

    if (renderComboSetting("Resolution", m_pendingSettings.resolution, resolutions)) {
        m_hasChanges = true;
    }

    if (renderCheckboxSetting("VSync", m_pendingSettings.vsyncEnabled)) {
        m_hasChanges = true;
    }
}

void SettingsWindow::renderGraphicsSettings() {
    ImGui::Text("Graphics Settings");
    ImGui::Separator();

    std::vector<Settings::TextureFiltering> textureFiltering = {
        Settings::TextureFiltering::None,
        Settings::TextureFiltering::Bilinear,
        Settings::TextureFiltering::Trilinear,
        Settings::TextureFiltering::Anisotropic
    };

    if (renderComboSetting("Texture Filtering", m_pendingSettings.textureFiltering, textureFiltering)) {
        m_hasChanges = true;
    }

    std::vector<Settings::MipmapMode> mipmapModes = {
        Settings::MipmapMode::Nearest,
        Settings::MipmapMode::Linear
    };

    if (renderComboSetting("Mipmap Mode", m_pendingSettings.mipmapMode, mipmapModes)) {
        m_hasChanges = true;
    }

    std::vector<Settings::AnisotropyLevel> anisotropyLevels = {
        Settings::AnisotropyLevel::X1,
        Settings::AnisotropyLevel::X2,
        Settings::AnisotropyLevel::X4,
        Settings::AnisotropyLevel::X8,
        Settings::AnisotropyLevel::X16
    };

    if (renderComboSetting("Anisotropy Level", m_pendingSettings.anisotropyLevel, anisotropyLevels)) {
        m_hasChanges = true;
    }

    std::vector<Settings::MsaaSampleCount> msaaSamples = {
        Settings::MsaaSampleCount::Samples1,
        Settings::MsaaSampleCount::Samples2,
        Settings::MsaaSampleCount::Samples4,
        Settings::MsaaSampleCount::Samples8,
        Settings::MsaaSampleCount::Samples16,
        Settings::MsaaSampleCount::Samples32,
        Settings::MsaaSampleCount::Samples64
    };

    if (renderComboSetting("MSAA Samples", m_pendingSettings.msaaSamples, msaaSamples)) {
        m_hasChanges = true;
    }

    if (renderSliderSetting("Frames in Flight", m_pendingSettings.framesInFlight, 1, 3)) {
        m_hasChanges = true;
    }
}

void SettingsWindow::renderActionButtons() {
    // Show warning if there are unsaved changes
    if (m_hasChanges) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        ImGui::Text("You have unsaved changes");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    if (ImGui::Button("Apply", ImVec2(80, 0))) {
        applyChanges();
    }

    ImGui::SameLine();

    if (ImGui::Button("OK", ImVec2(80, 0))) {
        applyChanges();
        m_showWindow = false;
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        discardChanges();
        m_showWindow = false;
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset to Defaults", ImVec2(120, 0))) {
        resetToDefaults();
    }
}

void SettingsWindow::loadCurrentSettings() {
    m_pendingSettings = m_settings.getAllSettings();
    m_hasChanges = false;
}

void SettingsWindow::applyChanges() {
    if (m_hasChanges) {
        m_settings.applySettings(m_pendingSettings);
		m_settings.save();
        m_hasChanges = false;
        SPDLOG_INFO("Settings applied successfully");
    }
}

void SettingsWindow::discardChanges() {
    loadCurrentSettings();
}

void SettingsWindow::resetToDefaults() {
    m_pendingSettings = Settings::getDefaultSettings();
    m_hasChanges = true;
}

template<typename T>
bool SettingsWindow::renderComboSetting(const char* label, T& value, const std::vector<T>& options) {
    bool changed = false;

    // Find current index
    int currentIndex = 0;
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == value) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    // Create preview text
    std::string preview = Settings::toString(value);

    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (size_t i = 0; i < options.size(); ++i) {
            bool isSelected = (i == static_cast<size_t>(currentIndex));
            std::string optionText = Settings::toString(options[i]);

            // Check if this option is supported by hardware
            bool isSupported = true;
            if constexpr (std::is_same_v<T, Settings::AnisotropyLevel>) {
                isSupported = isAnisotropyLevelSupported(options[i]);
            }
            else if constexpr (std::is_same_v<T, Settings::MsaaSampleCount>) {
                isSupported = isMsaaSampleCountSupported(options[i]);
            }
            else if constexpr (std::is_same_v<T, Settings::TextureFiltering>) {
                isSupported = isTextureFilteringSupported(options[i]);
            }

            // Gray out unsupported options
            if (!isSupported) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                optionText += " (Unsupported)";
            }

            if (ImGui::Selectable(optionText.c_str(), isSelected, isSupported ? 0 : ImGuiSelectableFlags_Disabled)) {
                if (isSupported) {
                    value = options[i];
                    changed = true;
                }
            }

            if (!isSupported) {
                ImGui::PopStyleColor();
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

bool SettingsWindow::renderCheckboxSetting(const char* label, bool& value) {
    return ImGui::Checkbox(label, &value);
}

bool SettingsWindow::renderSliderSetting(const char* label, uint32_t& value, uint32_t min, uint32_t max) {
    int intValue = static_cast<int>(value);
    bool changed = ImGui::SliderInt(label, &intValue, static_cast<int>(min), static_cast<int>(max));
    if (changed) {
        value = static_cast<uint32_t>(intValue);
    }
    return changed;
}

// Hardware limit validation methods
bool SettingsWindow::isAnisotropyLevelSupported(Settings::AnisotropyLevel level) const {
    if (!m_settings.hasHardwareLimits()) {
        return true; // If no limits set, assume all are supported
    }

    const auto& limits = m_settings.getHardwareLimits();
    if (!limits.anisotropySupported) {
        return level == Settings::AnisotropyLevel::X1; // Only 1x is supported when anisotropy is not supported
    }

    float levelValue = static_cast<float>(level);
    return levelValue <= limits.maxAnisotropy;
}

bool SettingsWindow::isMsaaSampleCountSupported(Settings::MsaaSampleCount samples) const {
    if (!m_settings.hasHardwareLimits()) {
        return true; // If no limits set, assume all are supported
    }

    const auto& limits = m_settings.getHardwareLimits();
    return static_cast<int>(samples) <= static_cast<int>(limits.maxMsaa);
}

bool SettingsWindow::isTextureFilteringSupported(Settings::TextureFiltering filtering) const {
    if (!m_settings.hasHardwareLimits()) {
        return true; // If no limits set, assume all are supported
    }

    const auto& limits = m_settings.getHardwareLimits();
    if (filtering == Settings::TextureFiltering::Anisotropic && !limits.anisotropySupported) {
        return false;
    }

    return true;
}