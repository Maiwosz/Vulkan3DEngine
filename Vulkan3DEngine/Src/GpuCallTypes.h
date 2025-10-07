#pragma once
#include <typeindex>
#include <unordered_set>

/**
 * Type traits for GPU call filtering
 * Each GpuCall subclass can declare what it is
 */
class GpuCallTypeInfo {
public:
    template<typename T>
    static std::type_index getTypeIndex() {
        return std::type_index(typeid(T));
    }
};

/**
 * Mixin for GpuCalls to declare their type
 */
template<typename Derived>
class TypedGpuCall : public GpuCall {
public:
    static std::type_index staticTypeIndex() {
        return std::type_index(typeid(Derived));
    }

    std::type_index getTypeIndex() const override {
        return staticTypeIndex();
    }
};