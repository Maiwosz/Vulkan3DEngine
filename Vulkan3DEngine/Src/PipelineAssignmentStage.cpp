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
            break;
        case ShaderLib::Stage::Fragment:
            config.shaderStages.fragmentShader = moduleHandle;
            break;
        case ShaderLib::Stage::Geometry:
            config.shaderStages.geometryShader = moduleHandle;
            break;
        case ShaderLib::Stage::TessellationControl:
            config.shaderStages.tessControlShader = moduleHandle;
            break;
        case ShaderLib::Stage::TessellationEvaluation:
            config.shaderStages.tessEvalShader = moduleHandle;
            break;
        case ShaderLib::Stage::Compute:
            config.shaderStages.computeShader = moduleHandle;
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

    // Configure vertex input based on mesh format
    config.vertexInput = createVertexInputConfig(mesh);

    // Configure viewport - use dynamic viewport and scissor
    config.viewport.dynamicViewport = true;
    config.viewport.dynamicScissor = true;

    // Configure rasterization
    config.rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    config.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.rasterization.lineWidth = 1.0f;

    // Configure depth/stencil
    config.depthStencil.depthTestEnable = true;
    config.depthStencil.depthWriteEnable = true;
    config.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    // Configure multisampling
    config.multisample.samples = VK_SAMPLE_COUNT_1_BIT;

    // Configure color blending
    config.colorBlend.blendEnable = false;

    // Set render pass
    config.renderPass.renderPass = m_renderPassManager.get(renderPassHandle);
    config.renderPass.subpass = 0;

    return config;
}

VertexInputConfig PipelineAssignmentStage::createVertexInputConfig(const Mesh& mesh) {
    VertexInputConfig config;

    // Binding description - one binding for all attributes
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = mesh.vertexStride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    config.vertexBindings.push_back(bindingDesc);

    // Attribute descriptions based on current shader layout:
    // layout(location = 0) in vec3 inPosition;
    // layout(location = 1) in vec3 inColor;
    // layout(location = 2) in vec2 inTexCoord;
    // layout(location = 3) in vec3 inNormal;
    //
    // FIXME: This is currently using hardcoded locations based on the shader.
    // TODO: Implement dynamic shader reflection to get actual attribute locations from shaders
    uint32_t offset = 0;

    // Position attribute (always present) - Always location 0
    if (mesh.hasPosition()) {
        VkVertexInputAttributeDescription posAttr = {};
        posAttr.binding = 0;
        posAttr.location = 0; // Fixed location for position
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        posAttr.offset = offset;
        config.vertexAttributes.push_back(posAttr);
        offset += sizeof(float) * 3;
    }

    // CRITICAL FIX: Always include required attributes that the shader expects
    // even if the mesh doesn't have them - provide dummy data

    // Color attribute - Always location 1 according to shader
    // Add it regardless of whether the mesh has color data
    {
        VkVertexInputAttributeDescription colorAttr = {};
        colorAttr.binding = 0;
        colorAttr.location = 1; // Fixed location for color
        colorAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3 as per shader

        if (mesh.hasColor()) {
            // If mesh has color, use the actual offset
            colorAttr.offset = offset;
            offset += sizeof(float) * 4; // Assuming color is vec4 in mesh
        }
        else {
            // If mesh doesn't have color, use position's offset
            // This means the shader will use position data as color data
            // Not ideal but prevents pipeline creation error
            colorAttr.offset = 0; // Use position data
        }

        config.vertexAttributes.push_back(colorAttr);
    }

    // TexCoord attribute - Always location 2 according to shader
    // Add it regardless of whether the mesh has texcoord data
    {
        VkVertexInputAttributeDescription uvAttr = {};
        uvAttr.binding = 0;
        uvAttr.location = 2; // Fixed location for texcoord
        uvAttr.format = VK_FORMAT_R32G32_SFLOAT; // vec2

        if (mesh.hasTexCoord()) {
            // If mesh has texcoord, use the actual offset
            uvAttr.offset = offset;
            offset += sizeof(float) * 2;
        }
        else {
            // If mesh doesn't have texcoord, use position's offset
            uvAttr.offset = 0; // Use position data
        }

        config.vertexAttributes.push_back(uvAttr);
    }

    // Normal attribute - Always location 3 according to shader
    // Add it regardless of whether the mesh has normal data
    {
        VkVertexInputAttributeDescription normalAttr = {};
        normalAttr.binding = 0;
        normalAttr.location = 3; // Fixed location for normal
        normalAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3

        if (mesh.hasNormal()) {
            // If mesh has normal, use the actual offset
            normalAttr.offset = offset;
            offset += sizeof(float) * 3;
        }
        else {
            // If mesh doesn't have normal, use position's offset
            normalAttr.offset = 0; // Use position data
        }

        config.vertexAttributes.push_back(normalAttr);
    }

    // Tangent attribute (not used in current shader)
    if (mesh.hasTangent()) {
        // Although not required by shader, still track the offset
        offset += sizeof(float) * 4;
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