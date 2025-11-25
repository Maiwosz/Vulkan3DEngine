#include "pch.h"
#include "ShaderLib.h"
#include <algorithm>

namespace ShaderLib {

    // ============================================================================
    // DescriptorSet Implementation
    // ============================================================================

    const DescriptorSlot* DescriptorSet::FindSlot(uint32_t binding) const {
        auto it = std::find_if(slots.begin(), slots.end(),
            [binding](const DescriptorSlot& slot) {
                return slot.binding == binding;
            });
        return it != slots.end() ? &(*it) : nullptr;
    }

    DescriptorSlot* DescriptorSet::FindSlot(uint32_t binding) {
        auto it = std::find_if(slots.begin(), slots.end(),
            [binding](const DescriptorSlot& slot) {
                return slot.binding == binding;
            });
        return it != slots.end() ? &(*it) : nullptr;
    }

    const DescriptorSlot* DescriptorSet::FindSlot(const std::string& name) const {
        auto it = std::find_if(slots.begin(), slots.end(),
            [&name](const DescriptorSlot& slot) {
                return slot.name == name;
            });
        return it != slots.end() ? &(*it) : nullptr;
    }

    std::shared_ptr<const BufferObjectDefinition> DescriptorSet::GetBuffer(const std::string& name) const {
        auto it = buffers.find(name);
        return it != buffers.end() ? it->second : nullptr;
    }

    std::shared_ptr<BufferObjectDefinition> DescriptorSet::GetBuffer(const std::string& name) {
        auto it = buffers.find(name);
        return it != buffers.end() ? it->second : nullptr;
    }

    std::shared_ptr<const BufferObjectDefinition> DescriptorSet::GetBufferByBinding(uint32_t binding) const {
        if (const auto* slot = FindSlot(binding)) {
            if (slot->IsBuffer()) {
                return GetBuffer(slot->name);
            }
        }
        return nullptr;
    }

    std::shared_ptr<BufferObjectDefinition> DescriptorSet::GetBufferByBinding(uint32_t binding) {
        if (auto* slot = FindSlot(binding)) {
            if (slot->IsBuffer()) {
                return GetBuffer(slot->name);
            }
        }
        return nullptr;
    }

    bool DescriptorSet::HasBindingConflict() const {
        std::set<uint32_t> bindings;
        for (const auto& slot : slots) {
            if (!bindings.insert(slot.binding).second) {
                return true; // Duplicate binding
            }
        }
        return false;
    }

    bool DescriptorSet::ValidateBuffers() const {
        for (const auto& slot : slots) {
            if (slot.IsBuffer() && buffers.find(slot.name) == buffers.end()) {
                return false; // Buffer slot without buffer data
            }
        }
        return true;
    }

    std::vector<std::shared_ptr<const BufferObjectDefinition>> DescriptorSet::GetAllBuffers() const {
        std::vector<std::shared_ptr<const BufferObjectDefinition>> result;
        for (const auto& slot : slots) {
            if (slot.IsBuffer()) {
                if (auto buf = GetBuffer(slot.name)) {
                    result.push_back(buf);
                }
            }
        }
        return result;
    }

    std::vector<const DescriptorSlot*> DescriptorSet::GetAllSamplers() const {
        std::vector<const DescriptorSlot*> result;
        for (const auto& slot : slots) {
            if (slot.IsSampler()) {
                result.push_back(&slot);
            }
        }
        return result;
    }

    std::vector<const DescriptorSlot*> DescriptorSet::GetSlotsByType(DescriptorType type) const {
        std::vector<const DescriptorSlot*> result;
        for (const auto& slot : slots) {
            if (slot.type == type) {
                result.push_back(&slot);
            }
        }
        return result;
    }

    std::string DescriptorSet::GenerateGLSL() const {
        std::stringstream ss;

        // FAZA 1: Zbierz wszystkie unikalne definicje zagnieżdżonych struktur
        std::set<std::string> generatedStructs;
        std::set<std::string> processedStructNames;

        for (const auto& [name, buffer] : buffers) {
            if (!buffer) {
                continue;
            }

            // Zbierz zagnieżdżone struktury z tego bufora
            buffer->CollectNestedStructDefinitions(generatedStructs, processedStructNames);
        }

        // FAZA 2: Wygeneruj wszystkie zebrane definicje struktur
        // (std::set gwarantuje unikalność i deterministyczną kolejność)
        for (const auto& structDef : generatedStructs) {
            ss << structDef << "\n\n";
        }

        // FAZA 3: Wygeneruj deklaracje buforów i samplerów w kolejności bindingów
        std::vector<const DescriptorSlot*> sortedSlots;
        sortedSlots.reserve(slots.size());

        for (const auto& slot : slots) {
            sortedSlots.push_back(&slot);
        }

        // Sortuj sloty po binding number dla deterministycznej kolejności
        std::sort(sortedSlots.begin(), sortedSlots.end(),
            [](const DescriptorSlot* a, const DescriptorSlot* b) {
                return a->binding < b->binding;
            });

        for (const auto* slot : sortedSlots) {
            if (slot->IsBuffer()) {
                // Wygeneruj deklarację bufora (bez duplikowania struktury)
                auto buffer = GetBuffer(slot->name);
                if (buffer) {
                    ss << buffer->GenerateBufferGLSL(setNumber, slot->binding) << "\n";
                }
            }
            else {
                // Wygeneruj deklarację samplera/image
                ss << GenerateSamplerGLSL(*slot, setNumber) << "\n";
            }
        }

        return ss.str();
    }

    std::string DescriptorSet::GenerateSamplerGLSL(const DescriptorSlot& slot, uint32_t set) const {
        std::stringstream ss;

        const DescriptorTypeInfo& typeInfo = GetDescriptorTypeInfo(slot.type);

        ss << "layout(set = " << set << ", binding = " << slot.binding << ") ";

        // Add format qualifier for images if needed
        if (typeInfo.RequiresFormat()) {
            ss << "/* format qualifier needed */ ";
        }

        ss << "uniform " << typeInfo.glslName << " " << slot.name << ";";

        return ss.str();
    }

    // ============================================================================
    // ShaderMetadata Implementation
    // ============================================================================

    const DescriptorSet* ShaderMetadata::GetSet(uint32_t setNumber) const {
        auto it = std::find_if(descriptorSets.begin(), descriptorSets.end(),
            [setNumber](const DescriptorSet& set) {
                return set.setNumber == setNumber;
            });
        return it != descriptorSets.end() ? &(*it) : nullptr;
    }

    DescriptorSet* ShaderMetadata::GetSet(uint32_t setNumber) {
        auto it = std::find_if(descriptorSets.begin(), descriptorSets.end(),
            [setNumber](const DescriptorSet& set) {
                return set.setNumber == setNumber;
            });
        return it != descriptorSets.end() ? &(*it) : nullptr;
    }

    const DescriptorSlot* ShaderMetadata::FindDescriptor(const std::string& name) const {
        for (const auto& set : descriptorSets) {
            if (auto* slot = set.FindSlot(name)) {
                return slot;
            }
        }
        return nullptr;
    }

    std::shared_ptr<const BufferObjectDefinition> ShaderMetadata::FindBuffer(const std::string& name) const {
        for (const auto& set : descriptorSets) {
            if (auto buffer = set.GetBuffer(name)) {
                return buffer;
            }
        }
        return nullptr;
    }

    std::shared_ptr<BufferObjectDefinition> ShaderMetadata::FindBuffer(const std::string& name) {
        for (auto& set : descriptorSets) {
            if (auto buffer = set.GetBuffer(name)) {
                return buffer;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<const BufferObjectDefinition>> ShaderMetadata::GetAllBuffers() const {
        std::vector<std::shared_ptr<const BufferObjectDefinition>> result;

        // Add buffers from all descriptor sets
        for (const auto& set : descriptorSets) {
            auto buffers = set.GetAllBuffers();
            result.insert(result.end(), buffers.begin(), buffers.end());
        }

        return result;
    }

    std::vector<const DescriptorSlot*> ShaderMetadata::GetAllSamplers() const {
        std::vector<const DescriptorSlot*> result;
        for (const auto& set : descriptorSets) {
            auto samplers = set.GetAllSamplers();
            result.insert(result.end(), samplers.begin(), samplers.end());
        }
        return result;
    }

    bool ShaderMetadata::ValidateDescriptorSets() const {
        for (const auto& set : descriptorSets) {
            if (set.HasBindingConflict() || !set.ValidateBuffers()) {
                return false;
            }
        }
        return true;
    }

    std::shared_ptr<const BufferObjectDefinition> ShaderMetadata::GetGlobalUBO() const {
        if (!usesGlobalUBO) {
            return nullptr;
        }
        const auto* globalSet = GetGlobalSet();
        if (!globalSet) {
            return nullptr;
        }
        return globalSet->GetBufferByBinding(GLOBAL_UBO_BINDING);
    }

    std::shared_ptr<BufferObjectDefinition> ShaderMetadata::GetGlobalUBO() {
        if (!usesGlobalUBO) {
            return nullptr;
        }
        auto* globalSet = GetSet(GLOBAL_DESCRIPTOR_SET);
        if (!globalSet) {
            return nullptr;
        }
        return globalSet->GetBufferByBinding(GLOBAL_UBO_BINDING);
    }

    std::shared_ptr<const BufferObjectDefinition> ShaderMetadata::GetObjectUBO() const {
        if (!usesObjectUBO) {
            return nullptr;
        }
        const auto* objectSet = GetObjectSet();
        if (!objectSet) {
            return nullptr;
        }
        return objectSet->GetBufferByBinding(OBJECT_UBO_BINDING);
    }

    std::shared_ptr<BufferObjectDefinition> ShaderMetadata::GetObjectUBO() {
        if (!usesObjectUBO) {
            return nullptr;
        }
        auto* objectSet = GetSet(OBJECT_DESCRIPTOR_SET);
        if (!objectSet) {
            return nullptr;
        }
        return objectSet->GetBufferByBinding(OBJECT_UBO_BINDING);
    }

} // namespace ShaderLib
