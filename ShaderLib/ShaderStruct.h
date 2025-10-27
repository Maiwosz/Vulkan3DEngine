#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include "BufferIO.h"

namespace ShaderLib {

    // ============================================================================
    // SHADER STRUCT - Struktura shadera
    // ============================================================================

    class ShaderStruct : public CompositeType {
    public:
        // Field definition
        struct Field {
            std::string name;
            BaseType baseType;
            uint32_t offset;
            uint32_t size;
            std::shared_ptr<CompositeType> composite; // Dla zagnieżdżonych struct/array

            // Constructor dla typu bazowego
            Field(const std::string& n, BaseType t, uint32_t off, uint32_t sz)
                : name(n), baseType(t), offset(off), size(sz) {
            }

            // Constructor dla typu kompozytowego
            Field(const std::string& n, std::shared_ptr<CompositeType> comp, uint32_t off)
                : name(n), baseType(BaseType::Unknown), offset(off),
                size(comp->GetSize()), composite(comp) {
            }

            bool IsComposite() const { return composite != nullptr; }
        };

    private:
        // Type definition (niezmienne)
        std::string typeName;
        std::vector<Field> fields;
        uint32_t size;
        uint32_t alignment;
        LayoutStandard layoutStandard;
        bool finalized;

        // Instance data (zmienne, opcjonalne)
        mutable std::unique_ptr<std::unordered_map<std::string, BufferValue>> fieldValues;
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
        explicit ShaderStruct(const std::string& name, LayoutStandard standard = LayoutStandard::Std140)
            : typeName(name), size(0), alignment(0), layoutStandard(standard), finalized(false) {
        }

        // Copy constructor
        ShaderStruct(const ShaderStruct& other)
            : typeName(other.typeName)
            , fields(other.fields)
            , size(other.size)
            , alignment(other.alignment)
            , layoutStandard(other.layoutStandard)
            , finalized(other.finalized) {

            if (other.fieldValues) {
                fieldValues = std::make_unique<std::unordered_map<std::string, BufferValue>>(*other.fieldValues);
            }
            if (other.buffer) {
                buffer = std::make_unique<std::vector<uint8_t>>(*other.buffer);
            }
        }

        ShaderStruct& operator=(const ShaderStruct& other) {
            if (this != &other) {
                typeName = other.typeName;
                fields = other.fields;
                size = other.size;
                alignment = other.alignment;
                layoutStandard = other.layoutStandard;
                finalized = other.finalized;

                fieldValues = other.fieldValues
                    ? std::make_unique<std::unordered_map<std::string, BufferValue>>(*other.fieldValues)
                    : nullptr;

                buffer = other.buffer
                    ? std::make_unique<std::vector<uint8_t>>(*other.buffer)
                    : nullptr;
            }
            return *this;
        }

        // ========================================================================
        // BUILDING INTERFACE - Definiowanie struktury
        // ========================================================================

        template<typename T>
        ShaderStruct& AddField(const std::string& name) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            if (finalized) {
                throw std::logic_error("Cannot add fields to finalized struct");
            }

            const BaseTypeInfo& info = BaseTypeTraits<T>::GetInfo();
            uint32_t fieldAlignment = info.GetAlignment(layoutStandard);

            size = AlignTo(size, fieldAlignment);
            fields.emplace_back(name, info.type, size, info.size);
            size += info.size;
            alignment = std::max(alignment, fieldAlignment);

            return *this;
        }

        ShaderStruct& AddCompositeField(const std::string& name, std::shared_ptr<CompositeType> composite) {
            if (finalized) {
                throw std::logic_error("Cannot add fields to finalized struct");
            }
            if (!composite) {
                throw std::invalid_argument("Composite type cannot be null");
            }

            uint32_t fieldAlignment = composite->GetAlignment();
            size = AlignTo(size, fieldAlignment);
            fields.emplace_back(name, composite, size);
            size += composite->GetSize();
            alignment = std::max(alignment, fieldAlignment);

            return *this;
        }

        void Finalize() {
            if (!finalized) {
                if (alignment > 0) {
                    size = AlignTo(size, alignment);
                }
                finalized = true;
            }
        }

        // ========================================================================
        // CompositeType INTERFACE
        // ========================================================================

        std::string GetTypeName() const override { return typeName; }
        uint32_t GetSize() const override { return size; }
        uint32_t GetAlignment() const override { return alignment; }
        LayoutStandard GetLayoutStandard() const override { return layoutStandard; }

        bool IsStruct() const override { return true; }
        bool IsArray() const override { return false; }

        // ========================================================================
        // DATA MANAGEMENT
        // ========================================================================

        void InitializeData() override {
            if (!buffer) {
                if (!finalized) {
                    throw std::logic_error("Cannot initialize data before finalizing struct");
                }

                buffer = std::make_unique<std::vector<uint8_t>>(size, 0);
                fieldValues = std::make_unique<std::unordered_map<std::string, BufferValue>>();
                InitializeFieldDefaults();
            }
        }

        bool HasData() const override {
            return buffer != nullptr;
        }

        void ClearData() override {
            fieldValues.reset();
            buffer.reset();
        }

        // ========================================================================
        // FIELD ACCESS - Ustawianie wartości
        // ========================================================================

        template<typename T>
        void SetField(const std::string& fieldName, const T& value) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            InitializeData();

            const Field* field = FindField(fieldName);
            if (!field) {
                throw std::out_of_range("Field '" + fieldName + "' not found");
            }

            const BaseType expectedType = GetBaseTypeOf<T>();
            if (field->baseType != expectedType) {
                throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
            }

            (*fieldValues)[fieldName] = value;
            WriteFieldToBuffer(*field, value);
        }

        void SetCompositeField(const std::string& fieldName, std::shared_ptr<CompositeType> value) {
            InitializeData();

            const Field* field = FindField(fieldName);
            if (!field) {
                throw std::out_of_range("Field '" + fieldName + "' not found");
            }
            if (!field->IsComposite()) {
                throw std::invalid_argument("Field '" + fieldName + "' is not a composite type");
            }
            if (!value) {
                throw std::invalid_argument("Composite value cannot be null");
            }
            if (value->GetTypeName() != field->composite->GetTypeName()) {
                throw std::invalid_argument("Composite type mismatch for field '" + fieldName + "'");
            }

            // Konwersja CompositeType na odpowiedni typ dla BufferValue
            (*fieldValues)[fieldName] = ConvertCompositeToBufferValue(value);

            if (value->HasData()) {
                const std::vector<uint8_t>& srcBuffer = value->GetRawBuffer();
                std::memcpy(buffer->data() + field->offset, srcBuffer.data(),
                    std::min(srcBuffer.size(), static_cast<size_t>(field->size)));
            }
        }

        // ========================================================================
        // FIELD ACCESS - Odczytywanie wartości
        // ========================================================================

        template<typename T>
        T GetField(const std::string& fieldName) const {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported");

            if (!fieldValues) {
                throw std::runtime_error("Struct data not initialized");
            }

            auto it = fieldValues->find(fieldName);
            if (it == fieldValues->end()) {
                throw std::out_of_range("Field '" + fieldName + "' not set");
            }

            try {
                return std::get<T>(it->second);
            }
            catch (const std::bad_variant_access&) {
                throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
            }
        }

        std::shared_ptr<CompositeType> GetCompositeField(const std::string& fieldName) const {
            if (!fieldValues) {
                throw std::runtime_error("Struct data not initialized");
            }

            auto it = fieldValues->find(fieldName);
            if (it == fieldValues->end()) {
                throw std::out_of_range("Field '" + fieldName + "' not set");
            }

            BufferValue& value = it->second;

            // Sprawdź czy to ShaderStruct
            if (auto structPtr = std::get_if<std::shared_ptr<ShaderStruct>>(&value)) {
                return std::static_pointer_cast<CompositeType>(*structPtr);
            }

            // Sprawdź czy to ShaderArray
            if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArray>>(&value)) {
                return std::static_pointer_cast<CompositeType>(*arrayPtr);
            }

            throw std::invalid_argument("Field '" + fieldName + "' is not a composite type");
        }

        bool HasField(const std::string& fieldName) const {
            return fieldValues && fieldValues->find(fieldName) != fieldValues->end();
        }

        // ========================================================================
        // BUFFER I/O
        // ========================================================================

        const std::vector<uint8_t>& GetRawBuffer() const override {
            if (!buffer) {
                throw std::runtime_error("Struct data not initialized");
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

            // Odczytaj wszystkie pola z bufora
            for (const auto& field : fields) {
                ReadFieldFromBuffer(field);
            }

            return true;
        }

        // ========================================================================
        // GLSL GENERATION
        // ========================================================================

        std::string GenerateGLSL() const override {
            std::stringstream ss;
            ss << "struct " << typeName << " {\n";

            for (const auto& field : fields) {
                ss << "    ";

                if (field.IsComposite()) {
                    ss << field.composite->GetTypeName();
                }
                else {
                    ss << BaseTypeToString(field.baseType);
                }

                ss << " " << field.name << ";\n";
            }

            ss << "};\n";
            return ss.str();
        }

        // ========================================================================
        // ADDITIONAL GETTERS
        // ========================================================================

        const std::vector<Field>& GetFields() const { return fields; }
        bool IsFinalized() const { return finalized; }

    private:
        const Field* FindField(const std::string& name) const {
            auto it = std::find_if(fields.begin(), fields.end(),
                [&name](const Field& f) { return f.name == name; });
            return (it != fields.end()) ? &(*it) : nullptr;
        }

        void InitializeFieldDefaults() {
            for (const auto& field : fields) {
                if (field.IsComposite()) {
                    // Stwórz kopię typu kompozytowego i zainicjalizuj
                    auto instance = CloneComposite(field.composite);
                    instance->InitializeData();
                    (*fieldValues)[field.name] = ConvertCompositeToBufferValue(instance);
                }
                else {
                    // Odczytaj domyślną wartość z bufora (zerowanego)
                    BufferValue defaultValue = ReadBaseTypeFromBuffer(field.baseType,
                        buffer->data() + field.offset);
                    (*fieldValues)[field.name] = defaultValue;
                }
            }
        }

        template<typename T>
        void WriteFieldToBuffer(const Field& field, const T& value) {
            WriteBaseTypeToBuffer(field.baseType, buffer->data() + field.offset, value);
        }

        void ReadFieldFromBuffer(const Field& field) {
            if (field.IsComposite()) {
                auto instance = CloneComposite(field.composite);
                instance->InitializeData();
                instance->ReadFromBuffer(buffer->data() + field.offset);
                (*fieldValues)[field.name] = ConvertCompositeToBufferValue(instance);
            }
            else {
                BufferValue value = ReadBaseTypeFromBuffer(field.baseType,
                    buffer->data() + field.offset);
                (*fieldValues)[field.name] = value;
            }
        }
    };

} // namespace ShaderLib