//#pragma once
//#include "BufferObjectDefinition.h"
//#include "ShaderTypes.h"
//#include <vector>
//#include <memory>
//#include <unordered_map>
//#include <json.hpp>
//#include <stdexcept>
//#include <iterator>
//
//using json = nlohmann::json;
//
//namespace ShaderLib {
//
//    // ============================================================================
//    // BUFFER OBJECT INSTANCE - Mutable buffer data with STL-like interface
//    // ============================================================================
//
//    class BufferObjectInstance : public std::enable_shared_from_this<BufferObjectInstance> {
//    private:
//        std::shared_ptr<const BufferObjectDefinition> definition;
//        std::unordered_map<std::string, BufferValue> values;
//        std::vector<uint8_t> buffer;
//        mutable bool bufferDirty = false;
//
//        void InitializeDefaults();
//        void WriteFieldToBuffer(const std::string& fieldName, const BufferValue& value);
//        void ReadFieldFromBuffer(const std::string& fieldName);
//        void SyncBufferIfDirty() const;
//
//    public:
//        // ============================================================================
//        // TYPE ALIASES (STL-like)
//        // ============================================================================
//        using key_type = std::string;
//        using mapped_type = BufferValue;
//        using value_type = std::pair<const std::string, BufferValue>;
//        using size_type = size_t;
//        using iterator = std::unordered_map<std::string, BufferValue>::iterator;
//        using const_iterator = std::unordered_map<std::string, BufferValue>::const_iterator;
//
//        // ============================================================================
//        // CONSTRUCTORS & ASSIGNMENT
//        // ============================================================================
//
//        explicit BufferObjectInstance(std::shared_ptr<const BufferObjectDefinition> def);
//        BufferObjectInstance(const BufferObjectInstance& other);
//        BufferObjectInstance(BufferObjectInstance&& other) noexcept;
//        BufferObjectInstance& operator=(const BufferObjectInstance& other);
//        BufferObjectInstance& operator=(BufferObjectInstance&& other) noexcept;
//
//        // ============================================================================
//        // ELEMENT ACCESS
//        // ============================================================================
//
//        // Access with bounds checking (throws if field doesn't exist)
//        BufferValue& at(const std::string& fieldName);
//        const BufferValue& at(const std::string& fieldName) const;
//
//        // Access without bounds checking (undefined behavior if field doesn't exist)
//        BufferValue& operator[](const std::string& fieldName);
//
//        // Type-safe getters (throw if wrong type)
//        template<typename T>
//        T& Get(const std::string& fieldName) {
//            return std::get<T>(at(fieldName));
//        }
//
//        template<typename T>
//        const T& Get(const std::string& fieldName) const {
//            return std::get<T>(at(fieldName));
//        }
//
//        // Composite access
//        std::shared_ptr<CompositeTypeInstance> GetComposite(const std::string& fieldName);
//        std::shared_ptr<const CompositeTypeInstance> GetComposite(const std::string& fieldName) const;
//
//        // Struct-specific access
//        std::shared_ptr<ShaderStructInstance> GetStruct(const std::string& fieldName);
//        std::shared_ptr<const ShaderStructInstance> GetStruct(const std::string& fieldName) const;
//
//        // Array-specific access
//        std::shared_ptr<ShaderArrayInstance> GetArray(const std::string& fieldName);
//        std::shared_ptr<const ShaderArrayInstance> GetArray(const std::string& fieldName) const;
//
//        // ============================================================================
//        // SETTERS
//        // ============================================================================
//
//        // Set from BufferValue (with type checking)
//        void Set(const std::string& fieldName, const BufferValue& value);
//
//        // Set typed value (compile-time type safety)
//        template<typename T>
//        void Set(const std::string& fieldName, const T& value) {
//            static_assert(IsBaseTypeSupported<T>() ||
//                std::is_same_v<T, std::shared_ptr<ShaderStructInstance>> ||
//                std::is_same_v<T, std::shared_ptr<ShaderArrayInstance>>,
//                "Type not supported");
//
//            const BufferFieldDefinition* field = definition->FindField(fieldName);
//            if (!field) {
//                throw std::out_of_range("Field '" + fieldName + "' not found");
//            }
//
//            if constexpr (IsBaseTypeSupported<T>()) {
//                if (field->baseType != GetBaseTypeOf<T>()) {
//                    throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
//                }
//            }
//            else {
//                if (!field->IsComposite()) {
//                    throw std::invalid_argument("Field '" + fieldName + "' is not composite");
//                }
//            }
//
//            values[fieldName] = value;
//            WriteFieldToBuffer(fieldName, values[fieldName]);
//            bufferDirty = false;
//        }
//
//        // Set composite (with type checking)
//        void SetComposite(const std::string& fieldName,
//            std::shared_ptr<CompositeTypeInstance> value);
//
//        // ============================================================================
//        // ITERATORS
//        // ============================================================================
//
//        iterator begin() noexcept { return values.begin(); }
//        const_iterator begin() const noexcept { return values.begin(); }
//        const_iterator cbegin() const noexcept { return values.cbegin(); }
//
//        iterator end() noexcept { return values.end(); }
//        const_iterator end() const noexcept { return values.end(); }
//        const_iterator cend() const noexcept { return values.cend(); }
//
//        // ============================================================================
//        // CAPACITY
//        // ============================================================================
//
//        bool empty() const noexcept { return values.empty(); }
//        size_type size() const noexcept { return values.size(); }
//        size_type max_size() const noexcept { return values.max_size(); }
//
//        // ============================================================================
//        // LOOKUP
//        // ============================================================================
//
//        size_type count(const std::string& fieldName) const {
//            return values.count(fieldName);
//        }
//
//        iterator find(const std::string& fieldName) {
//            return values.find(fieldName);
//        }
//
//        const_iterator find(const std::string& fieldName) const {
//            return values.find(fieldName);
//        }
//
//        bool contains(const std::string& fieldName) const {
//            return values.find(fieldName) != values.end();
//        }
//
//        // ============================================================================
//        // OPERATIONS
//        // ============================================================================
//
//        // Clear all values (reset to defaults)
//        void clear();
//
//        // Swap contents
//        void swap(BufferObjectInstance& other) noexcept;
//
//        // ============================================================================
//        // BUFFER MANAGEMENT
//        // ============================================================================
//
//        // Get raw buffer (syncs if dirty)
//        const std::vector<uint8_t>& GetRawBuffer() const {
//            SyncBufferIfDirty();
//            return buffer;
//        }
//
//        // Write entire buffer to destination
//        bool WriteToBuffer(void* dst) const;
//
//        // Read entire buffer from source
//        bool ReadFromBuffer(const void* src);
//
//        // Get buffer size
//        size_t GetBufferSize() const { return buffer.size(); }
//
//        // ============================================================================
//        // DEFINITION ACCESS
//        // ============================================================================
//
//        std::shared_ptr<const BufferObjectDefinition> GetDefinition() const {
//            return definition;
//        }
//
//        const std::string& GetName() const {
//            return definition->GetName();
//        }
//
//        BufferType GetBufferType() const {
//            return definition->GetBufferType();
//        }
//
//        LayoutStandard GetLayoutStandard() const {
//            return definition->GetLayoutStandard();
//        }
//
//        // ============================================================================
//        // TYPE CHECKING
//        // ============================================================================
//
//        bool IsUniformBuffer() const {
//            return definition->IsUniformBuffer();
//        }
//
//        bool IsStorageBuffer() const {
//            return definition->IsStorageBuffer();
//        }
//
//        // Check if field exists and is of specific type
//        bool HasField(const std::string& fieldName) const;
//        bool IsBaseField(const std::string& fieldName) const;
//        bool IsCompositeField(const std::string& fieldName) const;
//        bool IsStructField(const std::string& fieldName) const;
//        bool IsArrayField(const std::string& fieldName) const;
//
//        // ============================================================================
//        // CLONING
//        // ============================================================================
//
//        std::shared_ptr<BufferObjectInstance> Clone() const;
//
//        // ============================================================================
//        // COMPARISON
//        // ============================================================================
//
//        bool operator==(const BufferObjectInstance& other) const;
//        bool operator!=(const BufferObjectInstance& other) const {
//            return !(*this == other);
//        }
//
//        // ============================================================================
//        // VALIDATION
//        // ============================================================================
//
//        bool Validate() const;
//    };
//
//    // ============================================================================
//    // NON-MEMBER FUNCTIONS
//    // ============================================================================
//
//    inline void swap(BufferObjectInstance& lhs, BufferObjectInstance& rhs) noexcept {
//        lhs.swap(rhs);
//    }
//
//} // namespace ShaderLib