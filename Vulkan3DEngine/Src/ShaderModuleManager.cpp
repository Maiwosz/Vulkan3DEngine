#include "ShaderModuleManager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

ShaderModuleManager::ShaderModuleManager(const LogicalDevice& device)
    : m_device(device) {
}

ShaderModuleManager::~ShaderModuleManager() {
    // Cleanup all shader modules
    m_modules.clear();
}

ShaderModuleHandle ShaderModuleManager::createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode) {
    ShaderModuleHandle handle(m_nextModuleHandle++);

    try {
        auto module = std::make_unique<ShaderModule>(m_device, spirvCode);
        m_modules[handle] = std::move(module);
        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create shader module: {}", e.what());
        return ShaderModuleHandle{};
    }
}

void ShaderModuleManager::destroyModule(ShaderModuleHandle handle) {
    auto it = m_modules.find(handle);
    if (it != m_modules.end()) {
        m_modules.erase(it);
    }
}

ShaderModule* ShaderModuleManager::getModule(ShaderModuleHandle handle) {
    auto it = m_modules.find(handle);
    if (it != m_modules.end()) {
        return it->second.get();
    }
    SPDLOG_WARN("Shader module handle not found: {}", handle.id);
    return nullptr;
}