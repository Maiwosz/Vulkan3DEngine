//#pragma once
//#include "ShaderTypes.h"
//#include <string>
//#include <vector>
//#include <memory>
//#include <json.hpp>
//#include <stdexcept>
//
//using json = nlohmann::json;
//
//namespace ShaderLib {
//
//    // ============================================================================
//    // BUFFER ACCESS MODE
//    // ============================================================================
//
//    enum class BufferAccessMode : uint8_t {
//        ReadOnly,   // uniform buffers, readonly storage buffers
//        WriteOnly,  // writeonly storage buffers
//        ReadWrite   // read/write storage buffers
//    };
//
//    // ============================================================================
//    // BUFFER TYPE
//    // ============================================================================
//
//    enum class BufferType {
//        Uniform,  // UBO - always ReadOnly
//        Storage   // SSBO - can be ReadOnly, WriteOnly, or ReadWrite
//    };
//
//    // Forward declarations
//    class BufferObjectInstance;
//
//    // ============================================================================
//    // BUFFER FIELD DEFINITION - Single field metadata
//    // ============================================================================
//
//    struct BufferFieldDefinition {
//        std::string name;
//        BaseType baseType;
//        std::shared_ptr<const CompositeTypeDefinition> composite;
//        uint32_t size;
//        uint32_t offset;
//        BufferAccessMode accessMode;
//
//        // Constructor for base types
//        BufferFieldDefinition(const std::string& n, BaseType t, uint32_t sz, uint32_t off,
//            BufferAccessMode mode = BufferAccessMode::ReadWrite)
//            : name(n), baseType(t), composite(nullptr),
//            size(sz), offset(off), accessMode(mode) {
//        }
//
//        // Constructor for composite types
//        BufferFieldDefinition(const std::string& n,
//            std::shared_ptr<const CompositeTypeDefinition> comp,
//            uint32_t off, BufferAccessMode mode = BufferAccessMode::ReadWrite)
//            : name(n),
//            baseType(comp->IsStruct() ? BaseType::Struct : BaseType::Array),
//            composite(comp),
//            size(comp->GetSize()),
//            offset(off),
//            accessMode(mode) {
//        }
//
//        // Default constructor for serialization
//        BufferFieldDefinition()
//            : name(""), baseType(BaseType::Unknown), composite(nullptr),
//            size(0), offset(0), accessMode(BufferAccessMode::ReadOnly) {
//        }
//
//        // Type checking
//        bool IsBase() const {
//            return !IsComposite() && baseType != BaseType::Unknown;
//        }
//
//        bool IsComposite() const {
//            return composite != nullptr;
//        }
//
//        bool IsStruct() const {
//            return composite && composite->IsStruct();
//        }
//
//        bool IsArray() const {
//            return composite && composite->IsArray();
//        }
//
//        // Access mode checking
//        bool IsReadOnly() const {
//            return accessMode == BufferAccessMode::ReadOnly;
//        }
//
//        bool IsWriteOnly() const {
//            return accessMode == BufferAccessMode::WriteOnly;
//        }
//
//        bool IsReadWrite() const {
//            return accessMode == BufferAccessMode::ReadWrite;
//        }
//
//        // Type information
//        std::string GetTypeName() const;
//        ShaderTypeCategory GetCategory() const;
//        std::string GenerateGLSLDeclaration() const;
//
//        // Serialization
//        json ToJson() const;
//        static BufferFieldDefinition FromJson(const json& j);
//    };
//
//    // ============================================================================
//    // BUFFER OBJECT DEFINITION - Immutable buffer metadata
//    // ============================================================================
//
//    class BufferObjectDefinition : public std::enable_shared_from_this<BufferObjectDefinition> {
//    private:
//        std::string name;
//        uint32_t totalSize;
//        BufferType bufferType;
//        LayoutStandard layoutStandard;
//        BufferAccessMode bufferAccessMode;
//        std::vector<BufferFieldDefinition> fields;
//        bool useInstanceName;
//
//        // Field lookup cache (optional optimization)
//        mutable std::unordered_map<std::string, size_t> fieldIndexCache;
//        void BuildFieldCache() const;
//
//    public:
//        // ============================================================================
//        // CONSTRUCTION
//        // ============================================================================
//
//        BufferObjectDefinition(
//            const std::string& bufferName,
//            BufferType type,
//            LayoutStandard standard,
//            BufferAccessMode accessMode,
//            const std::vector<BufferFieldDefinition>& bufferFields,
//            bool useInstance = true
//        );
//
//        // ============================================================================
//        // METADATA ACCESS
//        // ============================================================================
//
//        const std::string& GetName() const { return name; }
//        uint32_t GetSize() const { return totalSize; }
//        BufferType GetBufferType() const { return bufferType; }
//        LayoutStandard GetLayoutStandard() const { return layoutStandard; }
//        BufferAccessMode GetAccessMode() const { return bufferAccessMode; }
//        bool UsesInstanceName() const { return useInstanceName; }
//
//        // ============================================================================
//        // FIELD ACCESS
//        // ============================================================================
//
//        size_t GetFieldCount() const { return fields.size(); }
//        const BufferFieldDefinition& GetField(size_t index) const;
//        const BufferFieldDefinition* FindField(const std::string& fieldName) const;
//
//        // STL-like iteration
//        using const_iterator = std::vector<BufferFieldDefinition>::const_iterator;
//        const_iterator begin() const { return fields.begin(); }
//        const_iterator end() const { return fields.end(); }
//        const_iterator cbegin() const { return fields.cbegin(); }
//        const_iterator cend() const { return fields.cend(); }
//
//        // ============================================================================
//        // TYPE CHECKING
//        // ============================================================================
//
//        bool IsUniformBuffer() const {
//            return bufferType == BufferType::Uniform;
//        }
//
//        bool IsStorageBuffer() const {
//            return bufferType == BufferType::Storage;
//        }
//
//        bool IsReadOnly() const {
//            return bufferAccessMode == BufferAccessMode::ReadOnly;
//        }
//
//        bool IsWriteOnly() const {
//            return bufferAccessMode == BufferAccessMode::WriteOnly;
//        }
//
//        bool IsReadWrite() const {
//            return bufferAccessMode == BufferAccessMode::ReadWrite;
//        }
//
//        // ============================================================================
//        // GLSL GENERATION
//        // ============================================================================
//
//        std::string GenerateGLSL(uint32_t setNumber, uint32_t binding) const;
//        std::string GetAccessQualifier() const;
//
//        // ============================================================================
//        // SERIALIZATION
//        // ============================================================================
//
//        json ToJson() const;
//        static std::shared_ptr<BufferObjectDefinition> FromJson(const json& j);
//
//        // ============================================================================
//        // FACTORY - Creates instance from this definition
//        // ============================================================================
//
//        std::shared_ptr<BufferObjectInstance> CreateInstance() const;
//
//        // ============================================================================
//        // VALIDATION
//        // ============================================================================
//
//        bool Validate() const;
//    };
//
//    // ============================================================================
//    // BUILDER - Fluent interface for creating BufferObjectDefinition
//    // ============================================================================
//
//    class BufferObjectBuilder {
//    private:
//        std::string name;
//        BufferType bufferType;
//        LayoutStandard layoutStandard;
//        BufferAccessMode defaultAccessMode;
//        std::vector<BufferFieldDefinition> fields;
//        bool useInstanceName = true;
//
//        uint32_t GetCurrentSize() const;
//        void ValidateAccessMode(BufferAccessMode mode) const;
//        BufferAccessMode ComputeBufferAccessMode() const;
//
//    public:
//        // ============================================================================
//        // CONSTRUCTION
//        // ============================================================================
//
//        explicit BufferObjectBuilder(const std::string& bufferName);
//
//        BufferObjectBuilder(const std::string& bufferName,
//            BufferType type,
//            BufferAccessMode defaultAccess = BufferAccessMode::ReadWrite,
//            LayoutStandard standard = LayoutStandard::Std140);
//
//        // ============================================================================
//        // CONFIGURATION
//        // ============================================================================
//
//        BufferObjectBuilder& SetBufferType(BufferType type);
//        BufferObjectBuilder& SetDefaultAccessMode(BufferAccessMode mode);
//        BufferObjectBuilder& SetLayoutStandard(LayoutStandard standard);
//        BufferObjectBuilder& SetUseInstanceName(bool use);
//
//        // ============================================================================
//        // ADD FIELDS - Base types by BaseType enum
//        // ============================================================================
//
//        BufferObjectBuilder& AddField(const std::string& fieldName, BaseType type);
//        BufferObjectBuilder& AddField(const std::string& fieldName, BaseType type,
//            BufferAccessMode accessMode);
//
//        // ============================================================================
//        // ADD FIELDS - Compile-time type safety
//        // ============================================================================
//
//        template<typename T>
//        BufferObjectBuilder& AddField(const std::string& fieldName) {
//            return AddField(fieldName, GetBaseTypeOf<T>(), defaultAccessMode);
//        }
//
//        template<typename T>
//        BufferObjectBuilder& AddField(const std::string& fieldName, BufferAccessMode accessMode) {
//            static_assert(IsBaseTypeSupported<T>(), "Type not supported in buffers");
//            return AddField(fieldName, GetBaseTypeOf<T>(), accessMode);
//        }
//
//        // ============================================================================
//        // ADD COMPOSITE FIELDS
//        // ============================================================================
//
//        BufferObjectBuilder& AddCompositeField(const std::string& fieldName,
//            std::shared_ptr<const CompositeTypeDefinition> compositeDefinition);
//
//        BufferObjectBuilder& AddCompositeField(const std::string& fieldName,
//            std::shared_ptr<const CompositeTypeDefinition> compositeDefinition,
//            BufferAccessMode accessMode);
//
//        // ============================================================================
//        // BUILD
//        // ============================================================================
//
//        std::shared_ptr<BufferObjectDefinition> Build();
//    };
//
//} // namespace ShaderLib