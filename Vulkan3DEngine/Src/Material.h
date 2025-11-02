#pragma once
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include "ShaderManager.h"
#include "VramManager.h"
#include "AssetHandle.h"
#include "AssetLib.h"
#include "ImageSamplerManager.h"
#include "ShaderLib.h"
#include "TextureManager.h"
#include "BufferManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include <glm/glm.hpp>

// Forward declarations
class MaterialManager;

class Material {
public:
    // Texture parameter type
    struct TextureParam {
        AssetHandle handle;
        TextureHandle textureHandle;
        SamplerHandle samplerHandle;
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB;
    };

    // Parameter value variant
    using ParamValue = std::variant<
        ShaderLib::BufferValue,  // All buffer-compatible types (UBO/SSBO)
        TextureParam             // Textures (descriptors only)
    >;

    // Parameter structure (optimized - removed redundant fields)
    struct Parameter {
        std::string name;
        ParamValue value;
        ShaderLib::DescriptorType descriptorType;
        uint32_t binding;  // Cached from shader metadata for fast binding

        ShaderLib::BaseType getBaseType() const {
            if (auto* bufVal = std::get_if<ShaderLib::BufferValue>(&value)) {
                return ShaderLib::GetBaseTypeFromVariant(*bufVal);
            }
            return ShaderLib::BaseType::Unknown;
        }

        bool isBufferParameter() const {
            return descriptorType == ShaderLib::DescriptorType::UniformBuffer ||
                descriptorType == ShaderLib::DescriptorType::StorageBuffer;
        }

        bool isTextureParameter() const {
            return std::holds_alternative<TextureParam>(value);
        }
    };

    // Constructor
    Material(
        const std::string& name,
        SmartAssetHandle<ShaderHandle, ShaderAsset> smartShader,
        const std::vector<Parameter>& parameters,
        const LogicalDevice& device,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager
    );

    ~Material();

    // Accessors
    const std::string& name() const { return m_name; }
    const SmartAssetHandle<ShaderHandle, ShaderAsset>& shader() const { return m_shader; }
    const std::vector<Parameter>& parameters() const { return m_parameters; }
    std::vector<Parameter>& parameters() { return m_parameters; }

    // Parameter management - automatically invalidates descriptor set
    bool setParameter(const std::string& name, const ParamValue& value);
    bool getParameter(const std::string& name, ParamValue& outValue) const;

    // Convenience methods for common types
    template<typename T>
    bool setBufferParameter(const std::string& name, const T& value) {
        static_assert(ShaderLib::BaseTypeTraits<T>::supported,
            "Type not supported by ShaderLib");

        ShaderLib::BufferValue bufVal = value;
        return setParameter(name, ParamValue{ bufVal });
    }

    bool setTextureParameter(const std::string& name, const TextureParam& texture) {
        return setParameter(name, ParamValue{ texture });
    }

    // GPU Buffer Readback - Read data from GPU buffers back to CPU parameters
    // These methods update the Material's parameter values with current GPU buffer contents

    /**
     * Read all buffer parameters from GPU back to CPU
     * Updates all parameter values with current GPU buffer contents
     * @return true if at least one parameter was read successfully
     */
    bool readbackBufferParameters();

    /**
     * Read specific buffer parameter from GPU back to CPU
     * @param name Name of the buffer parameter to read
     * @return true if parameter was read successfully
     */
    bool readbackBufferParameter(const std::string& name);

    /**
     * Get list of all buffer parameter names in this material
     * @return Vector of parameter names that are buffer parameters (UBO/SSBO)
     */
    std::vector<std::string> getBufferParameterNames() const;

    /**
     * Check if material has any buffer parameters
     * @return true if material has at least one buffer parameter
     */
    bool hasBufferParameters() const;

    // Descriptor set access - creates on demand and manages cache
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> getDescriptorSet();

    // Manually invalidate descriptor set (usually not needed - setParameter does this automatically)
    void invalidateDescriptorSet();

private:
    // Descriptor set management
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createDescriptorSet();
    bool needsDescriptorSetRecreation() const;
    void collectSamplerHandles();

    // Helper for binding parameters to descriptor set (optimized to use cached bindings)
    void bindParametersToBuilder(class DescriptorSetBuilder& builder);
    const ShaderLib::BufferObject* findBufferForVariable(
        const ShaderLib::DescriptorSet& descriptorSet,
        const std::string& variableName
    ) const;

    // Buffer readback helpers
    /**
     * Read single parameter value from its GPU buffer
     * @param param Parameter to read (will be updated with GPU value)
     * @return true if read was successful
     */
    bool readParameterFromBuffer(Parameter& param);

    /**
     * Get or find buffer handle for a parameter
     * Uses cached buffers from m_materialBuffers when available
     * @param param Parameter to get buffer for
     * @return Smart handle to the buffer, invalid if not found
     */
    SmartHandle<BufferHandle, Buffer> getBufferForParameter(const Parameter& param);

    // Core data
    std::string m_name;
    SmartAssetHandle<ShaderHandle, ShaderAsset> m_shader;
    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, size_t> m_parameterIndices;

    // Dependencies (non-owning references)
    const LogicalDevice& m_device;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;

    // Descriptor set cache
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_descriptorSet;
    bool m_descriptorSetValid;

    // Track sampler handles for dirty checking
    std::vector<SamplerHandle> m_samplerHandles;

    // Buffer cache for readback operations
    // Map: binding -> BufferHandle
    // These are the buffers created during descriptor set binding
    // Cached here so we can read from them without recreating
    std::unordered_map<uint32_t, SmartHandle<BufferHandle, Buffer>> m_materialBuffers;

    friend class MaterialManager;
};