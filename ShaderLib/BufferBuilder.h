#pragma once
#include "ShaderTypes.h"
#include "ShaderLib.h"
#include <string>
#include <vector>
#include <sstream>

namespace ShaderLib {

    class BufferBuilder {
    private:
        std::string name;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        BufferAccessMode defaultAccessMode;
        std::vector<BufferVariable> variables;
        bool useInstanceName = true;

    public:
        BufferBuilder(const std::string& bufferName)
            : name(bufferName),
            bufferType(BufferType::Uniform),
            layoutStandard(LayoutStandard::Std140),
            defaultAccessMode(BufferAccessMode::ReadOnly) {
        }

        BufferBuilder(const std::string& bufferName,
            BufferType type,
            BufferAccessMode defaultAccess = BufferAccessMode::ReadWrite,
            LayoutStandard standard = LayoutStandard::Std140)
            : name(bufferName),
            bufferType(type),
            layoutStandard(standard),
            defaultAccessMode(defaultAccess) {

            if (type == BufferType::Uniform) {
                defaultAccessMode = BufferAccessMode::ReadOnly;
            }

            if (type == BufferType::Storage && standard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
        }

        BufferBuilder& SetBufferType(BufferType type) {
            bufferType = type;
            if (type == BufferType::Uniform) {
                defaultAccessMode = BufferAccessMode::ReadOnly;
                if (layoutStandard == LayoutStandard::Std430) {
                    layoutStandard = LayoutStandard::Std140;
                }
            }
            else if (type == BufferType::Storage && layoutStandard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
            return *this;
        }

        BufferBuilder& SetDefaultAccessMode(BufferAccessMode mode) {
            if (bufferType == BufferType::Uniform && mode != BufferAccessMode::ReadOnly) {
                throw std::invalid_argument("Uniform buffers must be ReadOnly");
            }
            defaultAccessMode = mode;
            return *this;
        }

        BufferBuilder& SetLayoutStandard(LayoutStandard standard) {
            layoutStandard = standard;
            return *this;
        }

        BufferBuilder& SetUseInstanceName(bool use) {
            useInstanceName = use;
            return *this;
        }

        // ========================================================================
        // BASE TYPE FIELDS - AddField accepts BaseType
        // ========================================================================

        // Add field by BaseType with default access mode
        BufferBuilder& AddField(const std::string& fieldName, BaseType type) {
            return AddField(fieldName, type, defaultAccessMode);
        }

        // Add field by BaseType with explicit access mode
        BufferBuilder& AddField(const std::string& fieldName, BaseType type,
            BufferAccessMode accessMode) {
            if (type == BaseType::Unknown) {
                throw std::invalid_argument("BaseType::Unknown is not valid");
            }
            if (type == BaseType::Struct || type == BaseType::Array) {
                throw std::invalid_argument(
                    "Use AddCompositeField for structs/arrays"
                );
            }

            ValidateAccessMode(accessMode);

            const BaseTypeInfo& info = GetBaseTypeInfo(type);
            uint32_t offset = AlignTo(GetCurrentSize(), info.GetAlignment(layoutStandard));

            variables.emplace_back(fieldName, type, info.size, offset, accessMode);
            return *this;
        }

        // ========================================================================
        // TYPED FIELDS - Compile-time type safety
        // ========================================================================

        // Add basic type field with default access mode (compile-time type safety)
        template<typename T>
        BufferBuilder& AddField(const std::string& fieldName) {
            return AddField(fieldName, GetBaseTypeOf<T>(), defaultAccessMode);
        }

        // Add basic type field with explicit access mode (compile-time type safety)
        template<typename T>
        BufferBuilder& AddField(const std::string& fieldName, BufferAccessMode accessMode) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported in buffers");
            return AddField(fieldName, GetBaseTypeOf<T>(), accessMode);
        }

        // ========================================================================
        // COMPOSITE FIELDS - Now accepts CompositeTypeDefinition
        // ========================================================================

        // Add composite field (struct/array) with default access mode
        BufferBuilder& AddCompositeField(const std::string& fieldName,
            std::shared_ptr<const CompositeTypeDefinition> compositeDefinition) {
            return AddCompositeField(fieldName, compositeDefinition, defaultAccessMode);
        }

        // Add composite field (struct/array) with explicit access mode
        BufferBuilder& AddCompositeField(const std::string& fieldName,
            std::shared_ptr<const CompositeTypeDefinition> compositeDefinition,
            BufferAccessMode accessMode) {
            if (!compositeDefinition) {
                throw std::invalid_argument("Composite type definition cannot be null");
            }

            ValidateAccessMode(accessMode);

            uint32_t fieldAlignment = compositeDefinition->GetAlignment();
            uint32_t offset = AlignTo(GetCurrentSize(), fieldAlignment);

            // BufferVariable now stores CompositeTypeDefinition (not Instance)
            variables.emplace_back(fieldName, compositeDefinition, offset, accessMode);
            return *this;
        }

        // ========================================================================
        // BUILD METHOD
        // ========================================================================

        BufferObject Build() {
            BufferObject buffer;
            buffer.name = name;
            buffer.bufferType = bufferType;
            buffer.layoutStandard = layoutStandard;
            buffer.accessMode = ComputeBufferAccessMode();
            buffer.variables = variables;
            buffer.useInstanceName = useInstanceName;

            uint32_t finalAlignment = (bufferType == BufferType::Uniform) ? 16 : 4;
            buffer.size = AlignTo(GetCurrentSize(), finalAlignment);

            return buffer;
        }

    private:
        uint32_t GetCurrentSize() const {
            if (variables.empty()) return 0;
            const auto& last = variables.back();
            return last.offset + last.size;
        }

        void ValidateAccessMode(BufferAccessMode mode) const {
            if (bufferType == BufferType::Uniform && mode != BufferAccessMode::ReadOnly) {
                throw std::invalid_argument(
                    "Variables in Uniform buffers must be ReadOnly"
                );
            }
        }

        BufferAccessMode ComputeBufferAccessMode() const {
            if (bufferType == BufferType::Uniform) {
                return BufferAccessMode::ReadOnly;
            }

            bool hasReadOnly = false;
            bool hasWriteOnly = false;
            bool hasReadWrite = false;

            for (const auto& var : variables) {
                if (var.IsReadOnly()) hasReadOnly = true;
                if (var.IsWriteOnly()) hasWriteOnly = true;
                if (var.IsReadWrite()) hasReadWrite = true;
            }

            if (hasReadWrite || (hasReadOnly && hasWriteOnly)) {
                return BufferAccessMode::ReadWrite;
            }

            if (hasReadOnly && !hasWriteOnly) {
                return BufferAccessMode::ReadOnly;
            }

            if (hasWriteOnly && !hasReadOnly) {
                return BufferAccessMode::WriteOnly;
            }

            return BufferAccessMode::ReadWrite;
        }
    };

} // namespace ShaderLib