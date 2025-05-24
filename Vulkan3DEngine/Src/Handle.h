#pragma once
#include <cstdint>
#include <functional>

// Generyczny szablon dla wszystkich typów uchwytów
template<typename Tag, typename IdType = uint32_t>
struct Handle {
    IdType id;

    // Konstruktory
    constexpr Handle() : id(0) {}
    constexpr explicit Handle(IdType id) : id(id) {}

    // Operatory porównania
    bool operator==(const Handle&) const = default;
    bool operator!=(const Handle&) const = default;

    // Sprawdzanie ważności uchwytu
    constexpr bool isValid() const { return id != 0; }
    explicit operator bool() const { return id != 0; }
};

// Makro do szybkiego definiowania nowych typów uchwytów
#define DEFINE_HANDLE_TYPE(TypeName, IdType) \
    struct TypeName##Tag {}; \
    using TypeName = Handle<TypeName##Tag, IdType>; \
    \
    namespace std { \
        template <> \
        struct hash<TypeName> { \
            size_t operator()(const TypeName& handle) const { \
                return hash<IdType>()(handle.id); \
            } \
        }; \
    }

// Definicje poszczególnych typów uchwytów
DEFINE_HANDLE_TYPE(VramHandle, uint64_t)
DEFINE_HANDLE_TYPE(UniformBufferHandle, uint32_t)
DEFINE_HANDLE_TYPE(ShaderModuleHandle, uint32_t)
DEFINE_HANDLE_TYPE(ShaderHandle, uint32_t)
DEFINE_HANDLE_TYPE(PipelineHandle, uint32_t)
DEFINE_HANDLE_TYPE(PipelineLayoutHandle, uint32_t)
DEFINE_HANDLE_TYPE(MeshHandle, uint32_t)
DEFINE_HANDLE_TYPE(MaterialHandle, uint32_t)
DEFINE_HANDLE_TYPE(TextureHandle, uint32_t)
DEFINE_HANDLE_TYPE(DescriptorSetHandle, uint32_t)
DEFINE_HANDLE_TYPE(SamplerHandle, uint32_t)