#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include "BufferIO.h"

namespace ShaderLib {

    // ============================================================================
    // SHADER ARRAY - Tablica shadera
    // ============================================================================

    class ShaderArray : public CompositeType {
    private:
        // Type definition (niezmienne)
        BaseType elementBaseType;
        std::shared_ptr<CompositeType> elementComposite;
        uint32_t arrayCount;
        uint32_t elementSize;
        uint32_t elementStride;
        uint32_t totalSize;
        uint32_t alignment;
        LayoutStandard layoutStandard;

        // Instance data (zmienne, opcjonalne)
        mutable std::unique_ptr<std::vector<BufferValue>> elements;
        mutable std::unique_ptr<std::vector<uint8_t>> buffer;

        // Helper do konwersji CompositeType na odpowiedni typ
        BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeType> composite) const {
            if (!composite) {
                throw std::invalid_argument("Composite cannot be null");
            }

            if (composite->IsStruct()) {
                return std::static_pointer_cast<ShaderStruct>(composite);
            }
            else if (composite->IsArray()) {
                return std::static_pointer_cast<ShaderArray>(composite);
            }

            throw std::invalid_argument("Unknown composite type");
        }

    public:
        // Constructor dla typów bazowych
        ShaderArray(BaseType elemType, uint32_t count, LayoutStandard standard = LayoutStandard::Std140)
            : elementBaseType(elemType)
            , elementComposite(nullptr)
            , arrayCount(count)
            , layoutStandard(standard) {

            if (count == 0) {
                throw std::invalid_argument("Array count must be greater than 0");
            }

            const BaseTypeInfo& info = GetBaseTypeInfo(elementBaseType);
            if (!info.IsValid()) {
                throw std::invalid_argument("Invalid base type for array");
            }

            elementSize = info.size;
            uint32_t baseAlignment = info.GetAlignment(layoutStandard);
            alignment = GetArrayElementAlignment(baseAlignment, layoutStandard);
            elementStride = AlignTo(elementSize, alignment);
            totalSize = elementStride * arrayCount;
        }

        // Constructor dla typów kompozytowych
        ShaderArray(std::shared_ptr<CompositeType> elemType, uint32_t count, LayoutStandard standard = LayoutStandard::Std140)
            : elementBaseType(BaseType::Unknown)
            , elementComposite(elemType)
            , arrayCount(count)
            , layoutStandard(standard) {

            if (count == 0) {
                throw std::invalid_argument("Array count must be greater than 0");
            }
            if (!elementComposite) {
                throw std::invalid_argument("Composite element type cannot be null");
            }

            elementSize = elementComposite->GetSize();
            alignment = GetArrayElementAlignment(elementComposite->GetAlignment(), layoutStandard);
            elementStride = AlignTo(elementSize, alignment);
            totalSize = elementStride * arrayCount;
        }

        // Copy constructor
        ShaderArray(const ShaderArray& other)
            : elementBaseType(other.elementBaseType)
            , elementComposite(other.elementComposite)
            , arrayCount(other.arrayCount)
            , elementSize(other.elementSize)
            , elementStride(other.elementStride)
            , totalSize(other.totalSize)
            , alignment(other.alignment)
            , layoutStandard(other.layoutStandard) {

            if (other.elements) {
                elements = std::make_unique<std::vector<BufferValue>>(*other.elements);
            }
            if (other.buffer) {
                buffer = std::make_unique<std::vector<uint8_t>>(*other.buffer);
            }
        }

        ShaderArray& operator=(const ShaderArray& other) {
            if (this != &other) {
                elementBaseType = other.elementBaseType;
                elementComposite = other.elementComposite;
                arrayCount = other.arrayCount;
                elementSize = other.elementSize;
                elementStride = other.elementStride;
                totalSize = other.totalSize;
                alignment = other.alignment;
                layoutStandard = other.layoutStandard;

                elements = other.elements
                    ? std::make_unique<std::vector<BufferValue>>(*other.elements)
                    : nullptr;

                buffer = other.buffer
                    ? std::make_unique<std::vector<uint8_t>>(*other.buffer)
                    : nullptr;
            }
            return *this;
        }

        // ========================================================================
        // CompositeType INTERFACE
        // ========================================================================

        std::string GetTypeName() const override {
            std::stringstream ss;
            if (IsCompositeElement()) {
                ss << elementComposite->GetTypeName();
            }
            else {
                ss << BaseTypeToString(elementBaseType);
            }
            ss << "[" << arrayCount << "]";
            return ss.str();
        }

        uint32_t GetSize() const override { return totalSize; }
        uint32_t GetAlignment() const override { return alignment; }
        LayoutStandard GetLayoutStandard() const override { return layoutStandard; }

        bool IsStruct() const override { return false; }
        bool IsArray() const override { return true; }

        // ========================================================================
        // DATA MANAGEMENT
        // ========================================================================

        void InitializeData() override {
            if (!buffer) {
                buffer = std::make_unique<std::vector<uint8_t>>(totalSize, 0);
                elements = std::make_unique<std::vector<BufferValue>>(arrayCount);
                InitializeElementDefaults();
            }
        }

        bool HasData() const override {
            return buffer != nullptr;
        }

        void ClearData() override {
            elements.reset();
            buffer.reset();
        }

        // ========================================================================
        // ELEMENT ACCESS - Ustawianie wartości
        // ========================================================================

        template<typename T>
        void SetElement(uint32_t index, const T& value) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            InitializeData();
            ValidateIndex(index);

            const BaseType expectedType = GetBaseTypeOf<T>();
            if (elementBaseType != expectedType) {
                throw std::invalid_argument("Type mismatch: expected " +
                    std::string(BaseTypeToString(elementBaseType)));
            }

            (*elements)[index] = value;
            WriteElementToBuffer(index, value);
        }

        void SetCompositeElement(uint32_t index, std::shared_ptr<CompositeType> value) {
            InitializeData();
            ValidateIndex(index);

            if (!IsCompositeElement()) {
                throw std::invalid_argument("Array element type is not composite");
            }
            if (!value) {
                throw std::invalid_argument("Composite value cannot be null");
            }
            if (value->GetTypeName() != elementComposite->GetTypeName()) {
                throw std::invalid_argument("Composite type mismatch");
            }

            // Konwersja CompositeType na odpowiedni typ dla BufferValue
            (*elements)[index] = ConvertCompositeToBufferValue(value);

            if (value->HasData()) {
                uint32_t offset = index * elementStride;
                const std::vector<uint8_t>& srcBuffer = value->GetRawBuffer();
                std::memcpy(buffer->data() + offset, srcBuffer.data(),
                    std::min(srcBuffer.size(), static_cast<size_t>(elementSize)));
            }
        }

        // Bulk set dla typów bazowych
        template<typename T>
        void SetAllElements(const std::vector<T>& values) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            if (values.size() != arrayCount) {
                throw std::invalid_argument("Values size mismatch");
            }

            for (uint32_t i = 0; i < arrayCount; ++i) {
                SetElement(i, values[i]);
            }
        }

        // ========================================================================
        // ELEMENT ACCESS - Odczytywanie wartości
        // ========================================================================

        template<typename T>
        T GetElement(uint32_t index) const {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            if (!elements) {
                throw std::runtime_error("Array data not initialized");
            }
            ValidateIndex(index);

            try {
                return std::get<T>((*elements)[index]);
            }
            catch (const std::bad_variant_access&) {
                throw std::invalid_argument("Type mismatch at index " + std::to_string(index));
            }
        }

        std::shared_ptr<CompositeType> GetCompositeElement(uint32_t index) const {
            if (!elements) {
                throw std::runtime_error("Array data not initialized");
            }
            ValidateIndex(index);

            BufferValue& value = (*elements)[index];

            // Sprawdź czy to ShaderStruct
            if (auto structPtr = std::get_if<std::shared_ptr<ShaderStruct>>(&value)) {
                return std::static_pointer_cast<CompositeType>(*structPtr);
            }

            // Sprawdź czy to ShaderArray
            if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArray>>(&value)) {
                return std::static_pointer_cast<CompositeType>(*arrayPtr);
            }

            throw std::invalid_argument("Element at index " + std::to_string(index) +
                " is not composite");
        }

        // Bulk get dla typów bazowych
        template<typename T>
        std::vector<T> GetAllElements() const {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            std::vector<T> result;
            result.reserve(arrayCount);

            for (uint32_t i = 0; i < arrayCount; ++i) {
                result.push_back(GetElement<T>(i));
            }

            return result;
        }

        // ========================================================================
        // BUFFER I/O
        // ========================================================================

        const std::vector<uint8_t>& GetRawBuffer() const override {
            if (!buffer) {
                throw std::runtime_error("Array data not initialized");
            }
            return *buffer;
        }

        bool WriteToBuffer(void* dst) const override {
            if (!buffer || !dst) return false;
            std::memcpy(dst, buffer->data(), buffer->size());
            return true;
        }

        bool ReadFromBuffer(const void* src) override {
            if (!src) return false;

            InitializeData();
            std::memcpy(buffer->data(), src, buffer->size());

            // Odczytaj wszystkie elementy z bufora
            for (uint32_t i = 0; i < arrayCount; ++i) {
                ReadElementFromBuffer(i);
            }

            return true;
        }

        // ========================================================================
        // GLSL GENERATION
        // ========================================================================

        std::string GenerateGLSL() const override {
            // Arrays są częścią deklaracji zmiennej, nie oddzielnego typu
            // Zwracamy tylko informację o typie elementu
            if (IsCompositeElement()) {
                return elementComposite->GenerateGLSL();
            }
            return "";
        }

        // ========================================================================
        // ADDITIONAL GETTERS
        // ========================================================================

        uint32_t GetArrayCount() const { return arrayCount; }
        uint32_t GetElementSize() const { return elementSize; }
        uint32_t GetElementStride() const { return elementStride; }

        bool IsCompositeElement() const { return elementComposite != nullptr; }
        BaseType GetElementBaseType() const { return elementBaseType; }
        std::shared_ptr<CompositeType> GetElementComposite() const { return elementComposite; }

    private:
        void ValidateIndex(uint32_t index) const {
            if (index >= arrayCount) {
                throw std::out_of_range("Array index " + std::to_string(index) +
                    " out of bounds (size: " + std::to_string(arrayCount) + ")");
            }
        }

        void InitializeElementDefaults() {
            if (IsCompositeElement()) {
                // Dla typów kompozytowych - stwórz kopie
                for (uint32_t i = 0; i < arrayCount; ++i) {
                    auto instance = CloneComposite(elementComposite);
                    instance->InitializeData();
                    (*elements)[i] = ConvertCompositeToBufferValue(instance);
                }
            }
            else {
                // Dla typów bazowych - odczytaj z zerowanego bufora
                for (uint32_t i = 0; i < arrayCount; ++i) {
                    uint32_t offset = i * elementStride;
                    BufferValue defaultValue = ReadBaseTypeFromBuffer(elementBaseType,
                        buffer->data() + offset);
                    (*elements)[i] = defaultValue;
                }
            }
        }

        template<typename T>
        void WriteElementToBuffer(uint32_t index, const T& value) {
            uint32_t offset = index * elementStride;
            WriteBaseTypeToBuffer(elementBaseType, buffer->data() + offset, value);
        }

        void ReadElementFromBuffer(uint32_t index) {
            uint32_t offset = index * elementStride;

            if (IsCompositeElement()) {
                auto instance = CloneComposite(elementComposite);
                instance->InitializeData();
                instance->ReadFromBuffer(buffer->data() + offset);
                (*elements)[index] = ConvertCompositeToBufferValue(instance);
            }
            else {
                BufferValue value = ReadBaseTypeFromBuffer(elementBaseType,
                    buffer->data() + offset);
                (*elements)[index] = value;
            }
        }
    };

} // namespace ShaderLib