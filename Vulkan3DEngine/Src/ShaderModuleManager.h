#pragma once
#include "LogicalDevice.h"
#include "ShaderModule.h"
#include "Handle.h"
#include <vector>
#include <memory>
#include <unordered_map>

class ShaderModuleManager {
public:
    ShaderModuleManager(const LogicalDevice& device);
    ~ShaderModuleManager();

    // Shader module creation and management
    ShaderModuleHandle createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode);
    void destroyModule(ShaderModuleHandle handle);

    // Accessors
    ShaderModule* getModule(ShaderModuleHandle handle);

    // Disable copy operations
    ShaderModuleManager(const ShaderModuleManager&) = delete;
    ShaderModuleManager& operator=(const ShaderModuleManager&) = delete;

private:
    const LogicalDevice& m_device;
    std::unordered_map<ShaderModuleHandle, std::unique_ptr<ShaderModule>> m_modules;
    uint32_t m_nextModuleHandle = 1;
};