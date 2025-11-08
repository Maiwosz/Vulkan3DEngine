#include "pch.h"
#include "MaterialSerializer.h"
#include "Serialization.h"
#include <algorithm>
#include <cstring>

namespace AssetLib {

    // ============================================================================
    // MATERIAL DEFINITION HELPERS
    // ============================================================================

    bool MaterialDefinition::Validate() const {
        if (shaderName.empty()) return false;

        // Check for duplicate sampler names
        for (size_t i = 0; i < samplers.size(); ++i) {
            if (samplers[i].name.empty()) return false;
            if (samplers[i].texturePath.empty()) return false;

            for (size_t j = i + 1; j < samplers.size(); ++j) {
                if (samplers[i].name == samplers[j].name) return false;
            }
        }

        return true;
    }

    const SamplerDescription* MaterialDefinition::FindSampler(const std::string& name) const {
        auto it = std::find_if(samplers.begin(), samplers.end(),
            [&](const SamplerDescription& s) { return s.name == name; });
        return it != samplers.end() ? &(*it) : nullptr;
    }

    SamplerDescription* MaterialDefinition::FindSampler(const std::string& name) {
        auto it = std::find_if(samplers.begin(), samplers.end(),
            [&](const SamplerDescription& s) { return s.name == name; });
        return it != samplers.end() ? &(*it) : nullptr;
    }

    std::vector<std::string> MaterialDefinition::GetTextureDependencies() const {
        std::vector<std::string> deps;
        deps.reserve(samplers.size());
        for (const auto& sampler : samplers) {
            deps.push_back(sampler.texturePath);
        }
        return deps;
    }

    void MaterialDefinition::NormalizeSamplerBindings() {
        uint32_t binding = ShaderLib::SAMPLERS_START_BINDING;
        for (auto& sampler : samplers) {
            sampler.binding = binding++;
        }
    }

} // namespace AssetLib
