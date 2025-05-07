#include "RenderStage.h"
#include <array>
#include <stdexcept>

#include "Engine.h"

RenderStage::RenderStage(Renderer& renderer)
    : m_vulkanContext(renderer.vulkanContext()),
    m_frameManager(renderer.frameManager()),
    m_vramManager(renderer.vramManager()),
    m_swapChain(renderer.swapChain()),
    m_attachmentManager(renderer.attachmentManager()),
    m_framebufferManager(renderer.framebufferManager()),
    m_renderPassManager(renderer.renderPassManager()),
    m_pipelineManager(renderer.pipelineManager()),
    m_meshManager(renderer.meshManager()),
    m_depthAttachmentHandle(renderer.depthAttachmentHandle()),
    m_mainRenderPassHandle(renderer.renderPass()) {
    SPDLOG_INFO("RenderStage initialized");
}

void RenderStage::process(std::shared_ptr<RenderOrder> order) {
    // Add the render order to the current frame's collection
    m_frameManager.getCurrentFrame().renderOrders.push_back(order);
    SPDLOG_DEBUG("RenderOrder added: type={}", static_cast<int>(order->getType()));
}

void RenderStage::executeRenderPass() {
    // Get the current frame data
    auto& currentFrame = m_frameManager.getCurrentFrame();

    SPDLOG_INFO("Beginning render pass execution (frame {})", m_frameManager.getCurrentFrameIndex());
    SPDLOG_INFO("RenderOrders count: {}", currentFrame.renderOrders.size());

    if (currentFrame.renderOrders.empty()) {
        SPDLOG_WARN("No render orders to process in current frame!");
    }

    // Wait for previous frame to finish - IMPORTANT: Do this FIRST
    auto inFlightFence = m_frameManager.getInFlightFence();
    SPDLOG_DEBUG("Waiting for in-flight fence: {:#x}", reinterpret_cast<uint64_t>(inFlightFence));

    vkWaitForFences(
        m_vulkanContext.logical().get(),
        1,
        &inFlightFence,
        VK_TRUE,
        UINT64_MAX
    );

    // Now that we've waited for the fence, we can safely reset it
    vkResetFences(m_vulkanContext.logical().get(), 1, &inFlightFence);
    SPDLOG_DEBUG("Fence reset");

    // Reset command buffers after fence wait
    if (currentFrame.graphicsCommandBuffer) {
        // Check if the command buffer is in recording state and end it first
        if (currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        currentFrame.graphicsCommandBuffer->reset();
        currentFrame.graphicsCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        SPDLOG_DEBUG("Graphics command buffer reset and began");
    }
    else {
        SPDLOG_ERROR("Graphics command buffer is null!");
    }

    if (currentFrame.transferCommandBuffer) {
        // Check if the command buffer is in recording state and end it first
        if (currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }
        currentFrame.transferCommandBuffer->reset();
        currentFrame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        SPDLOG_DEBUG("Transfer command buffer reset and began");
    }
    else {
        SPDLOG_ERROR("Transfer command buffer is null!");
    }

    // Execute pending transfers from TransferManager if any
    if (m_vramManager.transferManager().hasPendingTransfers()) {
        SPDLOG_INFO("Executing pending VRAM transfers");
        // Execute transfers using both command buffers (transfer for the actual transfers,
        // graphics for the post-transfer barriers)
        m_vramManager.transferManager().executeTransfers(
            *currentFrame.transferCommandBuffer,
            *currentFrame.graphicsCommandBuffer
        );
        currentFrame.hasTransferCommands = true;
    }

    // Execute transfer commands if any were recorded
    if (currentFrame.hasTransferCommands) {
        SPDLOG_DEBUG("Submitting transfer commands");
        // End the transfer command buffer only if it's still recording
        if (currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }

        // Submit transfer commands
        currentFrame.transferCommandBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer),
            {}, // No wait semaphores
            {}, // No wait stages
            { currentFrame.transferFinished }, // Signal transfer finished semaphore
            VK_NULL_HANDLE // No fence needed, we'll wait for render fence later
        );
    }

    // Reclaim staging buffers that are no longer in use
    m_vramManager.reclaimStagingBuffers();

    // Acquire the next image from the swap chain
    uint32_t imageIndex;
    SPDLOG_DEBUG("Acquiring next swapchain image");
    VkResult result = m_swapChain.acquireNextImage(
        m_frameManager.getImageAvailableSemaphore(),
        &imageIndex
    );
    SPDLOG_INFO("Acquired swapchain image: index={}, result={}", imageIndex, static_cast<int>(result));

    // Handle swapchain recreation
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal, recreating");
        // Make sure we end command buffers before returning
        if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        m_swapChain.recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS) {
        // End command buffers before throwing
        SPDLOG_ERROR("Failed to acquire swap chain image! Result: {}", static_cast<int>(result));
        if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    VramHandle swapchainImageHandle = m_swapChain.getImageHandles()[imageIndex];
    SPDLOG_DEBUG("Using swapchain image handle: {}", swapchainImageHandle.id);

    // Get or create framebuffer for this swapchain image
    AttachmentHandle swapchainAttachmentHandle = m_attachmentManager.registerExternalImage(
        swapchainImageHandle,
        m_swapChain.getImageFormat(),
        m_swapChain.getSwapChainExtent(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        AttachmentType::Color,
        "SwapchainImage_" + std::to_string(imageIndex)
    );
    SPDLOG_DEBUG("Created swapchain attachment handle: {}", swapchainAttachmentHandle.id);

    std::vector<AttachmentHandle> framebufferAttachments = {
        swapchainAttachmentHandle,
        m_depthAttachmentHandle
    };
    SPDLOG_DEBUG("Using depth attachment handle: {}", m_depthAttachmentHandle.id);

    FrameBufferHandle framebufferHandle = m_framebufferManager.getOrCreate(
        m_mainRenderPassHandle,
        framebufferAttachments,
        m_swapChain.getSwapChainExtent()
    );
    SPDLOG_DEBUG("Using framebuffer handle: {}", framebufferHandle.id);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPassManager.get(m_mainRenderPassHandle);
    renderPassInfo.framebuffer = m_framebufferManager.get(framebufferHandle);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapChain.getSwapChainExtent();

    // Clear values for attachments
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.2f, 1.0f} }; // Dark blue background
    clearValues[1].depthStencil = { 1.0f, 0 }; // Depth clear value

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Begin render pass
    SPDLOG_INFO("Beginning render pass with dimensions: {}x{}",
        m_swapChain.getSwapChainExtent().width,
        m_swapChain.getSwapChainExtent().height);

    vkCmdBeginRenderPass(
        currentFrame.graphicsCommandBuffer->get(),
        &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );

    // Execute rendering commands for all collected render orders
    executeRenderCommands(
        currentFrame.graphicsCommandBuffer->get(),
        imageIndex,
        currentFrame.renderOrders
    );

    // End render pass
    vkCmdEndRenderPass(currentFrame.graphicsCommandBuffer->get());
    SPDLOG_DEBUG("Render pass ended");

    // Make sure we end the graphics command buffer only if it's still recording
    if (currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }

    // Prepare wait semaphores and stages
    std::vector<VkSemaphore> waitSemaphores = {
        m_frameManager.getImageAvailableSemaphore()  // Always wait for image to be available
    };
    std::vector<VkPipelineStageFlags> waitStages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    // Only wait for transfer if transfers were actually submitted
    if (currentFrame.hasTransferCommands) {
        waitSemaphores.push_back(m_frameManager.gettransferFinishedSemaphore());
        waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
        SPDLOG_DEBUG("Added transfer semaphore to wait list");
    }

    std::vector<VkSemaphore> signalSemaphores = { m_frameManager.getRenderFinishedSemaphore() };

    // Add try-catch block to handle errors during submission
    try {
        SPDLOG_DEBUG("Submitting graphics command buffer");
        currentFrame.graphicsCommandBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
            waitSemaphores,
            waitStages,
            signalSemaphores,
            m_frameManager.getInFlightFence()
        );
    }
    catch (const std::exception& e) {
        // Log the error but continue
        SPDLOG_ERROR("Error submitting graphics command buffer: {}", e.what());

        // Clear render orders for next frame
        currentFrame.renderOrders.clear();

        // Advance to the next frame
        m_frameManager.advanceFrame();
        return;
    }

    // Present the image to the swapchain
    SPDLOG_DEBUG("Presenting image index: {}", imageIndex);
    result = m_swapChain.presentImage(imageIndex, m_frameManager.getRenderFinishedSemaphore());
    SPDLOG_INFO("Present result: {}", static_cast<int>(result));

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal after present, recreating");
        m_swapChain.recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        SPDLOG_ERROR("Failed to present swap chain image! Result: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to present swap chain image!");
    }

    // Clear render orders for next frame
    currentFrame.renderOrders.clear();

    // Advance to the next frame
    m_frameManager.advanceFrame();
    SPDLOG_DEBUG("Advanced to next frame");
}

void RenderStage::executeRenderCommands(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    const std::vector<std::shared_ptr<RenderOrder>>& renderOrders
) {
    SPDLOG_INFO("Executing render commands for {} orders", renderOrders.size());

    // Process mesh orders to render geometry
    for (const auto& order : renderOrders) {
        if (order->getType() == RenderOrderType::Mesh) {
            auto meshOrder = static_cast<MeshRenderOrder*>(order.get());
            SPDLOG_DEBUG("Processing mesh render order, pipeline handle: {}, mesh handle: {}",
                meshOrder->pipelineHandle.id, meshOrder->meshHandle.id);

            Pipeline* pipeline = &m_pipelineManager.get(meshOrder->pipelineHandle);
            if (!pipeline) {
                SPDLOG_ERROR("Failed to get pipeline from handle: {}", meshOrder->pipelineHandle.id);
                continue;
            }

            // Bind the pipeline
            vkCmdBindPipeline(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->get()
            );
            SPDLOG_DEBUG("Pipeline bound");

            // Set dynamic viewport and scissor
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_swapChain.getSwapChainExtent().width);
            viewport.height = static_cast<float>(m_swapChain.getSwapChainExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = m_swapChain.getSwapChainExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            SPDLOG_DEBUG("Viewport and scissor set: {}x{}", viewport.width, viewport.height);

            // Bind descriptor sets
            if (meshOrder->globalDescriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    0, // First set
                    1, // Set count
                    &meshOrder->globalDescriptorSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Global descriptor set bound: {:#x}",
                    reinterpret_cast<uint64_t>(meshOrder->globalDescriptorSet));
            }
            else {
                SPDLOG_WARN("Global descriptor set is NULL");
            }

            if (meshOrder->objectDescriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    1, // Second set
                    1, // Set count
                    &meshOrder->objectDescriptorSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Object descriptor set bound: {:#x}",
                    reinterpret_cast<uint64_t>(meshOrder->objectDescriptorSet));
            }
            else {
                SPDLOG_WARN("Object descriptor set is NULL");
            }

            if (meshOrder->materialDescriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    2, // Third set
                    1, // Set count
                    &meshOrder->materialDescriptorSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Material descriptor set bound: {:#x}",
                    reinterpret_cast<uint64_t>(meshOrder->materialDescriptorSet));
            }
            else {
                SPDLOG_WARN("Material descriptor set is NULL");
            }

            // Bind vertex and index buffers
            const Mesh* mesh = m_meshManager.getMesh(meshOrder->meshHandle);
            if (mesh) {
                SPDLOG_DEBUG("Using mesh: handle={}, vertices={}, indices={}",
                    meshOrder->meshHandle.id, mesh->vertexCount, mesh->indexCount);

                VkBuffer vertexBuffer = m_vramManager.getResource<Buffer>(mesh->vertexBuffer)->get();
                VkBuffer indexBuffer = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->get();

                if (vertexBuffer == VK_NULL_HANDLE) {
                    SPDLOG_ERROR("Vertex buffer handle is NULL!");
                }

                if (indexBuffer == VK_NULL_HANDLE) {
                    SPDLOG_ERROR("Index buffer handle is NULL!");
                }

                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
                SPDLOG_DEBUG("Vertex buffer bound: {:#x}", reinterpret_cast<uint64_t>(vertexBuffer));

                // Proper index buffer binding and validation
                VkIndexType vkIndexType = (mesh->indexType == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, vkIndexType);
                SPDLOG_DEBUG("Index buffer bound: {:#x}, type: {}",
                    reinterpret_cast<uint64_t>(indexBuffer),
                    (vkIndexType == VK_INDEX_TYPE_UINT16 ? "UINT16" : "UINT32"));

                // Validate index buffer size
                size_t indexSize = mesh->getIndexSize();
                VkDeviceSize expectedSize = indexSize * mesh->indexCount;
                VkDeviceSize actualSize = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->getSize();
                SPDLOG_DEBUG("Index buffer: expected size={}, actual size={}, indexSize={}",
                    expectedSize, actualSize, indexSize);

                if (expectedSize > actualSize) {
                    SPDLOG_ERROR("Index buffer too small: expected {} bytes, got {} bytes",
                        expectedSize, actualSize);
                    // Calculate safe count
                    uint32_t safeCount = static_cast<uint32_t>(actualSize / indexSize);
                    vkCmdDrawIndexed(
                        commandBuffer,
                        safeCount,
                        1, // Instance count
                        0, // First index
                        0, // Vertex offset
                        0  // First instance
                    );
                    SPDLOG_WARN("Drawing with reduced index count: {}", safeCount);
                }
                else {
                    // Draw indexed
                    vkCmdDrawIndexed(
                        commandBuffer,
                        mesh->indexCount,
                        1, // Instance count
                        0, // First index
                        0, // Vertex offset
                        0  // First instance
                    );
                    SPDLOG_INFO("Drawing indexed: count={}", mesh->indexCount);
                }
            }
            else {
                SPDLOG_ERROR("Mesh not found for handle: {}", meshOrder->meshHandle.id);
            }
        }
        else {
            SPDLOG_WARN("Unsupported render order type: {}", static_cast<int>(order->getType()));
        }
    }
}


void RenderStage::createAndRenderDebugObject() {
    // ---------------------------------------------------------------
    // KONFIGURACJA - WSZYSTKIE PARAMETRY DO USTAWIENIA RĘCZNIE
    // ---------------------------------------------------------------

    // Konfiguracja zasobów
    std::string meshName = "Flora_C";          // Nazwa mesha
    std::string materialName = "Flora_c";      // Nazwa materiału

    // Konfiguracja kamery
    glm::vec3 cameraPosition = glm::vec3(0.0f, 2.0f, 15.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float fieldOfView = 45.0f;                 // Kąt widzenia w stopniach
    float aspectRatio = 1920.0f / 1080.0f;     // Proporcje szerokość/wysokość
    float nearPlane = 0.1f;                    // Near plane
    float farPlane = 100.0f;                   // Far plane

    // Konfiguracja modelu
    glm::mat4 modelMatrix = glm::mat4(1.0f);   // Macierz bazowa (identyczność)
    float rotationAngle = 30.0f;               // Kąt obrotu w stopniach
    glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f); // Oś obrotu (Y)
    glm::vec4 objectColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f); // Kolor obiektu (RGBA)

    // Konfiguracja światła kierunkowego
    glm::vec3 directionalLightDir = glm::vec3(-1.0f, -1.0f, -1.0f);
    glm::vec4 directionalLightColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f); // RGB + intensywność

    // Konfiguracja pierwszego światła punktowego
    glm::vec3 pointLightPosition = glm::vec3(2.0f, 2.0f, 2.0f);
    float pointLightRadius = 10.0f;
    glm::vec4 pointLightColor = glm::vec4(0.8f, 0.8f, 0.8f, 0.8f); // RGB + intensywność

    // Konfiguracja pipeline
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    bool depthTestEnable = true;

    // Liczba aktywnych świateł
    int activePointLights = 1;
    int activeSpotLights = 0;

    // ---------------------------------------------------------------
    // IMPLEMENTACJA - PONIŻSZY KOD UŻYWA PARAMETRÓW ZDEFINIOWANYCH POWYŻEJ
    // ---------------------------------------------------------------

    Renderer& renderer = Engine::get().renderer();
    AssetManager& assetManager = Engine::get().assetManager();
    ShaderModuleManager& shaderManager = renderer.shaderModuleManager();
    MeshManager& meshManager = renderer.meshManager();
    MaterialManager& materialManager = renderer.materialManager();

    // Utworzenie AssetHandles dla mesha i materiału
    AssetHandle meshAssetHandle(AssetType::Mesh, meshName);
    AssetHandle materialAssetHandle(AssetType::Material, materialName);

    // Ładowanie i przygotowanie mesha
    assetManager.ensureLoaded(meshAssetHandle);
    assetManager.ensureReady(meshAssetHandle);
    const MeshHandle& meshHandle = assetManager.getResource<MeshHandle>(meshAssetHandle);
    const Mesh* mesh = meshManager.getMesh(meshHandle);

    if (!mesh) {
        SPDLOG_ERROR("Failed to load debug mesh: {}", meshName);
        return;
    }

    assetManager.ensureLoaded(materialAssetHandle);
    assetManager.ensureReady(materialAssetHandle);

    const MaterialHandle& materialHandle = assetManager.getResource<Material>(materialAssetHandle);
    Material* material = materialManager.get(materialHandle);

    const ShaderHandle& shaderHandle = material->shader();

    if (!shaderManager.isShaderValid(shaderHandle)) {
        SPDLOG_ERROR("Invalid shader handle for material: {}", materialName);
        return;
    }

    // Pobieranie modułów shadera
    CombinedShader combined = shaderManager.getCombinedShader(shaderHandle);
    ShaderModuleHandle vertexShaderHandle = combined.stages.at(ShaderLib::Stage::Vertex);
    ShaderModuleHandle fragmentShaderHandle = combined.stages.at(ShaderLib::Stage::Fragment);

    SPDLOG_DEBUG("Created shader modules: vertex={}, fragment={}",
        vertexShaderHandle.id, fragmentShaderHandle.id);

    // Pobieranie zasobów shadera (layouty deskryptorów, layout pipeline'u)
    const ShaderResources& shaderResources = shaderManager.getShaderResources(shaderHandle);
    const ShaderLib::ShaderMetadata& shaderMetadata = shaderManager.getShaderMetadata(shaderHandle);

    // Tworzenie i konfiguracja uniform buforów
    // 1. Globalny uniform buffer (set=0, binding=0)
    UniformBufferHandle globalUboHandle = shaderManager.createGlobalUniformBuffer(shaderHandle);
    if (!globalUboHandle) {
        SPDLOG_ERROR("Failed to create global uniform buffer");
        return;
    }

    // 2. Obiektowy uniform buffer (set=1, binding=0)
    UniformBufferHandle objectUboHandle = shaderManager.createObjectUniformBuffer(shaderHandle);
    if (!objectUboHandle) {
        SPDLOG_ERROR("Failed to create object uniform buffer");
        return;
    }

    // 3. Custom uniform buffer (set=2, binding=0)
    UniformBufferHandle customUboHandle = shaderManager.createCustomUniformBuffer(shaderHandle, "InputData");
    if (!customUboHandle) {
        SPDLOG_ERROR("Failed to create custom uniform buffer");
        return;
    }

    // Pobieranie UniformBufferManager do aktualizacji danych w buforach
    UniformBufferManager& uniformBufferManager = renderer.uniformBufferManager();

    // Przygotowanie danych dla globalnego UBO (kamera, światła)
    struct alignas(16) {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPosition;
        // Directional Light
        alignas(16) struct {
            glm::vec3 direction;
            alignas(16) glm::vec4 color; // w is intensity
        } directionalLight;
        // Point Lights
        struct {
            glm::vec3 position;
            float radius;
            glm::vec4 color; // w is intensity
        } pointLights[64];
        // Spot Lights
        struct alignas(16) {
            glm::vec3 position;
            float innerCutoff;
            glm::vec3 direction;
            float outerCutoff;
            glm::vec4 color; // w is intensity
            float range;
        } spotLights[16];
        int activePointLights;
        int activeSpotLights;
    } globalUboData;

    // Wypełnienie danych globalnego UBO
    globalUboData.view = glm::lookAt(
        cameraPosition,  // Pozycja kamery
        cameraTarget,    // Punkt, na który patrzy kamera
        cameraUp         // Wektor "góry"
    );

    glm::mat4 vulkanCorrection = glm::mat4(1.0f);
    vulkanCorrection[1][1] = -1.0f;
    globalUboData.proj = vulkanCorrection * glm::perspective(
        glm::radians(fieldOfView),  // Kąt widzenia
        aspectRatio,                // Aspect ratio
        nearPlane,                  // Near plane
        farPlane                    // Far plane
    );
    globalUboData.cameraPosition = cameraPosition;

    // Konfiguracja światła kierunkowego
    globalUboData.directionalLight.direction = glm::normalize(directionalLightDir);
    globalUboData.directionalLight.color = directionalLightColor;

    // Konfiguracja pierwszego światła punktowego
    globalUboData.pointLights[0].position = pointLightPosition;
    globalUboData.pointLights[0].radius = pointLightRadius;
    globalUboData.pointLights[0].color = pointLightColor;

    // Ustawienie liczby aktywnych świateł
    globalUboData.activePointLights = activePointLights;
    globalUboData.activeSpotLights = activeSpotLights;

    // Aktualizacja danych w globalnym UBO
    uniformBufferManager.updateBuffer(globalUboHandle, &globalUboData, sizeof(globalUboData));

    // Przygotowanie danych dla obiektowego UBO
    struct alignas(16) {
        glm::mat4 model;
        glm::vec4 color;
    } objectUboData;

    // Wypełnienie danych obiektowego UBO
    objectUboData.model = modelMatrix;
    objectUboData.model = glm::rotate(objectUboData.model, glm::radians(rotationAngle), rotationAxis);
    objectUboData.color = objectColor;

    // Aktualizacja danych w obiektowym UBO
    uniformBufferManager.updateBuffer(objectUboHandle, &objectUboData, sizeof(objectUboData));

    // Aktualizacja danych w custom UBO na podstawie parametrów materiału
    const auto& customUboInfo = uniformBufferManager.getBufferInfo(customUboHandle);
    const auto& materialParams = material->parameters();

    // Przygotowanie bufora dla customowego UBO
    std::vector<uint8_t> customUboData(customUboInfo.size, 0);

    // Mapowanie parametrów materiału do pól w custom UBO
    for (const auto& param : materialParams) {
        // Znajdź odpowiednią zmienną w customowym UBO
        bool foundVariable = false;
        for (const auto& variable : customUboInfo.variables) {
            if (variable.name == param.name) {
                foundVariable = true;

                // Kopiowanie danych parametru do bufora na podstawie typu
                if (std::holds_alternative<Material::FloatParam>(param.value)) {
                    const auto& floatParam = std::get<Material::FloatParam>(param.value);
                    memcpy(customUboData.data() + variable.offset, &floatParam.value, sizeof(float));
                }
                else if (std::holds_alternative<Material::Vec2Param>(param.value)) {
                    const auto& vec2Param = std::get<Material::Vec2Param>(param.value);
                    float values[2] = { vec2Param.x, vec2Param.y };
                    memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 2);
                }
                else if (std::holds_alternative<Material::Vec3Param>(param.value)) {
                    const auto& vec3Param = std::get<Material::Vec3Param>(param.value);
                    float values[3] = { vec3Param.x, vec3Param.y, vec3Param.z };
                    memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 3);
                }
                else if (std::holds_alternative<Material::Vec4Param>(param.value)) {
                    const auto& vec4Param = std::get<Material::Vec4Param>(param.value);
                    float values[4] = { vec4Param.x, vec4Param.y, vec4Param.z, vec4Param.w };
                    memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 4);
                }
                else if (std::holds_alternative<Material::IntParam>(param.value)) {
                    const auto& intParam = std::get<Material::IntParam>(param.value);
                    memcpy(customUboData.data() + variable.offset, &intParam.value, sizeof(int32_t));
                }
                else if (std::holds_alternative<Material::BoolParam>(param.value)) {
                    const auto& boolParam = std::get<Material::BoolParam>(param.value);
                    int32_t value = boolParam.value ? 1 : 0;  // Booleans are often represented as ints in shaders
                    memcpy(customUboData.data() + variable.offset, &value, sizeof(int32_t));
                }
                else if (std::holds_alternative<Material::Mat4Param>(param.value)) {
                    const auto& mat4Param = std::get<Material::Mat4Param>(param.value);
                    memcpy(customUboData.data() + variable.offset, mat4Param.data, sizeof(float) * 16);
                }
                // Inne typy można dodać podobnie...

                break;
            }
        }

        if (!foundVariable) {
            SPDLOG_WARN("Material parameter '{}' not found in custom UBO", param.name);
        }
    }

    // Aktualizacja danych w customowym UBO
    uniformBufferManager.updateBuffer(customUboHandle, customUboData.data(), customUboData.size());

    // Konfiguracja pipeline'u na podstawie właściwości mesha
    GraphicsPipelineConfig pipelineConfig;

    // Dynamiczna konfiguracja vertex input na podstawie atrybutów mesha
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = mesh->vertexStride; // Używamy stride z mesha
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    uint32_t location = 0;
    size_t currentOffset = 0;

    if (mesh->hasPosition()) {
        VkVertexInputAttributeDescription posAttr{};
        posAttr.binding = 0;
        posAttr.location = location++;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = currentOffset;
        attributeDescriptions.push_back(posAttr);
        currentOffset += 3 * sizeof(float);
    }

    if (mesh->hasColor()) {
        VkVertexInputAttributeDescription colorAttr{};
        colorAttr.binding = 0;
        colorAttr.location = location++;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorAttr.offset = currentOffset;
        attributeDescriptions.push_back(colorAttr);
        currentOffset += 3 * sizeof(float);
    }

    if (mesh->hasTexCoord()) {
        VkVertexInputAttributeDescription texCoordAttr{};
        texCoordAttr.binding = 0;
        texCoordAttr.location = location++;
        texCoordAttr.format = VK_FORMAT_R32G32_SFLOAT;
        texCoordAttr.offset = currentOffset;
        attributeDescriptions.push_back(texCoordAttr);
        currentOffset += 2 * sizeof(float);
    }

    if (mesh->hasNormal()) {
        VkVertexInputAttributeDescription normalAttr{};
        normalAttr.binding = 0;
        normalAttr.location = location++;
        normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        normalAttr.offset = currentOffset;
        attributeDescriptions.push_back(normalAttr);
        currentOffset += 3 * sizeof(float);
    }

    pipelineConfig.vertexInput.vertexBindings.push_back(bindingDescription);
    pipelineConfig.vertexInput.vertexAttributes = attributeDescriptions;

    // Konfiguracja shaderów
    pipelineConfig.shaderStages.vertexShader = vertexShaderHandle;
    pipelineConfig.shaderStages.fragmentShader = fragmentShaderHandle;

    // Używamy layout'u pipeline'u z zasobów shadera
    pipelineConfig.layoutHandle = shaderResources.pipelineLayout;

    // Zastosowanie parametrów pipeline'u
    pipelineConfig.rasterization.cullMode = cullMode;
    pipelineConfig.depthStencil.depthTestEnable = depthTestEnable;
    pipelineConfig.renderPass.renderPass = m_renderPassManager.get(m_mainRenderPassHandle);
    pipelineConfig.renderPass.subpass = 0;

    PipelineManager& pipelineManager = renderer.pipelineManager();
    PipelineHandle pipelineHandle = pipelineManager.createGraphicsPipeline(pipelineConfig);

    SPDLOG_INFO("Created debug pipeline: handle={}", pipelineHandle.id);

    // Tworzenie descriptor setów
    DescriptorAllocator& descriptorAllocator = renderer.descriptorAllocator();
    ImageSamplerManager& samplerManager = renderer.imageSamplerManager();

    // Tworzenie deskryptora dla globalnego UBO (set=0)
    VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE;
    if (shaderResources.descriptorLayouts.count(0) > 0) {
        // Pobierz layout deskryptora dla set=0
        VkDescriptorSetLayout globalSetLayout =
            renderer.descriptorLayoutManager().get(shaderResources.descriptorLayouts.at(0));

        // Alokuj deskryptor set używając DescriptorAllocator
        globalDescriptorSet = descriptorAllocator.allocate(globalSetLayout);

        if (globalDescriptorSet != VK_NULL_HANDLE) {
            // Wypełnij deskryptor set danymi o buforze uniform
            DescriptorWriter writer;
            Buffer* globalBuffer = uniformBufferManager.getBuffer(globalUboHandle);
            writer.writeBuffer(0, globalBuffer->get(), globalBuffer->getSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.updateSet(renderer.vulkanContext().logical().get(), globalDescriptorSet);

            SPDLOG_DEBUG("Created global descriptor set using allocator");
        }
    }

    // Tworzenie deskryptora dla obiektowego UBO (set=1)
    VkDescriptorSet objectDescriptorSet = VK_NULL_HANDLE;
    if (shaderResources.descriptorLayouts.count(1) > 0) {
        // Pobierz layout deskryptora dla set=1
        VkDescriptorSetLayout objectSetLayout =
            renderer.descriptorLayoutManager().get(shaderResources.descriptorLayouts.at(1));

        // Alokuj deskryptor set używając DescriptorAllocator
        objectDescriptorSet = descriptorAllocator.allocate(objectSetLayout);

        if (objectDescriptorSet != VK_NULL_HANDLE) {
            // Wypełnij deskryptor set danymi o buforze uniform
            DescriptorWriter writer;
            Buffer* objectBuffer = uniformBufferManager.getBuffer(objectUboHandle);
            writer.writeBuffer(0, objectBuffer->get(), objectBuffer->getSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.updateSet(renderer.vulkanContext().logical().get(), objectDescriptorSet);

            SPDLOG_DEBUG("Created object descriptor set using allocator");
        }
    }

    VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
    if (shaderResources.descriptorLayouts.count(2) > 0) {
        // Pobierz layout deskryptora dla set=2
        VkDescriptorSetLayout materialSetLayout =
            renderer.descriptorLayoutManager().get(shaderResources.descriptorLayouts.at(2));

        // Alokuj deskryptor set używając DescriptorAllocator
        materialDescriptorSet = descriptorAllocator.allocate(materialSetLayout);

        if (materialDescriptorSet != VK_NULL_HANDLE) {
            // Przygotuj deskryptor writer
            DescriptorWriter writer;

            // Dodaj UBO materiału
            Buffer* customBuffer = uniformBufferManager.getBuffer(customUboHandle);
            if (customBuffer) {
                writer.writeBuffer(0, customBuffer->get(), customBuffer->getSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                SPDLOG_DEBUG("Added custom UBO to material descriptor set at binding 0");
            }

            // Przejdź przez wszystkie parametry tekstur materiału
            for (const auto& param : materialParams) {
                if (std::holds_alternative<Material::TextureParam>(param.value)) {
                    const auto& textureParam = std::get<Material::TextureParam>(param.value);

                    // Sprawdź czy to jest tekstura z poprawnym uchwytem
                    if (textureParam.vramHandle.isValid()) {
                        // Znajdź odpowiedni binding na podstawie metadanych shadera
                        int binding = -1;

                        // Najpierw szukamy bezpośrednio po nazwie parametru
                        for (const auto& descriptor : shaderMetadata.descriptors) {
                            if (descriptor.name == param.name &&
                                descriptor.set == ShaderLib::CUSTOM_DESCRIPTOR_SET &&
                                descriptor.type == ShaderLib::DescriptorType::CombinedImageSampler) {
                                binding = descriptor.binding;
                                SPDLOG_DEBUG("Found texture binding for '{}' by direct name match: binding={}",
                                    param.name, binding);
                                break;
                            }
                        }

                        // Jeśli nie znaleziono bezpośrednio, spróbuj dopasować po deklaracji samplera
                        if (binding < 0) {
                            // Próbuj znaleźć sampler w różnych konwencjach nazewnictwa
                            std::vector<std::string> possibleNames = {
                                param.name,
                                param.name + "Sampler",
                                param.name + "_sampler",
                                "sampler" + param.name.substr(0, 1) + param.name.substr(1)  // samplerDiffuse
                            };

                            for (const auto& name : possibleNames) {
                                for (const auto& descriptor : shaderMetadata.descriptors) {
                                    if (descriptor.name == name &&
                                        descriptor.set == ShaderLib::CUSTOM_DESCRIPTOR_SET &&
                                        descriptor.type == ShaderLib::DescriptorType::CombinedImageSampler) {
                                        binding = descriptor.binding;
                                        SPDLOG_DEBUG("Found texture binding for '{}' by alternative name '{}': binding={}",
                                            param.name, name, binding);
                                        break;
                                    }
                                }
                                if (binding >= 0) break;
                            }
                        }

                        if (binding >= 0) {
                            // Pobierz sampler z textureParam lub użyj domyślnego z samplerManager
                            VkSampler sampler = textureParam.sampler;
                            if (sampler == VK_NULL_HANDLE) {
                                // Użyj domyślnej konfiguracji samplera
                                SamplerConfig samplerConfig{}; // Domyślna konfiguracja
                                sampler = samplerManager.getSampler(samplerConfig);
                                SPDLOG_DEBUG("Using default sampler for texture '{}'", param.name);
                            }
                            else {
                                SPDLOG_DEBUG("Using provided sampler for texture '{}'", param.name);
                            }

                            Image* textureImage = m_vramManager.getResource<Image>(textureParam.vramHandle);

                            // Pobierz ImageView dla tekstury z menedżera obrazów
                            VkImageView imageView = textureImage->createView(
                                renderer.vulkanContext().logical().get(),
                                VK_IMAGE_VIEW_TYPE_2D
                                // Format will use the image's format by default
                                // AspectMask will be derived from the format automatically
                                // Mip levels will default to VK_REMAINING_MIP_LEVELS
                                // Array layers will default to VK_REMAINING_ARRAY_LAYERS
                            );

                            if (imageView != VK_NULL_HANDLE) {
                                writer.writeImage(binding, imageView, sampler,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                                SPDLOG_DEBUG("Added texture '{}' to material descriptor set at binding {}",
                                    param.name, binding);
                            }
                            else {
                                SPDLOG_ERROR("Failed to get valid ImageView for texture '{}'", param.name);
                            }
                        }
                        else {
                            SPDLOG_WARN("No suitable binding found for texture parameter '{}' in shader metadata",
                                param.name);
                        }
                    }
                    else {
                        SPDLOG_WARN("Texture parameter '{}' has invalid VRAM handle", param.name);
                    }
                }
            }

            // Zaktualizuj deskryptor set
            writer.updateSet(renderer.vulkanContext().logical().get(), materialDescriptorSet);
            SPDLOG_DEBUG("Created material descriptor set using allocator");
        }
        else {
            SPDLOG_ERROR("Failed to allocate material descriptor set");
        }
    }
    else {
        SPDLOG_WARN("No descriptor layout found for custom material set (set=2)");
    }

    // Tworzenie render order z użyciem poprawnego meshHandle
    auto renderOrder = std::make_shared<MeshRenderOrder>();
    renderOrder->meshHandle = meshHandle;
    renderOrder->pipelineHandle = pipelineHandle;
    renderOrder->globalDescriptorSet = globalDescriptorSet;
    renderOrder->objectDescriptorSet = objectDescriptorSet;
    renderOrder->materialDescriptorSet = materialDescriptorSet; // Używamy deskryptora materiału

    process(renderOrder);
    SPDLOG_INFO("Debug object render order processed");
}