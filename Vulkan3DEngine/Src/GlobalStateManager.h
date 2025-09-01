#pragma once
#include <memory>
#include <vector>
#include "RenderOrder.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <vulkan/vulkan.h>

#include "Registry.h"
#include "Renderer.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "DescriptorWriter.h"
#include "DescriptorLayoutManager.h"
#include "LogicalDevice.h"
#include "LightRenderOrder.h"
#include "CameraRenderOrder.h"
#include "MeshRenderOrder.h"

// Forward declarations
class Registry;
class Renderer;
class UniformBufferManager;
class DescriptorAllocator;
class DescriptorLayoutManager;
class LogicalDevice;
class Buffer;

class GlobalStateManager {
public:
    GlobalStateManager(Registry& registry, Renderer& renderer);

    // Process camera data and store for the frame
    void processCamera(std::shared_ptr<CameraRenderOrder> camera);

    // Process light data and add to lights list
    void processLight(std::shared_ptr<LightRenderOrder> light);

    // Create global uniform buffer with smart handle
    SmartHandle<UniformBufferHandle, Buffer> createGlobalUniformBuffer();

    // Build global data (uniform buffer, descriptor set)
    void buildGlobalData();

    // Apply global data to a mesh render order
    void applyGlobalDataToMesh(std::shared_ptr<MeshRenderOrder> mesh);

    // Reset state for next frame
    void reset();

    // Accessor
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> getGlobalDescriptorSet() const { return m_globalDescriptorSet; }

    // Check if global data is ready
    bool hasValidGlobalData() const { return m_globalUBO.isValid() && m_globalDescriptorSet.isValid(); }

private:
    Registry& m_registry;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    const LogicalDevice& m_device;
    DescriptorWriter m_writer;

    // Global state data - używamy SmartHandle dla automatycznego zarządzania
    std::shared_ptr<CameraRenderOrder> m_activeCamera;
    std::vector<std::shared_ptr<LightRenderOrder>> m_lights;
    SmartHandle<UniformBufferHandle, Buffer> m_globalUBO;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_globalDescriptorSet;

    // Flags to track state
    bool m_globalDataBuilt = false;

    // Helper method to create global descriptor set
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createGlobalDescriptorSet();
};