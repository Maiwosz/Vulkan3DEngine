#include "PipelineLayoutManager.h"
#include <stdexcept>
#include "PipelineLayoutHandle.h"

PipelineLayoutManager::PipelineLayoutManager(const LogicalDevice& device)
    : m_device(device) {
}

PipelineLayoutManager::~PipelineLayoutManager() {
    // Clean up all layouts
    for (const auto& [handle, layout] : m_layouts) {
        vkDestroyPipelineLayout(m_device.get(), layout, nullptr);
    }
    m_layouts.clear();
}

PipelineLayoutHandle PipelineLayoutManager::createLayout(const PipelineLayoutConfig& config) {
    // Check if we already have this layout configuration
    auto cacheIt = m_layoutCache.find(config);
    if (cacheIt != m_layoutCache.end()) {
        // Update usage tracking
        updateLastUsed(cacheIt->second);
        return cacheIt->second;
    }

    // Create new pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // Set descriptor set layouts
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(config.descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = config.descriptorSetLayouts.empty() ? nullptr : config.descriptorSetLayouts.data();

    // Set push constant ranges
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(config.pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = config.pushConstantRanges.empty() ? nullptr : config.pushConstantRanges.data();

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(m_device.get(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Create handle and store layout
    PipelineLayoutHandle handle(m_nextHandle++);
    m_layouts[handle] = pipelineLayout;
    m_layoutCache[config] = handle;

    // Update usage tracking
    updateLastUsed(handle);

    return handle;
}

void PipelineLayoutManager::destroy(PipelineLayoutHandle handle) {
    if (!isValid(handle)) return;

    // Find the config corresponding to this handle
    for (auto it = m_layoutCache.begin(); it != m_layoutCache.end(); ++it) {
        if (it->second == handle) {
            m_layoutCache.erase(it);
            break;
        }
    }

    // Remove usage tracking
    m_layoutLastUsed.erase(handle);

    // Destroy the layout
    vkDestroyPipelineLayout(m_device.get(), m_layouts[handle], nullptr);

    // Remove the layout from the map
    m_layouts.erase(handle);
}

VkPipelineLayout PipelineLayoutManager::get(PipelineLayoutHandle handle) {
    if (!isValid(handle)) {
        throw std::runtime_error("Invalid pipeline layout handle");
    }

    // Update usage tracking
    updateLastUsed(handle);

    return m_layouts[handle];
}

bool PipelineLayoutManager::isValid(PipelineLayoutHandle handle) const {
    return m_layouts.find(handle) != m_layouts.end();
}

void PipelineLayoutManager::advanceFrame() {
    m_currentFrame++;
}

void PipelineLayoutManager::purgeUnusedLayouts(uint64_t ageThresholdFrames) {
    std::vector<PipelineLayoutHandle> layoutsToRemove;

    // Find layouts that haven't been used in a while
    for (const auto& [handle, lastUsedFrame] : m_layoutLastUsed) {
        if (m_currentFrame - lastUsedFrame > ageThresholdFrames) {
            layoutsToRemove.push_back(handle);
        }
    }

    // Remove the unused layouts
    for (const auto& handle : layoutsToRemove) {
        destroy(handle);
    }
}

void PipelineLayoutManager::updateLastUsed(PipelineLayoutHandle handle) {
    m_layoutLastUsed[handle] = m_currentFrame;
}