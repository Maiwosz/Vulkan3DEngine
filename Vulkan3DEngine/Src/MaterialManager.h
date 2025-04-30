#pragma once
#include "Material.h"
#include "ImageSamplerManager.h"
#include <unordered_map>

struct MaterialHandle {
    uint32_t id;
    constexpr explicit MaterialHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const MaterialHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<MaterialHandle> {
        size_t operator()(const MaterialHandle& h) const {
            return hash<uint32_t>()(h.id);
        }
    };
}

class MaterialManager {
public:
    MaterialManager(ImageSamplerManager& samplerManager);

    MaterialHandle cacheMaterial(const std::string& filename, std::unique_ptr<Material> material);
    Material* get(MaterialHandle handle) const;
    void freeResource(MaterialHandle handle);

private:
    SamplerConfig convertSamplerDescription(const AssetLib::SamplerDescription& desc);
    void configureTextureSamplers(Material* material);

    ImageSamplerManager& m_samplerManager;
    std::unordered_map<MaterialHandle, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, MaterialHandle> m_filenameToHandle;
    uint32_t m_nextId = 1;
};