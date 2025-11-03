#pragma once
#include "ShaderTypes.h"
#include "ShaderStructDefinition.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <json.hpp>
#include <stdexcept>
#include <iterator>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // SHADER STRUCT INSTANCE - Mutable instance data with STL-like interface
    // ============================================================================

    class ShaderStructInstance : public CompositeTypeInstance,
        public std::enable_shared_from_this<ShaderStructInstance> {
    private:
        std::shared_ptr<const ShaderStructDefinition> definition;
        std::unordered_map<std::string, BufferValue> fieldValues;
        std::vector<uint8_t> buffer;
        mutable bool bufferDirty = false;

        void InitializeFieldDefaults();
        void WriteFieldToBuffer(const ShaderStructDefinition::Field& field, const BufferValue& value);
        void ReadFieldFromBuffer(const ShaderStructDefinition::Field& field);
        BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance) const;
        void SyncBufferIfDirty() const;

    public:
        // ============================================================================
        // TYPE ALIASES (STL-like)
        // ============================================================================
        using key_type = std::string;
        using mapped_type = BufferValue;
        using value_type = std::pair<const std::string, BufferValue>;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = BufferValue&;
        using const_reference = const BufferValue&;
        using pointer = BufferValue*;
        using const_pointer = const BufferValue*;
        using iterator = std::unordered_map<std::string, BufferValue>::iterator;
        using const_iterator = std::unordered_map<std::string, BufferValue>::const_iterator;

        // ============================================================================
        // CONSTRUCTORS & ASSIGNMENT
        // ============================================================================
        explicit ShaderStructInstance(std::shared_ptr<const ShaderStructDefinition> def);
        ShaderStructInstance(const ShaderStructInstance& other);
        ShaderStructInstance(ShaderStructInstance&& other) noexcept;
        ShaderStructInstance& operator=(const ShaderStructInstance& other);
        ShaderStructInstance& operator=(ShaderStructInstance&& other) noexcept;

        // Construct from initializer list of field values
        ShaderStructInstance(std::shared_ptr<const ShaderStructDefinition> def,
            std::initializer_list<std::pair<const std::string, BufferValue>> fields);

        // ============================================================================
        // ELEMENT ACCESS
        // ============================================================================

        // Map-like access with bounds checking
        BufferValue& at(const std::string& fieldName);
        const BufferValue& at(const std::string& fieldName) const;

        // Map-like access without bounds checking (creates default if not exists for non-const)
        BufferValue& operator[](const std::string& fieldName);

        // Type-safe getters (throw if wrong type)
        template<typename T>
        T& Get(const std::string& fieldName) {
            return std::get<T>(at(fieldName));
        }

        template<typename T>
        const T& Get(const std::string& fieldName) const {
            return std::get<T>(at(fieldName));
        }

        // Type-safe getters with default value (no throw)
        template<typename T>
        T GetOr(const std::string& fieldName, const T& defaultValue) const {
            auto it = fieldValues.find(fieldName);
            if (it == fieldValues.end()) {
                return defaultValue;
            }
            try {
                return std::get<T>(it->second);
            }
            catch (const std::bad_variant_access&) {
                return defaultValue;
            }
        }

        // ============================================================================
        // COMPOSITE FIELD ACCESS (for struct/array fields)
        // ============================================================================

        // Get composite field (returns nullptr if not composite or not found)
        std::shared_ptr<CompositeTypeInstance> GetComposite(const std::string& fieldName);
        std::shared_ptr<const CompositeTypeInstance> GetComposite(const std::string& fieldName) const;

        // Set composite field
        void SetComposite(const std::string& fieldName, std::shared_ptr<CompositeTypeInstance> value);

        // Type-safe composite getters
        template<typename T>
        std::shared_ptr<T> GetCompositeAs(const std::string& fieldName) {
            auto composite = GetComposite(fieldName);
            return composite ? std::dynamic_pointer_cast<T>(composite) : nullptr;
        }

        template<typename T>
        std::shared_ptr<const T> GetCompositeAs(const std::string& fieldName) const {
            auto composite = GetComposite(fieldName);
            return composite ? std::dynamic_pointer_cast<const T>(composite) : nullptr;
        }

        // ============================================================================
        // ITERATORS
        // ============================================================================

        iterator begin() noexcept { return fieldValues.begin(); }
        const_iterator begin() const noexcept { return fieldValues.begin(); }
        const_iterator cbegin() const noexcept { return fieldValues.cbegin(); }

        iterator end() noexcept { return fieldValues.end(); }
        const_iterator end() const noexcept { return fieldValues.end(); }
        const_iterator cend() const noexcept { return fieldValues.cend(); }

        // ============================================================================
        // CAPACITY
        // ============================================================================

        bool empty() const noexcept { return fieldValues.empty(); }
        size_type size() const noexcept { return fieldValues.size(); }
        size_type max_size() const noexcept { return fieldValues.max_size(); }

        // ============================================================================
        // MODIFIERS
        // ============================================================================

        // Clear all field values (resets to defaults)
        void clear();

        // Insert or assign field value
        template<typename T>
        std::pair<iterator, bool> insert_or_assign(const std::string& fieldName, T&& value) {
            at(fieldName) = std::forward<T>(value);
            const auto* field = definition->FindField(fieldName);
            if (field && !field->IsComposite()) {
                WriteFieldToBuffer(*field, at(fieldName));
            }
            return { fieldValues.find(fieldName), true };
        }

        // Swap contents
        void swap(ShaderStructInstance& other) noexcept;

        // ============================================================================
        // LOOKUP
        // ============================================================================

        // Count (always 0 or 1 for fields)
        size_type count(const std::string& fieldName) const {
            return fieldValues.count(fieldName);
        }

        // Check if field exists
        bool contains(const std::string& fieldName) const {
            return fieldValues.find(fieldName) != fieldValues.end();
        }

        // Find field
        iterator find(const std::string& fieldName) {
            return fieldValues.find(fieldName);
        }

        const_iterator find(const std::string& fieldName) const {
            return fieldValues.find(fieldName);
        }

        // ============================================================================
        // CONVERSION TO/FROM MAP
        // ============================================================================

        // Convert to map (copy)
        std::unordered_map<std::string, BufferValue> ToMap() const;

        // Assign from map (validates all fields)
        void FromMap(const std::unordered_map<std::string, BufferValue>& values);

        // ============================================================================
        // FIELD INFORMATION
        // ============================================================================

        // Get all field names from definition
        std::vector<std::string> GetFieldNames() const;

        // Check if field is composite type
        bool IsFieldComposite(const std::string& fieldName) const;

        // Get field base type (throws if composite)
        BaseType GetFieldBaseType(const std::string& fieldName) const;

        // Get field size in bytes
        uint32_t GetFieldSize(const std::string& fieldName) const;

        // Get field offset in buffer
        uint32_t GetFieldOffset(const std::string& fieldName) const;

        // ============================================================================
        // COMPARISON
        // ============================================================================

        bool operator==(const ShaderStructInstance& other) const;
        bool operator!=(const ShaderStructInstance& other) const { return !(*this == other); }

        // ============================================================================
        // CompositeTypeInstance interface
        // ============================================================================

        std::shared_ptr<const CompositeTypeDefinition> GetDefinition() const override {
            return definition;
        }

        const std::vector<uint8_t>& GetRawBuffer() const override {
            SyncBufferIfDirty();
            return buffer;
        }

        bool WriteToBuffer(void* dst) const override;
        bool ReadFromBuffer(const void* src) override;
        std::shared_ptr<CompositeTypeInstance> Clone() const override;
        json ToJson() const override;
        bool FromJson(const json& j) override;
        bool IsStruct() const override { return true; }
        bool IsArray() const override { return false; }
    };

    // ============================================================================
    // NON-MEMBER FUNCTIONS
    // ============================================================================

    // Swap specialization
    inline void swap(ShaderStructInstance& lhs, ShaderStructInstance& rhs) noexcept {
        lhs.swap(rhs);
    }

    // ============================================================================
    // FACTORY FUNCTIONS
    // ============================================================================

    // Create struct from initializer list
    inline std::shared_ptr<ShaderStructInstance> MakeShaderStruct(
        std::shared_ptr<const ShaderStructDefinition> def,
        std::initializer_list<std::pair<const std::string, BufferValue>> fields)
    {
        return std::make_shared<ShaderStructInstance>(def, fields);
    }

} // namespace ShaderLib