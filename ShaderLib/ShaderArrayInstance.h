#pragma once
#include "ShaderTypes.h"
#include "ShaderArrayDefinition.h"
#include <string>
#include <vector>
#include <memory>
#include <json.hpp>
#include <stdexcept>
#include <iterator>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // SHADER ARRAY INSTANCE - Mutable instance data with STL-like interface
    // ============================================================================

    class ShaderArrayInstance : public CompositeTypeInstance,
        public std::enable_shared_from_this<ShaderArrayInstance> {
    private:
        std::shared_ptr<const ShaderArrayDefinition> definition;
        std::vector<BufferValue> elements;
        std::vector<uint8_t> buffer;
        mutable bool bufferDirty = false;

        void InitializeElementDefaults();
        void WriteElementToBuffer(uint32_t index, const BufferValue& value);
        void ReadElementFromBuffer(uint32_t index);
        BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance) const;
        void SyncBufferIfDirty() const;

    public:
        // ============================================================================
        // TYPE ALIASES (STL-like)
        // ============================================================================
        using value_type = BufferValue;
        using size_type = uint32_t;
        using difference_type = int32_t;
        using reference = BufferValue&;
        using const_reference = const BufferValue&;
        using pointer = BufferValue*;
        using const_pointer = const BufferValue*;
        using iterator = std::vector<BufferValue>::iterator;
        using const_iterator = std::vector<BufferValue>::const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // ============================================================================
        // CONSTRUCTORS & ASSIGNMENT
        // ============================================================================
        explicit ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def);
        ShaderArrayInstance(const ShaderArrayInstance& other);
        ShaderArrayInstance(ShaderArrayInstance&& other) noexcept;
        ShaderArrayInstance& operator=(const ShaderArrayInstance& other);
        ShaderArrayInstance& operator=(ShaderArrayInstance&& other) noexcept;

        // Construct from vector of values
        ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def,
            const std::vector<BufferValue>& values);

        // Construct from initializer list
        ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def,
            std::initializer_list<BufferValue> values);

        // ============================================================================
        // ELEMENT ACCESS
        // ============================================================================

        // Array-like access with bounds checking
        BufferValue& at(size_type index);
        const BufferValue& at(size_type index) const;

        // Array-like access without bounds checking (fast)
        BufferValue& operator[](size_type index);
        const BufferValue& operator[](size_type index) const;

        // Front and back access
        BufferValue& front();
        const BufferValue& front() const;
        BufferValue& back();
        const BufferValue& back() const;

        // Direct buffer access
        BufferValue* data() noexcept { return elements.data(); }
        const BufferValue* data() const noexcept { return elements.data(); }

        // ============================================================================
        // COMPOSITE ELEMENT ACCESS (for struct/array elements)
        // ============================================================================

        // Get composite element (returns nullptr if not composite)
        std::shared_ptr<CompositeTypeInstance> GetComposite(size_type index);
        std::shared_ptr<const CompositeTypeInstance> GetComposite(size_type index) const;

        // Set composite element
        void SetComposite(size_type index, std::shared_ptr<CompositeTypeInstance> value);

        // Type-safe getters (throw if wrong type)
        template<typename T>
        T& Get(size_type index) {
            return std::get<T>(at(index));
        }

        template<typename T>
        const T& Get(size_type index) const {
            return std::get<T>(at(index));
        }

        // ============================================================================
        // ITERATORS
        // ============================================================================

        iterator begin() noexcept { return elements.begin(); }
        const_iterator begin() const noexcept { return elements.begin(); }
        const_iterator cbegin() const noexcept { return elements.cbegin(); }

        iterator end() noexcept { return elements.end(); }
        const_iterator end() const noexcept { return elements.end(); }
        const_iterator cend() const noexcept { return elements.cend(); }

        reverse_iterator rbegin() noexcept { return elements.rbegin(); }
        const_reverse_iterator rbegin() const noexcept { return elements.rbegin(); }
        const_reverse_iterator crbegin() const noexcept { return elements.crbegin(); }

        reverse_iterator rend() noexcept { return elements.rend(); }
        const_reverse_iterator rend() const noexcept { return elements.rend(); }
        const_reverse_iterator crend() const noexcept { return elements.crend(); }

        // ============================================================================
        // CAPACITY
        // ============================================================================

        bool empty() const noexcept { return elements.empty(); }
        size_type size() const noexcept { return static_cast<size_type>(elements.size()); }
        size_type max_size() const noexcept { return static_cast<size_type>(elements.max_size()); }

        // ============================================================================
        // OPERATIONS
        // ============================================================================

        // Fill all elements with value
        void fill(const BufferValue& value);

        // Fill with default value
        void clear();

        // Swap contents
        void swap(ShaderArrayInstance& other) noexcept;

        // ============================================================================
        // CONVERSION TO/FROM VECTOR
        // ============================================================================

        // Convert to vector (copy)
        std::vector<BufferValue> ToVector() const;

        // Convert to vector of specific type (throws if type mismatch)
        template<typename T>
        std::vector<T> ToVectorOf() const {
            std::vector<T> result;
            result.reserve(size());
            for (const auto& elem : elements) {
                result.push_back(std::get<T>(elem));
            }
            return result;
        }

        // Assign from vector (size must match)
        void FromVector(const std::vector<BufferValue>& values);

        // Assign from vector of specific type
        template<typename T>
        void FromVectorOf(const std::vector<T>& values) {
            if (values.size() != size()) {
                throw std::invalid_argument("Vector size must match array size");
            }

            for (size_type i = 0; i < size(); ++i) {
                at(i) = values[i];
                WriteElementToBuffer(i, at(i));
            }
        }

        // ============================================================================
        // COMPARISON
        // ============================================================================

        bool operator==(const ShaderArrayInstance& other) const;
        bool operator!=(const ShaderArrayInstance& other) const { return !(*this == other); }

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
        bool IsStruct() const override { return false; }
        bool IsArray() const override { return true; }

        // ============================================================================
        // CONVENIENCE GETTERS
        // ============================================================================

        std::shared_ptr<const ShaderArrayDefinition> GetArrayDefinition() const {
            return definition;
        }
    };

    // ============================================================================
    // NON-MEMBER FUNCTIONS
    // ============================================================================

    // Swap specialization
    inline void swap(ShaderArrayInstance& lhs, ShaderArrayInstance& rhs) noexcept {
        lhs.swap(rhs);
    }

    // ============================================================================
    // FACTORY FUNCTIONS
    // ============================================================================

    // Create array from vector
    template<typename T>
    std::shared_ptr<ShaderArrayInstance> MakeShaderArray(
        BaseType elementType,
        const std::vector<T>& values,
        LayoutStandard standard = LayoutStandard::Std140)
    {
        auto definition = std::make_shared<ShaderArrayDefinition>(
            elementType,
            static_cast<uint32_t>(values.size()),
            standard
        );

        auto instance = std::make_shared<ShaderArrayInstance>(definition);
        instance->FromVectorOf(values);
        return instance;
    }

    // Create array from initializer list
    template<typename T>
    std::shared_ptr<ShaderArrayInstance> MakeShaderArray(
        BaseType elementType,
        std::initializer_list<T> values,
        LayoutStandard standard = LayoutStandard::Std140)
    {
        return MakeShaderArray(elementType, std::vector<T>(values), standard);
    }

} // namespace ShaderLib