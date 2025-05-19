#include "PipelineAssignmentStage.h"
#include "Material.h"
#include "Mesh.h"
#include "Engine.h"

PipelineAssignmentStage::PipelineAssignmentStage(Renderer& renderer, AssetSystem& assetSystem)
    :
    m_pipelineManager(renderer.pipelineManager()),
    m_shaderManager(assetSystem.shaderManager()),
    m_materialManager(assetSystem.materialManager()),
    m_meshManager(assetSystem.meshManager()),
    m_renderPassManager(renderer.renderPassManager()),
    m_defaultRenderPassHandle(renderer.renderPass())
{
    SPDLOG_INFO("Initializing PipelineAssignmentStage");
}

PipelineAssignmentStage::~PipelineAssignmentStage() {
    SPDLOG_INFO("Destroying PipelineAssignmentStage, clearing pipeline cache");
    clearCache();
}

void PipelineAssignmentStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    // Check if this is a mesh render order
    if (order->getType() != RenderOrderType::Mesh) {
        // Forward to next stage and return
        forwardToNextStage(order);
        return;
    }

    // Cast to mesh render order
    auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

    // Skip if mesh is invalid
    const Mesh* mesh = m_meshManager.getMesh(meshOrder->meshHandle);
    if (!mesh) {
        SPDLOG_WARN("Invalid mesh handle for entity {}", meshOrder->entity.id);
        forwardToNextStage(order);
        return;
    }

    // TODO: Determine the appropriate RenderPassHandle based on render settings
    // For now, use the default render pass handle
    RenderPassHandle renderPassHandle = m_defaultRenderPassHandle;

    // Get or create a pipeline for this material and mesh combination
    PipelineHandle pipelineHandle = getPipelineForMaterialAndMesh(
        meshOrder->materialHandle,
        meshOrder->meshHandle,
        renderPassHandle
    );

    if (pipelineHandle.id == 0) {
        SPDLOG_ERROR("Failed to get valid pipeline handle for entity {}", meshOrder->entity.id);
    }

    // Add the pipeline handle to the mesh render order
    meshOrder->pipelineHandle = pipelineHandle;

    // Forward to next stage
    forwardToNextStage(order);
}

PipelineHandle PipelineAssignmentStage::getPipelineForMaterialAndMesh(
    MaterialHandle materialHandle,
    const MeshHandle& meshHandle,
    RenderPassHandle renderPassHandle
) {
    Material* material = m_materialManager.getMaterial(materialHandle);
    if (!material) {
        SPDLOG_ERROR("Cannot get pipeline: invalid material handle");
        return PipelineHandle{};
    }

    const Mesh* mesh = m_meshManager.getMesh(meshHandle);
    if (!mesh) {
        SPDLOG_ERROR("Cannot get pipeline: invalid mesh handle");
        return PipelineHandle{};
    }

    // Create a key for the cache
    MaterialMeshPipelineKey key{ materialHandle, mesh->attributes };

    // Check if we already have a pipeline for this combination
    auto it = m_pipelineCache.find(key);
    if (it != m_pipelineCache.end() && it->second.renderPassHandle == renderPassHandle) {
        // Return cached pipeline
        return it->second.pipelineHandle;
    }

    SPDLOG_INFO("Creating new pipeline for material '{}' and mesh attributes {}",
        material->name(), mesh->attributes);

    // Create a new pipeline configuration
    GraphicsPipelineConfig config = createPipelineConfig(materialHandle, *mesh, renderPassHandle);

    // Create the pipeline
    PipelineHandle pipelineHandle = m_pipelineManager.createGraphicsPipeline(config);

    if (!pipelineHandle.id != 0) {
        SPDLOG_ERROR("Failed to create graphics pipeline for material '{}' and mesh attributes {}",
            material->name(), mesh->attributes);
        return PipelineHandle{};
    }

    // Cache the result
    m_pipelineCache[key] = { pipelineHandle, renderPassHandle };

    return pipelineHandle;
}

GraphicsPipelineConfig PipelineAssignmentStage::createPipelineConfig(
    MaterialHandle materialHandle,
    const Mesh& mesh,
    RenderPassHandle renderPassHandle
) {
    GraphicsPipelineConfig config;
    SPDLOG_INFO("Creating pipeline configuration for material {} and mesh {}",
        materialHandle.id, mesh.attributes);

    // Get the material
    Material* material = m_materialManager.getMaterial(materialHandle);
    if (!material) {
        SPDLOG_WARN("Invalid material handle in createPipelineConfig, returning default config");
        return config;
    }

    // Get the shader from the material
    ShaderHandle shaderHandle = material->shader();

    // Configure shader stages
    const CombinedShader& combinedShader = m_shaderManager.getCombinedShader(shaderHandle);

    // Set shader stages
    for (const auto& [stage, moduleHandle] : combinedShader.stages) {
        switch (stage) {
        case ShaderLib::Stage::Vertex:
            config.shaderStages.vertexShader = moduleHandle;
            SPDLOG_DEBUG("Added vertex shader module {}", moduleHandle.id);
            break;
        case ShaderLib::Stage::Fragment:
            config.shaderStages.fragmentShader = moduleHandle;
            SPDLOG_DEBUG("Added fragment shader module {}", moduleHandle.id);
            break;
        case ShaderLib::Stage::Geometry:
            config.shaderStages.geometryShader = moduleHandle;
            SPDLOG_DEBUG("Added geometry shader module {}", moduleHandle.id);
            break;
        case ShaderLib::Stage::TessellationControl:
            config.shaderStages.tessControlShader = moduleHandle;
            SPDLOG_DEBUG("Added tessellation control shader module {}", moduleHandle.id);
            break;
        case ShaderLib::Stage::TessellationEvaluation:
            config.shaderStages.tessEvalShader = moduleHandle;
            SPDLOG_DEBUG("Added tessellation evaluation shader module {}", moduleHandle.id);
            break;
        case ShaderLib::Stage::Compute:
            config.shaderStages.computeShader = moduleHandle;
            SPDLOG_DEBUG("Added compute shader module {}", moduleHandle.id);
            break;
        default:
            SPDLOG_WARN("Unknown shader stage encountered");
            break;
        }
    }

    // Get shader resources
    const ShaderResources& shaderResources = m_shaderManager.getShaderResources(shaderHandle);

    // Set pipeline layout
    config.layoutHandle = shaderResources.pipelineLayout;
    SPDLOG_DEBUG("Using pipeline layout from shader resources");

    // Configure vertex input based on mesh format
    config.vertexInput = createVertexInputConfig(mesh);

    // Configure viewport - similar to debug code, set both dynamic and default values
    config.viewport.dynamicViewport = true;
    config.viewport.dynamicScissor = true;

    // Configure rasterization - match the working debug code more closely
    config.rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    config.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.rasterization.lineWidth = 1.0f;

    // Configure depth/stencil - be explicit like debug code
    config.depthStencil.depthTestEnable = VK_TRUE;
    config.depthStencil.depthWriteEnable = VK_TRUE;
    config.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    config.depthStencil.stencilTestEnable = VK_FALSE;

    Settings& settings = Engine::get().settings();
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(settings.getMsaaSamples());

    // Configure multisampling
    config.multisample.samples = samples;

    // Configure color blending - match debug code's approach
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

    // Set render pass - be explicit and validate
    VkRenderPass renderPass = m_renderPassManager.get(renderPassHandle);
    if (renderPass == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Invalid render pass handle in pipeline config");
    }

    config.renderPass.renderPass = renderPass;
    config.renderPass.subpass = 0;

    SPDLOG_INFO("Pipeline configuration created successfully");
    return config;
}

VertexInputConfig PipelineAssignmentStage::createVertexInputConfig(const Mesh& mesh) {
    VertexInputConfig config;
    SPDLOG_DEBUG("Creating vertex input config for mesh with attributes {}", mesh.attributes);

    // Binding description - jedno wiązanie dla wszystkich atrybutów
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = mesh.vertexStride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    config.vertexBindings.push_back(bindingDesc);

    // Lokalizacje atrybutów zgodne z oczekiwaniami shadera:
    // location = 0: Position (vec3) - zawsze obecna
    // location = 1: Normal (vec3) - opcjonalnie, ale zawsze generujemy
    // location = 2: TexCoord (vec2) - opcjonalnie, ale zawsze generujemy
    // location = 3: Color (vec4) - opcjonalnie, ale zawsze generujemy
    // location = 4: Tangent (vec4) - opcjonalnie

    uint32_t currentOffset = 0;

    // Position attribute (zawsze obecna) - location 0
    VkVertexInputAttributeDescription posAttr = {};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
    posAttr.offset = currentOffset;
    config.vertexAttributes.push_back(posAttr);
    currentOffset += sizeof(float) * 3;
    SPDLOG_DEBUG("Added position attribute at location 0, offset {}", posAttr.offset);

    // Normal attribute - location 1
    if (mesh.hasNormal()) {
        VkVertexInputAttributeDescription normalAttr = {};
        normalAttr.binding = 0;
        normalAttr.location = 1;
        normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        normalAttr.offset = currentOffset;
        config.vertexAttributes.push_back(normalAttr);
        currentOffset += sizeof(float) * 3;
        SPDLOG_DEBUG("Added normal attribute at location 1, offset {}", normalAttr.offset);
    }

    // TexCoord attribute - location 2
    if (mesh.hasTexCoord()) {
        VkVertexInputAttributeDescription uvAttr = {};
        uvAttr.binding = 0;
        uvAttr.location = 2;
        uvAttr.format = VK_FORMAT_R32G32_SFLOAT; // vec2
        uvAttr.offset = currentOffset;
        config.vertexAttributes.push_back(uvAttr);
        currentOffset += sizeof(float) * 2;
        SPDLOG_DEBUG("Added texcoord attribute at location 2, offset {}", uvAttr.offset);
    }

    // Color attribute - location 3
    if (mesh.hasColor()) {
        VkVertexInputAttributeDescription colorAttr = {};
        colorAttr.binding = 0;
        colorAttr.location = 3;
        colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
        colorAttr.offset = currentOffset;
        config.vertexAttributes.push_back(colorAttr);
        currentOffset += sizeof(float) * 4;
        SPDLOG_DEBUG("Added color attribute at location 3, offset {}", colorAttr.offset);
    }

    // Tangent attribute - location 4
    if (mesh.hasTangent()) {
        VkVertexInputAttributeDescription tangentAttr = {};
        tangentAttr.binding = 0;
        tangentAttr.location = 4;
        tangentAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4 z handedness
        tangentAttr.offset = currentOffset;
        config.vertexAttributes.push_back(tangentAttr);
        currentOffset += sizeof(float) * 4;
        SPDLOG_DEBUG("Added tangent attribute at location 4, offset {}", tangentAttr.offset);
    }

    SPDLOG_DEBUG("Created vertex input config with {} attributes", config.vertexAttributes.size());

    // Weryfikacja, czy obliczony stride wierzchołka zgadza się z zadeklarowanym
    if (currentOffset != mesh.vertexStride) {
        SPDLOG_WARN("Computed vertex stride ({}) doesn't match mesh vertex stride ({})",
            currentOffset, mesh.vertexStride);
    }

    return config;
}

void PipelineAssignmentStage::clearCache() {
    SPDLOG_INFO("Clearing pipeline cache with {} entries", m_pipelineCache.size());

    // Destroy all pipelines in the cache
    for (const auto& entry : m_pipelineCache) {
        m_pipelineManager.destroy(entry.second.pipelineHandle);
    }

    // Clear the cache
    m_pipelineCache.clear();
    SPDLOG_INFO("Pipeline cache cleared successfully");
}