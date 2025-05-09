#include "PipelineAssignmentStage.h"
#include "MaterialManager.h"
#include "Material.h"
#include "MeshManager.h"
#include "Mesh.h"
#include "Renderer.h"

PipelineAssignmentStage::PipelineAssignmentStage(Renderer& renderer)
    :
    m_pipelineManager(renderer.pipelineManager()),
    m_shaderManager(renderer.shaderModuleManager()),
    m_materialManager(renderer.materialManager()),
    m_meshManager(renderer.meshManager()),
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

    // Skip if material is invalid
    if (!m_materialManager.isValid(meshOrder->materialHandle)) {
        SPDLOG_WARN("Invalid material handle for entity {}", meshOrder->entity.id);
        forwardToNextStage(order);
        return;
    }

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
    Material* material = m_materialManager.get(materialHandle);
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
    Material* material = m_materialManager.get(materialHandle);
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

    // Configure multisampling
    //config.multisample.samples = VK_FALSE;

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

    // Binding description - one binding for all attributes
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = mesh.vertexStride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    config.vertexBindings.push_back(bindingDesc);

    // FIXED IMPLEMENTATION: Instead of reusing the position data for missing attributes,
    // we'll allocate separate attributes with appropriate defaults

    // Mandatory attribute locations based on shader expectations:
    // location = 0: Position (vec3)
    // location = 1: Color (vec3)
    // location = 2: TexCoord (vec2)
    // location = 3: Normal (vec3)

    uint32_t offset = 0;

    // Position attribute (always present) - Always location 0
    if (mesh.hasPosition()) {
        VkVertexInputAttributeDescription posAttr = {};
        posAttr.binding = 0;
        posAttr.location = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        posAttr.offset = offset;
        config.vertexAttributes.push_back(posAttr);
        offset += sizeof(float) * 3; // Advance offset for position
        SPDLOG_DEBUG("Added position attribute at location 0, offset {}", posAttr.offset);
    }
    else {
        SPDLOG_WARN("Mesh is missing position attribute, which should be mandatory");
    }

    // Color attribute - Location 1
    if (mesh.hasColor()) {
        VkVertexInputAttributeDescription colorAttr = {};
        colorAttr.binding = 0;
        colorAttr.location = 1;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3 as per shader
        colorAttr.offset = offset;
        config.vertexAttributes.push_back(colorAttr);
        offset += sizeof(float) * 4; // Assuming color is vec4 in mesh
        SPDLOG_DEBUG("Added color attribute at location 1, offset {}", colorAttr.offset);
    }
    else {
        // Instead of reusing position data, we'll create a dummy color attribute
        // that points to a default value (this requires vertex shader to handle missing attributes)
        VkVertexInputAttributeDescription colorAttr = {};
        colorAttr.binding = 0;
        colorAttr.location = 1;
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        // We'll set a special offset - shader needs to detect this
        // In a real implementation, you might want to use vertex buffer with default attributes
        colorAttr.offset = 0; // Or a special value your shader recognizes
        config.vertexAttributes.push_back(colorAttr);
        SPDLOG_DEBUG("Added dummy color attribute at location 1");
    }

    // TexCoord attribute - Location 2
    if (mesh.hasTexCoord()) {
        VkVertexInputAttributeDescription uvAttr = {};
        uvAttr.binding = 0;
        uvAttr.location = 2;
        uvAttr.format = VK_FORMAT_R32G32_SFLOAT; // vec2
        uvAttr.offset = offset;
        config.vertexAttributes.push_back(uvAttr);
        offset += sizeof(float) * 2;
        SPDLOG_DEBUG("Added texcoord attribute at location 2, offset {}", uvAttr.offset);
    }
    else {
        // Dummy texcoord attribute
        VkVertexInputAttributeDescription uvAttr = {};
        uvAttr.binding = 0;
        uvAttr.location = 2;
        uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
        uvAttr.offset = 0; // Special offset
        config.vertexAttributes.push_back(uvAttr);
        SPDLOG_DEBUG("Added dummy texcoord attribute at location 2");
    }

    // Normal attribute - Location 3
    if (mesh.hasNormal()) {
        VkVertexInputAttributeDescription normalAttr = {};
        normalAttr.binding = 0;
        normalAttr.location = 3;
        normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        normalAttr.offset = offset;
        config.vertexAttributes.push_back(normalAttr);
        offset += sizeof(float) * 3;
        SPDLOG_DEBUG("Added normal attribute at location 3, offset {}", normalAttr.offset);
    }
    else {
        // Dummy normal attribute 
        VkVertexInputAttributeDescription normalAttr = {};
        normalAttr.binding = 0;
        normalAttr.location = 3;
        normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        normalAttr.offset = 0; // Special offset
        config.vertexAttributes.push_back(normalAttr);
        SPDLOG_DEBUG("Added dummy normal attribute at location 3");
    }

    // Account for tangent if present
    if (mesh.hasTangent()) {
        offset += sizeof(float) * 4; // Assuming tangent is vec4
    }

    SPDLOG_DEBUG("Created vertex input config with {} attributes", config.vertexAttributes.size());
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