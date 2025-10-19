#include "PipelineAssignmentStage.h"
#include "Material.h"
#include "Mesh.h"
#include "Engine.h"
#include "MeshRenderOrder.h"
#include "CameraRenderOrder.h"

PipelineAssignmentStage::PipelineAssignmentStage(
    ProcessingContext& context,
    EngineCore& renderer,
    AssetSystem& assetSystem,
    Settings& settings
)
    : ProcessingStage(context),
    m_settings(settings),
    m_shaderManager(assetSystem.shaderManager()),
    m_materialManager(assetSystem.materialManager()),
    m_meshManager(assetSystem.meshManager()),
    m_renderPassManager(renderer.renderPassManager())
{
    SPDLOG_INFO("Initializing PipelineAssignmentStage");
}

PipelineAssignmentStage::~PipelineAssignmentStage() {
    SPDLOG_INFO("Destroying PipelineAssignmentStage");
}

ProcessingResult PipelineAssignmentStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return ProcessingResult::Failure;
    }

    // Handle different render order types
    switch (order->getType()) {
    case RenderOrderType::Mesh:
        return processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
    default:
        // Other types (Light, EditorUI) pass through unchanged
        SPDLOG_DEBUG("PipelineAssignmentStage: unexpected render order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure;
    }
}

ProcessingResult PipelineAssignmentStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> meshOrder) {
    if (!meshOrder) {
        SPDLOG_ERROR("Null mesh order in processMeshOrder");
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Processing mesh order for entity {}", meshOrder->entity.id);

    // Validate mesh
    const Mesh* mesh = m_meshManager.getMesh(meshOrder->meshHandle);
    if (!mesh) {
        SPDLOG_WARN("Invalid mesh handle for entity {}", meshOrder->entity.id);
        return ProcessingResult::Failure;
    }

    // Get or create a pipeline configuration for this material and mesh
    GraphicsPipelineConfig pipelineConfig = createPipelineConfig(
        meshOrder->materialHandle,
        mesh
    );

    // Assign the pipeline configuration to the draw call
    meshOrder->drawCall->setPipelineConfig(pipelineConfig);

    SPDLOG_TRACE("Created pipeline config for mesh entity {}", meshOrder->entity.id);

    return ProcessingResult::Success;
}

GraphicsPipelineConfig PipelineAssignmentStage::createPipelineConfig(
    MaterialHandle materialHandle,
    const Mesh* mesh
) {
    if (!mesh) {
        SPDLOG_ERROR("Cannot create pipeline config: null mesh");
        return GraphicsPipelineConfig{};
    }

    Material* material = m_materialManager.getMaterial(materialHandle);
    if (!material) {
        SPDLOG_ERROR("Cannot create pipeline config: invalid material handle");
        return GraphicsPipelineConfig{};
    }

    SPDLOG_DEBUG("Creating pipeline for material '{}', mesh attributes {}",
        material->name(), mesh->attributes);

    // Create a pipeline configuration
    GraphicsPipelineConfig config;

    // Get the shader from the material
    ShaderHandle shaderHandle = material->shader();

    // Get shader resources (zawiera teraz wszystko czego potrzebujemy!)
    const ShaderResources& shaderResources = m_shaderManager.getShaderResources(shaderHandle);

    // Set shader stages, pipeline layout - wszystko z jednego miejsca
    config.shaderStages = shaderResources.stageConfig;
    config.layoutHandle = shaderResources.pipelineLayout;
    SPDLOG_DEBUG("Using shader resources (stages + pipeline layout)");

    // Use pre-computed vertex input configuration from mesh
    config.vertexInput = mesh->vertexInputConfig;
    SPDLOG_DEBUG("Using pre-computed vertex input config with {} attributes",
        mesh->vertexInputConfig.vertexAttributes.size());

    // Configure viewport
    config.viewport.dynamicViewport = true;
    config.viewport.dynamicScissor = true;

    // Configure rasterization
    config.rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    config.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.rasterization.lineWidth = 1.0f;

    // Configure depth/stencil
    config.depthStencil.depthTestEnable = VK_TRUE;
    config.depthStencil.depthWriteEnable = VK_TRUE;
    config.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    config.depthStencil.stencilTestEnable = VK_FALSE;

    // Get MSAA samples from settings
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(m_settings.getMsaaSamples());
    config.multisample.samples = samples;

    // Configure color blending
    config.colorBlend.blendEnable = VK_FALSE;
    config.colorBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.colorBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.colorBlend.colorBlendOp = VK_BLEND_OP_ADD;
    config.colorBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    config.colorBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.colorBlend.alphaBlendOp = VK_BLEND_OP_ADD;
    config.colorBlend.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Render pass will be assigned later in the render graph
    config.renderPass.renderPass = 0;
    config.renderPass.subpass = 0;

    SPDLOG_INFO("Pipeline configuration created successfully");
    return config;
}