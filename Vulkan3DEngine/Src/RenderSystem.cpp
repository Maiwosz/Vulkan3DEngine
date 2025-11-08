#include "RenderSystem.h"
#include "MeshRenderOrder.h"
#include "CameraRenderOrder.h"
#include "LightRenderOrder.h"
#include <spdlog/spdlog.h>

RenderSystem::RenderSystem(Registry& registry, AssetSystem& assetSystem, EngineCore& renderer, Settings& settings)
    : m_registry(registry), m_assetSystem(assetSystem), m_renderer(renderer), m_settings(settings) {

    // Create semaphore manager first
    m_semaphoreManager = std::make_unique<SemaphoreManager>();

    // Create processing context with semaphore manager
    m_processingContext = std::make_unique<ProcessingContext>(*m_semaphoreManager);

    // Initialize all processing stages first (RenderSystem owns them)
    initializeStages();

    // Then initialize pipelines using the owned stages
    initializePipelines();
}

void RenderSystem::initializeStages() {
    SPDLOG_DEBUG("Initializing processing stages...");

    // Asset processing stages
    m_assetResolutionStage = std::make_shared<AssetResolutionStage>(*m_processingContext, m_registry, m_assetSystem);

    // Buffer and descriptor stages
    m_uniformBufferStage = std::make_shared<UniformBufferStage>(*m_processingContext, m_registry, m_renderer, m_assetSystem);
    m_descriptorSetStage = std::make_shared<DescriptorSetStage>(*m_processingContext, m_renderer, m_assetSystem);

    // Storage stages
    m_meshStorageStage = std::make_shared<MeshStorageStage>(*m_processingContext);
    m_lightStorageStage = std::make_shared<LightStorageStage>(*m_processingContext);

    // Camera processing stages
    m_cameraProcessingStage = std::make_shared<CameraProcessingStage>(*m_processingContext, m_registry, m_renderer);
    m_renderPipelineAssignmentStage = std::make_shared<RenderPipelineAssignmentStage>(*m_processingContext, m_assetSystem, m_renderer, m_registry);
    m_meshCullingStage = std::make_shared<MeshCullingStage>(*m_processingContext, m_registry);

    // Pipeline assignment stage
    m_pipelineAssignmentStage = std::make_shared<PipelineAssignmentStage>(*m_processingContext, m_renderer, m_assetSystem, m_settings);

    // Final rendering stage
    m_renderStage = std::make_shared<RenderStage>(*m_processingContext, m_renderer, m_assetSystem);

    // Create semaphores for synchronization
    SemaphoreHandle lightsStoredHandle = m_semaphoreManager->createSemaphore("lights_stored", 0);
    SemaphoreHandle meshesStoredHandle = m_semaphoreManager->createSemaphore("meshes_stored", 0);

    // Synchronization stages with proper semaphore handles
    m_waitForLightsStage = std::make_shared<WaitForStage>(*m_processingContext, lightsStoredHandle, 1);
    m_waitForMeshesStage = std::make_shared<WaitForStage>(*m_processingContext, meshesStoredHandle, 1);
    m_notifyLightsStoredStage = std::make_shared<NotifyStage>(*m_processingContext, lightsStoredHandle, 1);
    m_notifyMeshesStoredStage = std::make_shared<NotifyStage>(*m_processingContext, meshesStoredHandle, 1);

    SPDLOG_DEBUG("All processing stages initialized successfully");
}

void RenderSystem::initializePipelines() {
    SPDLOG_DEBUG("Initializing processing pipelines...");

    // Create the three main pipelines using owned stages
    createMeshPipeline();
    createLightPipeline();
    createCameraPipeline();

    SPDLOG_DEBUG("All processing pipelines initialized successfully");
}

void RenderSystem::createMeshPipeline() {
    m_meshPipeline = std::make_unique<ProcessingPipeline>("MeshPipeline", RenderOrderType::Mesh, *m_processingContext);

    // Use owned stages - no new allocations
    m_meshPipeline->addStage(m_assetResolutionStage);
    m_meshPipeline->addStage(m_uniformBufferStage);
    m_meshPipeline->addStage(m_descriptorSetStage);
    m_meshPipeline->addStage(m_pipelineAssignmentStage);
    m_meshPipeline->addStage(m_meshStorageStage);
    m_meshPipeline->addStage(m_notifyMeshesStoredStage);

    // Register semaphores used by this pipeline
    m_meshPipeline->registerSemaphore(m_notifyMeshesStoredStage->getSemaphoreHandle());

    SPDLOG_DEBUG("Mesh pipeline created with {} stages and {} semaphores",
        m_meshPipeline->getStageCount(), m_meshPipeline->getSemaphoreCount());
}

void RenderSystem::createLightPipeline() {
    m_lightPipeline = std::make_unique<ProcessingPipeline>("LightPipeline", RenderOrderType::Light, *m_processingContext);

    // Use owned stages - no new allocations
    m_lightPipeline->addStage(m_lightStorageStage);
    m_lightPipeline->addStage(m_notifyLightsStoredStage);

    // Register semaphores used by this pipeline
    m_lightPipeline->registerSemaphore(m_notifyLightsStoredStage->getSemaphoreHandle());

    SPDLOG_DEBUG("Light pipeline created with {} stages and {} semaphores",
        m_lightPipeline->getStageCount(), m_lightPipeline->getSemaphoreCount());
}

void RenderSystem::createCameraPipeline() {
    m_cameraPipeline = std::make_unique<ProcessingPipeline>("CameraPipeline", RenderOrderType::Camera, *m_processingContext);

    // Use owned stages - no new allocations
    m_cameraPipeline->addStage(m_waitForLightsStage);
    m_cameraPipeline->addStage(m_cameraProcessingStage);
    m_cameraPipeline->addStage(m_descriptorSetStage);
    m_cameraPipeline->addStage(m_renderPipelineAssignmentStage);
    m_cameraPipeline->addStage(m_waitForMeshesStage);
    m_cameraPipeline->addStage(m_meshCullingStage);
    m_cameraPipeline->addStage(m_renderStage);

    // Register semaphores used by this pipeline
    m_cameraPipeline->registerSemaphore(m_waitForLightsStage->getSemaphoreHandle());
    m_cameraPipeline->registerSemaphore(m_waitForMeshesStage->getSemaphoreHandle());

    SPDLOG_DEBUG("Camera pipeline created with {} stages and {} semaphores",
        m_cameraPipeline->getStageCount(), m_cameraPipeline->getSemaphoreCount());
}

void RenderSystem::addOrderToCurrentFrame(std::shared_ptr<RenderOrder> order) {
    // Add the order to current frame's renderOrders vector
    auto& currentFrame = m_renderer.frameManager().getCurrentFrame();
    currentFrame.renderOrders.push_back(order);

    SPDLOG_TRACE("Added {} order to frame {}",
        renderOrderTypeToString(order->getType()),
        m_renderer.frameManager().getCurrentFrameIndex());
}

void RenderSystem::submitRenderOrder(std::shared_ptr<RenderOrder> order) {
    // Add to pending orders for processing
    m_pendingOrders.push_back(order);

    // Also add to current frame data
    addOrderToCurrentFrame(order);
}

void RenderSystem::submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders) {
    // Add all orders to pending list
    m_pendingOrders.insert(m_pendingOrders.end(), orders.begin(), orders.end());

    // Also add all orders to current frame data
    for (const auto& order : orders) {
        addOrderToCurrentFrame(order);
    }
}

void RenderSystem::processOrders() {
    if (m_pendingOrders.empty()) {
        SPDLOG_DEBUG("No pending orders to process");
        return;
    }

    try {
        // Reset processing context for this frame
        m_processingContext->reset();

        // Reset all semaphores in all pipelines
        m_meshPipeline->resetAllSemaphores();
        m_lightPipeline->resetAllSemaphores();
        m_cameraPipeline->resetAllSemaphores();

        // Categorize orders by type
        std::vector<std::shared_ptr<RenderOrder>> meshOrders;
        std::vector<std::shared_ptr<RenderOrder>> lightOrders;
        std::vector<std::shared_ptr<RenderOrder>> cameraOrders;
        std::vector<std::shared_ptr<RenderOrder>> otherOrders;

        categorizeOrders(meshOrders, lightOrders, cameraOrders, otherOrders);

        // Log order counts
        SPDLOG_DEBUG("Processing frame {}: {} mesh, {} light, {} camera, {} other orders",
            m_renderer.frameManager().getCurrentFrameIndex(),
            meshOrders.size(), lightOrders.size(), cameraOrders.size(), otherOrders.size());

        // If no lights, signal lights_stored semaphore so camera pipeline can proceed
        if (lightOrders.empty()) {
            SPDLOG_DEBUG("No lights to process, signaling lights_stored");
            m_lightPipeline->signalAllSemaphores();
        }

        // If no meshes, signal meshes_stored semaphore so camera pipeline can proceed
        if (meshOrders.empty()) {
            SPDLOG_DEBUG("No meshes to process, signaling meshes_stored");
            m_meshPipeline->signalAllSemaphores();
        }

        // Process all pipelines in a loop until all orders are complete
        // This allows cross-pipeline synchronization to work properly

        SPDLOG_DEBUG("Starting coordinated pipeline processing");

        size_t globalIteration = 0;
        bool allPipelinesComplete = false;

        while (!allPipelinesComplete && globalIteration < 100) { // Safety limit
            ++globalIteration;
            SPDLOG_DEBUG("Global processing iteration: {}", globalIteration);

            bool anyPipelineProgressed = false;

            // Process lights first (they need to be available for camera processing)
            if (!lightOrders.empty()) {
                SPDLOG_DEBUG("Processing {} light orders in iteration {}", lightOrders.size(), globalIteration);
                size_t lightsBefore = m_processingContext->getLightCount();
                m_lightPipeline->executeBatch(lightOrders);
                size_t lightsAfter = m_processingContext->getLightCount();

                if (lightsAfter > lightsBefore) {
                    anyPipelineProgressed = true;
                    SPDLOG_DEBUG("Light pipeline made progress: {} lights processed", lightsAfter - lightsBefore);
                }

                // Clear processed orders to avoid reprocessing them
                lightOrders.clear();
            }

            // Process meshes (can run in parallel with light processing)
            if (!meshOrders.empty()) {
                SPDLOG_DEBUG("Processing {} mesh orders in iteration {}", meshOrders.size(), globalIteration);
                size_t meshesBefore = m_processingContext->getProcessedMeshCount();
                m_meshPipeline->executeBatch(meshOrders);
                size_t meshesAfter = m_processingContext->getProcessedMeshCount();

                if (meshesAfter > meshesBefore) {
                    anyPipelineProgressed = true;
                    SPDLOG_DEBUG("Mesh pipeline made progress: {} meshes processed", meshesAfter - meshesBefore);
                }

                // Clear processed orders to avoid reprocessing them
                meshOrders.clear();
            }

            // Process cameras (will wait for lights and meshes through synchronization stages)
            if (!cameraOrders.empty()) {
                SPDLOG_DEBUG("Processing {} camera orders in iteration {}", cameraOrders.size(), globalIteration);
                size_t camerasBefore = m_processingContext->getProcessedCameraCount();
                m_cameraPipeline->executeBatch(cameraOrders);
                size_t camerasAfter = m_processingContext->getProcessedCameraCount();

                if (camerasAfter > camerasBefore) {
                    anyPipelineProgressed = true;
                    SPDLOG_DEBUG("Camera pipeline made progress: {} cameras processed", camerasAfter - camerasBefore);
                }

                // Clear processed orders to avoid reprocessing them
                cameraOrders.clear();
            }
            else if (globalIteration == 1) {
                SPDLOG_WARN("No camera orders to process - no rendering will occur");
            }

            // Check if all processing is complete
            bool lightsComplete = lightOrders.empty();
            bool meshesComplete = meshOrders.empty();
            bool camerasComplete = cameraOrders.empty();

            allPipelinesComplete = lightsComplete && meshesComplete && camerasComplete;

            SPDLOG_DEBUG("Global iteration {} status: lights_complete={}, meshes_complete={}, cameras_complete={}, any_progress={}",
                globalIteration, lightsComplete, meshesComplete, camerasComplete, anyPipelineProgressed);

            if (allPipelinesComplete) {
                SPDLOG_DEBUG("All pipelines completed processing after {} iterations", globalIteration);
                break;
            }

            // Safety check for infinite loops
            if (!anyPipelineProgressed) {
                SPDLOG_DEBUG("No progress made in iteration {}, continuing to next iteration...", globalIteration);
            }
        }

        if (globalIteration >= 100) {
            SPDLOG_ERROR("Processing loop reached safety limit of 100 iterations - possible deadlock");
        }

        // Log unhandled orders
        if (!otherOrders.empty()) {
            SPDLOG_WARN("Received {} orders of unhandled types", otherOrders.size());
            for (const auto& order : otherOrders) {
                SPDLOG_WARN("Unhandled order type: {}", renderOrderTypeToString(order->getType()));
            }
        }

        // Clear pending orders (but NOT FrameData orders - they persist for frames in flight)
        m_pendingOrders.clear();

        SPDLOG_DEBUG("Frame processing completed successfully after {} global iterations", globalIteration);

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error processing render orders: {}", e.what());
        m_pendingOrders.clear();

        // Emergency reset all pipelines on error
        if (m_meshPipeline) m_meshPipeline->emergencyReset();
        if (m_lightPipeline) m_lightPipeline->emergencyReset();
        if (m_cameraPipeline) m_cameraPipeline->emergencyReset();

        throw;
    }
}

void RenderSystem::categorizeOrders(
    std::vector<std::shared_ptr<RenderOrder>>& meshOrders,
    std::vector<std::shared_ptr<RenderOrder>>& lightOrders,
    std::vector<std::shared_ptr<RenderOrder>>& cameraOrders,
    std::vector<std::shared_ptr<RenderOrder>>& otherOrders) {

    for (const auto& order : m_pendingOrders) {
        switch (order->getType()) {
        case RenderOrderType::Mesh:
            meshOrders.push_back(order);
            break;
        case RenderOrderType::Light:
            lightOrders.push_back(order);
            break;
        case RenderOrderType::Camera:
            cameraOrders.push_back(order);
            break;
        default:
            otherOrders.push_back(order);
            break;
        }
    }
}

void RenderSystem::prepareForNextFrame() {
    // Reset any per-frame state
    reset();

    // Additional frame preparation logic can be added here
}

void RenderSystem::reset() {
    // DON'T clear FrameManager's renderOrders - they persist for frames in flight
    // FrameManager will clear them when the frame is recycled

    // Only clear RenderSystem's local pending orders
    m_pendingOrders.clear();

    // Reset processing context
    if (m_processingContext) {
        m_processingContext->reset();
    }

    // Reset all semaphores in all pipelines for next frame
    if (m_meshPipeline) {
        m_meshPipeline->resetAllSemaphores();
    }
    if (m_lightPipeline) {
        m_lightPipeline->resetAllSemaphores();
    }
    if (m_cameraPipeline) {
        m_cameraPipeline->resetAllSemaphores();
    }

    SPDLOG_DEBUG("RenderSystem reset for next frame (FrameData orders preserved for frames in flight)");
}
