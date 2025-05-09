#pragma once
#include <memory>
#include <vector>
#include "RenderOrder.h"
#include "UniformBufferHandle.h"
#include <vulkan/vulkan.h>

// Forward declarations
class Registry;
class Renderer;
class UniformBufferManager;
class DescriptorAllocator;
class DescriptorLayoutManager;
class LogicalDevice;

class GlobalStateManager {
public:
    GlobalStateManager(Registry& registry, Renderer& renderer);

    // Process camera data and store for the frame
    void processCamera(std::shared_ptr<CameraRenderOrder> camera);

    // Process light data and add to lights list
    void processLight(std::shared_ptr<LightRenderOrder> light);

    UniformBufferHandle createGlobalUniformBuffer();

    // Build global data (uniform buffer, descriptor set)
    void buildGlobalData();

    // Apply global data to a mesh render order
    void applyGlobalDataToMesh(std::shared_ptr<MeshRenderOrder> mesh);

    // Reset state for next frame
    void reset();

    // Accessors
    std::shared_ptr<CameraRenderOrder> getActiveCamera() const { return m_activeCamera; }
    const std::vector<std::shared_ptr<LightRenderOrder>>& getLights() const { return m_lights; }
    UniformBufferHandle getGlobalUBO() const { return m_globalUBO; }

private:
    Registry& m_registry;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    const LogicalDevice& m_device;

    // Global state data
    std::shared_ptr<CameraRenderOrder> m_activeCamera;
    std::vector<std::shared_ptr<LightRenderOrder>> m_lights;
    UniformBufferHandle m_globalUBO;

    // Flags to track state
    bool m_globalDataBuilt = false;
};