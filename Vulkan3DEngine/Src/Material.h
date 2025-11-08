#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "ShaderManager.h"
#include "BufferManager.h"
#include "AssetHandle.h"
#include "ImageSamplerManager.h"
#include "TextureManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include "BufferObjectInstance.h"
#include "ShaderLib.h"

// Forward declarations
class MaterialManager;

/**
 * Material - High-level wrapper for Custom Descriptor Set (set 2)
 *
 * Provides intuitive field access with full support for:
 * - Nested structures: material["transform"]["position"]
 * - Arrays: material["colors"][2]
 * - Direct value access: float roughness = material["roughness"]
 * - Partial GPU sync: material.SyncFieldToGPU("transform.position")
 * - Texture management
 *
 * Design principles:
 * - Delegates complex operations to BufferObjectInstance/FieldProxy
 * - Provides convenient high-level API
 * - Maintains type safety and field validation
 */
class Material {
public:
    // Texture parameter
    struct TextureParam {
        AssetHandle assetHandle;
        TextureHandle textureHandle;
        SamplerHandle samplerHandle;
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB;
    };

    // Field metadata (enriched with structure info)
    struct FieldInfo {
        std::string name;              // Field name
        std::string path;              // Full path (e.g., "transform.position")
        ShaderLib::BaseType baseType;  // Type (Unknown for structures)
        uint32_t binding;              // Descriptor binding
        bool isBaseType;               // true = primitive, false = structure
        bool isArray;                  // true if array
        uint32_t arraySize;            // 0 if not array
        uint32_t offset;               // Offset in buffer
        uint32_t size;                 // Size in bytes
    };

    // Constructor
    Material(
        const std::string& name,
        SmartAssetHandle<ShaderHandle, ShaderAsset> shader,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer,
        const LogicalDevice& device,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager
    );

    ~Material();

    // Basic accessors
    const std::string& GetName() const { return m_name; }
    const SmartAssetHandle<ShaderHandle, ShaderAsset>& GetShader() const { return m_shader; }

    // ==========================================================================
    // FIELD ACCESS - Full FieldProxy support with chaining
    // ==========================================================================

    /**
     * Access field by name - supports chaining for nested structures and arrays
     *
     * Examples:
     *   material["color"] = glm::vec3(1.0f, 0.0f, 0.0f);
     *   material["transform"]["position"] = glm::vec3(0, 1, 0);
     *   material["colors"][2] = glm::vec4(1.0f);
     *   float roughness = material["roughness"];
     *
     * @param name Top-level field name (without dots/brackets)
     * @return FieldProxy for chaining operations
     * @throws std::runtime_error if field not found
     */
    ShaderLib::FieldProxy operator[](const std::string& name);
    ShaderLib::FieldProxy operator[](const char* name);

    /**
     * Access field by full path - for explicit path-based access
     *
     * Examples:
     *   material.GetField("transform.position")
     *   material.GetField("colors[2]")
     *
     * @param path Full field path
     * @return FieldProxy for the field
     * @throws std::runtime_error if field not found
     */
    ShaderLib::FieldProxy GetField(const std::string& path);

    /**
     * Typed field access - convenience wrappers
     */
    template<typename T>
    T Get(const std::string& path) const;

    template<typename T>
    void Set(const std::string& path, const T& value);

    /**
     * Check if field exists (supports both top-level names and full paths)
     * @param nameOrPath Field name or full path
     */
    bool HasField(const std::string& nameOrPath) const;

    /**
     * Get all top-level field names (unique base names without paths)
     */
    std::vector<std::string> GetFieldNames() const;

    /**
     * Get all field paths (includes nested fields and array elements)
     */
    std::vector<std::string> GetAllFieldPaths() const;

    /**
     * Get field metadata
     * @param nameOrPath Field name or full path
     * @return Field info, or nullptr if not found
     */
    const FieldInfo* GetFieldInfo(const std::string& nameOrPath) const;

    // ==========================================================================
    // ARRAY FIELD OPERATIONS
    // ==========================================================================

    /**
     * Check if field is an array
     * @param name Field name (without array index)
     */
    bool IsArrayField(const std::string& name) const;

    /**
     * Get array size
     * @param name Field name
     * @return Array size, or 0 if not an array
     */
    size_t GetArraySize(const std::string& name) const;

    /**
     * Set entire array from vector
     * Example: material.SetArray("colors", colorVector);
     */
    template<typename T>
    void SetArray(const std::string& name, const std::vector<T>& values);

    /**
     * Get entire array as vector
     * Example: auto colors = material.GetArray<glm::vec3>("colors");
     */
    template<typename T>
    std::vector<T> GetArray(const std::string& name) const;

    // ==========================================================================
    // STRUCTURE FIELD OPERATIONS
    // ==========================================================================

    /**
     * Check if field is a structure (not a base type)
     */
    bool IsStructureField(const std::string& name) const;

    /**
     * Get all child field names of a structure
     * Example: auto children = material.GetStructureChildren("transform");
     * Returns: {"position", "rotation", "scale"}
     */
    std::vector<std::string> GetStructureChildren(const std::string& name) const;

    // ==========================================================================
    // TEXTURE MANAGEMENT
    // ==========================================================================

    bool SetTexture(const std::string& name, const TextureParam& texture);
    bool GetTexture(const std::string& name, TextureParam& outTexture) const;
    std::vector<std::string> GetTextureNames() const;
    bool HasTexture(const std::string& name) const;

    // ==========================================================================
    // GPU SYNCHRONIZATION - Full spectrum from bulk to granular
    // ==========================================================================

    /**
     * BULK SYNC: Transfer entire buffer(s)
     */
    void SyncToGPU();                    // CPU -> GPU (all buffers)
    void SyncFromGPU();                  // GPU -> CPU (all buffers)

    // More advanced control through buffer instances

    // ==========================================================================
    // DESCRIPTOR SET MANAGEMENT
    // ==========================================================================

    SmartHandle<DescriptorSetHandle, VkDescriptorSet> GetDescriptorSet();
    void InvalidateDescriptorSet();
    bool IsDescriptorSetValid() const { return m_descriptorSetValid; }

    // ==========================================================================
    // BUFFER ACCESS
    // ==========================================================================

    std::shared_ptr<ShaderLib::BufferObjectInstance> GetInputBuffer() const { return m_inputBuffer; }
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetOutputBuffer() const { return m_outputBuffer; }
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetInputOutputBuffer() const { return m_inputOutputBuffer; }

    bool HasInputBuffer() const { return m_inputBuffer != nullptr; }
    bool HasOutputBuffer() const { return m_outputBuffer != nullptr; }
    bool HasInputOutputBuffer() const { return m_inputOutputBuffer != nullptr; }

    /**
     * Find which buffer contains a field
     */
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetBufferForField(const std::string& path) const;

private:
    // Field mapping (now includes structures)
    struct FieldMapping {
        std::string fieldName;          // Base name (without path)
        std::string fullPath;           // Full path
        ShaderLib::BufferType bufferType;
        uint32_t binding;
        bool isBaseType;
        bool isArray;
        uint32_t arraySize;

        std::shared_ptr<ShaderLib::BufferObjectInstance> GetBufferInstance(
            const std::shared_ptr<ShaderLib::BufferObjectInstance>& input,
            const std::shared_ptr<ShaderLib::BufferObjectInstance>& output,
            const std::shared_ptr<ShaderLib::BufferObjectInstance>& inputOutput
        ) const;
    };

    // Texture binding
    struct TextureBinding {
        std::string name;
        uint32_t binding;
        ShaderLib::DescriptorType type;
        TextureParam texture;
    };

    // Initialization
    void BuildFieldMappings();
    void CollectSamplerHandles();

    // Descriptor set
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> CreateDescriptorSet();
    bool NeedsDescriptorSetRecreation() const;
    void AcquireBuffersForDescriptorSet();
    void ReleaseDescriptorSetBuffers();

    // Helper methods
    const FieldMapping* FindFieldMapping(const std::string& nameOrPath) const;
    std::string ExtractTopLevelName(const std::string& nameOrPath) const;
    const ShaderLib::DescriptorSet* GetCustomDescriptorSet() const;

    // Collect all paths that belong to a structure
    std::vector<std::string> CollectStructurePaths(const std::string& structureName) const;

    // Collect all paths that belong to an array
    std::vector<std::string> CollectArrayPaths(const std::string& arrayName) const;

    // Core data
    std::string m_name;
    SmartAssetHandle<ShaderHandle, ShaderAsset> m_shader;

    // Buffer instances
    std::shared_ptr<ShaderLib::BufferObjectInstance> m_inputBuffer;
    std::shared_ptr<ShaderLib::BufferObjectInstance> m_outputBuffer;
    std::shared_ptr<ShaderLib::BufferObjectInstance> m_inputOutputBuffer;

    // Field mappings: path -> mapping (includes structures and base types)
    std::unordered_map<std::string, FieldMapping> m_fieldMappings;

    // Additional mapping: top-level name -> all paths starting with that name
    std::unordered_map<std::string, std::vector<std::string>> m_topLevelToPaths;

    // Cached field info
    std::unordered_map<std::string, FieldInfo> m_fieldInfoCache;

    // Texture bindings
    std::unordered_map<std::string, TextureBinding> m_textureBindings;

    // Dependencies
    const LogicalDevice& m_device;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;

    // Descriptor set cache
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_descriptorSet;
    bool m_descriptorSetValid;

    // Buffer handles
    SmartHandle<BufferHandle, Buffer> m_inputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_outputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_inputOutputBufferHandle;

    // Sampler handles for dirty checking
    std::vector<SamplerHandle> m_samplerHandles;

    friend class MaterialManager;
};

// =============================================================================
// TEMPLATE IMPLEMENTATIONS
// =============================================================================

template<typename T>
inline T Material::Get(const std::string& path) const {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("Material " + m_name + ": Field not found: " + path);
    }
    return buffer->Get<T>(path);
}

template<typename T>
inline void Material::Set(const std::string& path, const T& value) {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("Material " + m_name + ": Field not found: " + path);
    }
    buffer->Set(path, value);
}

template<typename T>
inline void Material::SetArray(const std::string& name, const std::vector<T>& values) {
    if (!IsArrayField(name)) {
        throw std::runtime_error("Material " + m_name + ": Field is not an array: " + name);
    }

    size_t arraySize = GetArraySize(name);
    if (values.size() > arraySize) {
        throw std::runtime_error("Material " + m_name + ": Array size mismatch for " + name);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        operator[](name)[i] = values[i];
    }
}

template<typename T>
inline std::vector<T> Material::GetArray(const std::string& name) const {
    if (!IsArrayField(name)) {
        throw std::runtime_error("Material " + m_name + ": Field is not an array: " + name);
    }

    size_t arraySize = GetArraySize(name);
    std::vector<T> result;
    result.reserve(arraySize);

    auto buffer = GetBufferForField(name);
    if (!buffer) {
        throw std::runtime_error("Material " + m_name + ": Buffer not found for array: " + name);
    }

    for (size_t i = 0; i < arraySize; ++i) {
        std::string elementPath = name + "[" + std::to_string(i) + "]";
        result.push_back(buffer->Get<T>(elementPath));
    }

    return result;
}
