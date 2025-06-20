#include "RenderSystem.h"

RenderSystem::RenderSystem(Registry& registry, AssetSystem& assetSystem, Renderer& renderer, Settings& settings)
    : m_registry(registry), m_assetSystem(assetSystem), m_renderer(renderer), m_settings(settings) {

    m_globalStateManager = std::make_unique<GlobalStateManager>(registry, renderer);

    initializePipeline();
}

void RenderSystem::initializePipeline() {
    // Create pipeline stages
    m_assetResolutionStage = std::make_shared<AssetResolutionStage>(m_registry, m_assetSystem);
    m_uniformBufferStage = std::make_shared<UniformBufferStage>(m_registry, m_renderer, m_assetSystem);
    m_descriptorSetStage = std::make_shared<DescriptorSetStage>(m_renderer, m_assetSystem);
    m_pipelineAssignmentStage = std::make_shared<PipelineAssignmentStage>(m_renderer, m_assetSystem, m_settings);
    m_renderStage = std::make_shared<RenderStage>(m_renderer, m_assetSystem);

    // Connect stages
    m_assetResolutionStage->connectTo(RenderOrderType::Mesh, m_uniformBufferStage);
    m_uniformBufferStage->connectTo(RenderOrderType::Mesh, m_descriptorSetStage);
    m_descriptorSetStage->connectTo(RenderOrderType::Mesh, m_pipelineAssignmentStage);
    m_pipelineAssignmentStage->connectTo(RenderOrderType::Mesh, m_renderStage);

}

void RenderSystem::submitRenderOrder(std::shared_ptr<RenderOrder> order) {
    m_pendingOrders.push_back(std::move(order));
}

void RenderSystem::submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders) {
    m_pendingOrders.insert(m_pendingOrders.end(), orders.begin(), orders.end());
}

void RenderSystem::processOrders() {
    if (m_pendingOrders.empty()) {
        return;
    }

    try {
        // Reset global state manager for this frame
        m_globalStateManager->reset();

        // Sort orders: Cameras first, then Lights, then Meshes
        std::stable_sort(m_pendingOrders.begin(), m_pendingOrders.end(),
            [](const std::shared_ptr<RenderOrder>& a, const std::shared_ptr<RenderOrder>& b) {
                static const std::unordered_map<RenderOrderType, int> priority{
                    {RenderOrderType::Camera, 0},
                    {RenderOrderType::Light, 1},
                    {RenderOrderType::Mesh, 2}
                };
                return priority.at(a->getType()) < priority.at(b->getType());
            });

        // First pass: Process cameras and lights
        for (const auto& order : m_pendingOrders) {
            if (order->getType() == RenderOrderType::Camera) {
                auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
                m_globalStateManager->processCamera(cameraOrder);
            }
            else if (order->getType() == RenderOrderType::Light) {
                auto lightOrder = std::static_pointer_cast<LightRenderOrder>(order);
                m_globalStateManager->processLight(lightOrder);
            }
        }

        // Build global data after processing all cameras and lights
        m_globalStateManager->buildGlobalData();

        // Process mesh render orders with asset resolution stage
        std::vector<std::shared_ptr<RenderOrder>> meshOrders;
        for (const auto& order : m_pendingOrders) {
            if (order->getType() == RenderOrderType::Mesh) {
                // Process through asset resolution
                // Apply global data
                auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);
                m_globalStateManager->applyGlobalDataToMesh(meshOrder);
                m_assetResolutionStage->process(order);
            }
        }

        m_pendingOrders.clear();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error processing render orders: {}", e.what());
        m_pendingOrders.clear();
        throw;
    }
}

void RenderSystem::prepareForNextFrame() {
    // Reset any per-frame state
    reset();

    // Additional frame preparation logic can be added here
}

void RenderSystem::reset() {
    // Clear current frame data
    m_renderer.frameManager().clearCurrentFrameOrders();
    m_pendingOrders.clear();
}

void RenderSystem::renderFrame() {
    try {
        //m_renderStage->createAndRenderDebugObject();

        // Execute the actual render commands
        m_renderStage->executeRenderPass();

        // Prepare for the next frame
        prepareForNextFrame();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error rendering frame: {}", e.what());
        // Attempt to reset for next frame even if rendering failed
        reset();
        throw; // Rethrow to be handled by engine
    }
}