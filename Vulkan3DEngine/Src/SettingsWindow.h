#pragma once

#include <memory>
#include "Settings.h"

class SettingsWindow {
public:
    explicit SettingsWindow(Settings& settings);
    ~SettingsWindow() = default;

    void render();

    bool m_showWindow = false;

private:
    Settings& m_settings;
    Settings::SettingsBundle m_pendingSettings;
    bool m_hasChanges = false;

    // UI helper methods
    void renderLogSettings();
    void renderDisplaySettings();
    void renderGraphicsSettings();
    void renderActionButtons();

    // Utility methods
    void loadCurrentSettings();
    void applyChanges();
    void discardChanges();
    void resetToDefaults();

    template<typename T>
    bool renderComboSetting(const char* label, T& value, const std::vector<T>& options);

    bool renderCheckboxSetting(const char* label, bool& value);
    bool renderSliderSetting(const char* label, uint32_t& value, uint32_t min, uint32_t max);

    // Hardware limit validation methods
    bool isAnisotropyLevelSupported(Settings::AnisotropyLevel level) const;
    bool isMsaaSampleCountSupported(Settings::MsaaSampleCount samples) const;
    bool isTextureFilteringSupported(Settings::TextureFiltering filtering) const;
};