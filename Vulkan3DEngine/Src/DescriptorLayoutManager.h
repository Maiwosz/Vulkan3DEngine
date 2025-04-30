#pragma once
#include "DescriptorLayoutBuilder.h"
#include "LogicalDevice.h"
#include <unordered_map>
#include <vector>
#include <cstdint>

struct DescriptorLayoutHandle {
    uint32_t id;
    constexpr explicit DescriptorLayoutHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const DescriptorLayoutHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<DescriptorLayoutHandle> {
        size_t operator()(const DescriptorLayoutHandle& h) const {
            return hash<uint32_t>()(h.id);
        }
    };
}

class DescriptorLayoutManager {
public:
    DescriptorLayoutManager(const LogicalDevice& device);
    DescriptorLayoutHandle create(
        const DescriptorLayoutBuilder& builder,
        VkShaderStageFlags shaderStages,
        void* pNext = nullptr,
        VkDescriptorSetLayoutCreateFlags flags = 0
    );
    void destroy(DescriptorLayoutHandle handle);
    VkDescriptorSetLayout get(DescriptorLayoutHandle handle) const;
    bool isValid(DescriptorLayoutHandle handle) const;

private:
    const LogicalDevice& m_device;
    std::unordered_map<DescriptorLayoutHandle, VkDescriptorSetLayout> m_layouts;
    uint32_t m_nextHandle = 1;
};