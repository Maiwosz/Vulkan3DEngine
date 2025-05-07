#include "Settings.h"
#include <fstream>
#include <iostream>
#include <json.hpp>

// Mapowania dla enumów
const std::map<Settings::LogLevel, std::string> Settings::LOG_LEVEL_MAP = {
    {LogLevel::Trace,    "Trace"},
    {LogLevel::Debug,    "Debug"},
    {LogLevel::Info,     "Info"},
    {LogLevel::Warn,     "Warn"},
    {LogLevel::Error,    "Error"},
    {LogLevel::Critical, "Critical"},
    {LogLevel::Off,      "Off"}
};

const std::map<Settings::Resolution, Settings::ResolutionDetails> Settings::RESOLUTION_MAP = {
    {Resolution::R_640x480,    {640, 480, "640x480"}},
    {Resolution::R_800x600,    {800, 600, "800x600"}},
    {Resolution::R_1024x768,   {1024, 768, "1024x768"}},
    {Resolution::R_1280x720,   {1280, 720, "1280x720"}},
    {Resolution::R_1366x768,   {1366, 768, "1366x768"}},
    {Resolution::R_1600x900,   {1600, 900, "1600x900"}},
    {Resolution::R_1920x1080,  {1920, 1080, "1920x1080"}},
};

const std::map<Settings::MsaaSampleCount, uint32_t> Settings::MSAA_SAMPLE_MAP = {
    {MsaaSampleCount::Samples1,  1},
    {MsaaSampleCount::Samples2,  2},
    {MsaaSampleCount::Samples4,  4},
    {MsaaSampleCount::Samples8,  8},
    {MsaaSampleCount::Samples16, 16},
    {MsaaSampleCount::Samples32, 32},
    {MsaaSampleCount::Samples64, 64},
};

const std::map<Settings::WindowMode, std::string> Settings::WINDOW_MODE_MAP = {
    {WindowMode::Windowed, "Windowed"},
    {WindowMode::Fullscreen, "Fullscreen"},
    {WindowMode::Borderless, "Borderless"}
};

const std::map<Settings::TextureFiltering, std::string> Settings::TEXTURE_FILTERING_MAP = {
    {TextureFiltering::None, "None"},
    {TextureFiltering::Bilinear, "Bilinear"},
    {TextureFiltering::Trilinear, "Trilinear"},
    {TextureFiltering::Anisotropic, "Anisotropic"}
};

const std::map<Settings::MipmapMode, std::string> Settings::MIPMAP_MODE_MAP = {
    {MipmapMode::Nearest, "Nearest"},
    {MipmapMode::Linear, "Linear"}
};

const std::map<Settings::AnisotropyLevel, uint32_t> Settings::ANISOTROPY_LEVEL_MAP = {
    {AnisotropyLevel::X1, 1},
    {AnisotropyLevel::X2, 2},
    {AnisotropyLevel::X4, 4},
    {AnisotropyLevel::X8, 8},
    {AnisotropyLevel::X16, 16}
};

Settings::Settings()
    : m_logLevel(DEFAULT_LOG_LEVEL),
    m_windowMode(DEFAULT_WINDOW_MODE),
    m_resolution(DEFAULT_RESOLUTION),
    m_vsyncEnabled(DEFAULT_VSYNC_ENABLED),
    m_textureFiltering(DEFAULT_TEXTURE_FILTER_MODE),
    m_mipmapMode(DEFAULT_MIPMAP_MODE),
    m_anisotropyLevel(DEFAULT_ANISOTROPY_LEVEL),
    m_msaaSamples(DEFAULT_MSAA_SAMPLES),
    m_framesInFlight(DEFAULT_FRAMES_IN_FLIGHT),
    m_maxMsaaSamples(MsaaSampleCount::Samples1),
    m_maxAnisotropy(1.0f),
    m_anisotropySupported(false)
{

}


void Settings::setLogLevel(LogLevel level) {
    if (m_logLevel != level) {
        SPDLOG_INFO("Changing log level from {} to {}",
            LOG_LEVEL_MAP.at(m_logLevel), LOG_LEVEL_MAP.at(level));
        m_logLevel = level;
    }
}

void Settings::setHardwareLimits(MsaaSampleCount maxMsaa, float maxAnisotropy, bool anisotropySupported) {
    m_maxMsaaSamples = maxMsaa;
    m_maxAnisotropy = maxAnisotropy;
    m_anisotropySupported = anisotropySupported;
    m_hardwareLimitsSet = true;

    SPDLOG_INFO("Hardware limits: Max MSAA samples = {}x, Max Anisotropy = {}, Anisotropy Supported = {}",
        MSAA_SAMPLE_MAP.at(maxMsaa), maxAnisotropy, anisotropySupported ? "Yes" : "No");

    setMsaaSamples(m_msaaSamples);
    setTextureFiltering(m_textureFiltering);
    setAnisotropyLevel(m_anisotropyLevel); // Validate current anisotropy level
}

void Settings::setWindowMode(WindowMode mode) {
    if (m_windowMode != mode) {
        SPDLOG_INFO("Window mode changed: {} → {}",
            WINDOW_MODE_MAP.at(m_windowMode), WINDOW_MODE_MAP.at(mode));
        m_windowMode = mode;
    }
}

void Settings::setResolution(Resolution resolution) {
    if (m_resolution != resolution) {
        auto oldRes = RESOLUTION_MAP.at(m_resolution);
        auto newRes = RESOLUTION_MAP.at(resolution);
        SPDLOG_INFO("Changing resolution from {}x{} to {}x{}",
            oldRes.width, oldRes.height, newRes.width, newRes.height);
        m_resolution = resolution;
    }
}

void Settings::setVsyncEnabled(bool enabled) {
    if (m_vsyncEnabled != enabled) {
        SPDLOG_INFO("Changing VSync from {} to {}",
            m_vsyncEnabled ? "enabled" : "disabled", enabled ? "enabled" : "disabled");
        m_vsyncEnabled = enabled;
    }
}

void Settings::setTextureFiltering(TextureFiltering filtering) {
    // Sprawdzenie czy anizotropowe filtrowanie jest wspierane
    if (filtering == TextureFiltering::Anisotropic && !m_anisotropySupported) {
        SPDLOG_WARN("Warning: Anisotropic filtering not supported, falling back to Trilinear");
        filtering = TextureFiltering::Trilinear;
    }

    if (m_textureFiltering != filtering) {
        SPDLOG_INFO("Changing texture filtering from {} to {}",
            TEXTURE_FILTERING_MAP.at(m_textureFiltering), TEXTURE_FILTERING_MAP.at(filtering));
        m_textureFiltering = filtering;
    }
}

void Settings::setMsaaSamples(MsaaSampleCount samples) {
    // Clampowanie do wspieranego maximum
    MsaaSampleCount original = samples;
    if (static_cast<int>(samples) > static_cast<int>(m_maxMsaaSamples)) {
        samples = m_maxMsaaSamples;
        SPDLOG_WARN("Warning: Requested MSAA sample count ({}x) exceeds maximum supported, clamping to {}x",
            MSAA_SAMPLE_MAP.at(original), MSAA_SAMPLE_MAP.at(samples));
    }

    if (m_msaaSamples != samples) {
        SPDLOG_INFO("Changing MSAA samples from {}x to {}x",
            MSAA_SAMPLE_MAP.at(m_msaaSamples), MSAA_SAMPLE_MAP.at(samples));
        m_msaaSamples = samples;
    }
}

void Settings::setFramesInFlight(uint32_t count) {
    uint32_t original = count;
    // Minimum 1, maximum rozsądnych wartości
    if (count < 1) {
        count = 1;
        SPDLOG_WARN("Warning: Frames in flight must be at least 1, clamping from {} to {}", original, count);
    }
    if (count > 3) {
        count = 3; // Typowy limit dla większości implementacji
        SPDLOG_WARN("Warning: Frames in flight clamped from {} to {} (typical maximum for most implementations)", original, count);
    }

    if (m_framesInFlight != count) {
        SPDLOG_INFO("Changing frames in flight from {} to {}", m_framesInFlight, count);
        m_framesInFlight = count;
    }
}

void Settings::setMipmapMode(MipmapMode mode) {
    if (m_mipmapMode != mode) {
        SPDLOG_INFO("Changing mipmap mode from {} to {}",
            MIPMAP_MODE_MAP.at(m_mipmapMode), MIPMAP_MODE_MAP.at(mode));
        m_mipmapMode = mode;
    }
}

void Settings::setAnisotropyLevel(AnisotropyLevel level) {
    // Clamp to hardware max if anisotropy is supported
    AnisotropyLevel original = level;
    float levelValue = static_cast<float>(ANISOTROPY_LEVEL_MAP.at(level));

    if (m_anisotropySupported && levelValue > m_maxAnisotropy) {
        // Find the highest supported level
        if (m_maxAnisotropy >= 16.0f) level = AnisotropyLevel::X16;
        else if (m_maxAnisotropy >= 8.0f) level = AnisotropyLevel::X8;
        else if (m_maxAnisotropy >= 4.0f) level = AnisotropyLevel::X4;
        else if (m_maxAnisotropy >= 2.0f) level = AnisotropyLevel::X2;
        else level = AnisotropyLevel::X1;

        SPDLOG_WARN("Warning: Requested anisotropy level ({}x) exceeds maximum supported ({}), clamping to {}x",
            ANISOTROPY_LEVEL_MAP.at(original), m_maxAnisotropy, ANISOTROPY_LEVEL_MAP.at(level));
    }

    if (m_anisotropyLevel != level) {
        SPDLOG_INFO("Changing anisotropy level from {}x to {}x",
            ANISOTROPY_LEVEL_MAP.at(m_anisotropyLevel), ANISOTROPY_LEVEL_MAP.at(level));
        m_anisotropyLevel = level;
    }
}

// Add the implementation of getCurrentAnisotropyLevel
float Settings::getCurrentAnisotropyLevel() const {
    if (m_textureFiltering == TextureFiltering::Anisotropic && m_anisotropySupported) {
        return static_cast<float>(ANISOTROPY_LEVEL_MAP.at(m_anisotropyLevel));
    }
    return 1.0f; // No anisotropy
}

Settings::ResolutionDetails Settings::getCurrentResolutionDetails() const {
    return RESOLUTION_MAP.at(m_resolution);
}

uint32_t Settings::getCurrentMsaaSampleCount() const {
    return MSAA_SAMPLE_MAP.at(m_msaaSamples);
}

bool Settings::saveToFile(const std::string& filename) {
    try {
        nlohmann::json j;
        j["logLevel"] = static_cast<int>(m_logLevel);
        j["windowMode"] = static_cast<int>(m_windowMode);
        j["resolution"] = static_cast<int>(m_resolution);
        j["vsyncEnabled"] = m_vsyncEnabled;
        j["textureFiltering"] = static_cast<int>(m_textureFiltering);
        j["mipmapMode"] = static_cast<int>(m_mipmapMode);          // Add new setting
        j["anisotropyLevel"] = static_cast<int>(m_anisotropyLevel); // Add new setting
        j["msaaSamples"] = static_cast<int>(m_msaaSamples);
        j["framesInFlight"] = m_framesInFlight;

        std::ofstream o(filename);
        o << std::setw(4) << j << std::endl;
        SPDLOG_INFO("Settings saved to {}", filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save settings: {}", e.what());
        return false;
    }
}

bool Settings::loadFromFile(const std::string& filename) {
    try {
        std::ifstream i(filename);
        if (!i.is_open()) {
            SPDLOG_WARN("Settings file {} not found", filename);
            return false;
        }

        SPDLOG_INFO("Loading settings from {}", filename);
        nlohmann::json j;
        i >> j;

        // Load values without directly invoking setters
        auto logLevel = static_cast<LogLevel>(j["logLevel"].get<int>());
        auto windowMode = static_cast<WindowMode>(j["windowMode"].get<int>());
        auto resolution = static_cast<Resolution>(j["resolution"].get<int>());
        auto vsyncEnabled = j["vsyncEnabled"].get<bool>();
        auto textureFiltering = static_cast<TextureFiltering>(j["textureFiltering"].get<int>());

        // Load new settings with backwards compatibility check
        MipmapMode mipmapMode = DEFAULT_MIPMAP_MODE;
        if (j.contains("mipmapMode")) {
            mipmapMode = static_cast<MipmapMode>(j["mipmapMode"].get<int>());
        }

        AnisotropyLevel anisotropyLevel = DEFAULT_ANISOTROPY_LEVEL;
        if (j.contains("anisotropyLevel")) {
            anisotropyLevel = static_cast<AnisotropyLevel>(j["anisotropyLevel"].get<int>());
        }

        auto msaaSamples = static_cast<MsaaSampleCount>(j["msaaSamples"].get<int>());
        auto framesInFlight = j["framesInFlight"].get<uint32_t>();

        // Set values directly to fields (without clamping)
        m_logLevel = logLevel;
        m_windowMode = windowMode;
        m_resolution = resolution;
        m_vsyncEnabled = vsyncEnabled;
        m_textureFiltering = textureFiltering;
        m_mipmapMode = mipmapMode;           // Set new field
        m_anisotropyLevel = anisotropyLevel; // Set new field
        m_msaaSamples = msaaSamples;
        m_framesInFlight = framesInFlight;

        // Apply hardware limits if already set
        if (m_hardwareLimitsSet) {
            setMsaaSamples(msaaSamples);
            setTextureFiltering(textureFiltering);
            setAnisotropyLevel(anisotropyLevel); // Validate anisotropy level
        }

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load settings: {}", e.what());
        return false;
    }
}

spdlog::level::level_enum Settings::convertLogLevel(Settings::LogLevel level)
{
    switch (level) {
    case Settings::LogLevel::Trace:    return spdlog::level::trace;
    case Settings::LogLevel::Debug:    return spdlog::level::debug;
    case Settings::LogLevel::Info:     return spdlog::level::info;
    case Settings::LogLevel::Warn:     return spdlog::level::warn;
    case Settings::LogLevel::Error:    return spdlog::level::err;
    case Settings::LogLevel::Critical: return spdlog::level::critical;
    case Settings::LogLevel::Off:      return spdlog::level::off;
    default:                           return spdlog::level::info;
    }
}
