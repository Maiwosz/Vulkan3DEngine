#include "pch.h"
#include "ShaderLib.h"
#include "ShaderArray.h"
#include <algorithm>

namespace ShaderLib {

    // ============================================================================
    // BufferVariable Implementation
    // ============================================================================

    std::string BufferVariable::GetTypeName() const {
        if (IsComposite()) {
            return composite->GetTypeName();
        }
        return BaseTypeToString(baseType);
    }

    ShaderTypeCategory BufferVariable::GetCategory() const {
        if (IsComposite()) {
            return ShaderTypeCategory::Composite;
        }
        return (baseType != BaseType::Unknown)
            ? ShaderTypeCategory::Base
            : ShaderTypeCategory::Unknown;
    }

    std::string BufferVariable::GenerateGLSLDeclaration() const {
        std::stringstream ss;

        if (IsComposite() && composite->IsArray()) {
            // Array: "elementType name[count]"
            auto arrayDef = std::static_pointer_cast<const ShaderArrayDefinition>(composite);

            if (arrayDef->IsCompositeElement()) {
                ss << arrayDef->GetElementComposite()->GetTypeName();
            }
            else {
                ss << BaseTypeToString(arrayDef->GetElementBaseType());
            }
            ss << " " << name << "[" << arrayDef->GetArrayCount() << "]";
        }
        else {
            // Base type or struct: "typeName name"
            ss << GetTypeName() << " " << name;
        }

        return ss.str();
    }

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

    const BufferObject* DescriptorSet::GetBuffer(const std::string& name) const {
        auto it = buffers.find(name);
        return it != buffers.end() ? &it->second : nullptr;
    }

    BufferObject* DescriptorSet::GetBuffer(const std::string& name) {
        auto it = buffers.find(name);
        return it != buffers.end() ? &it->second : nullptr;
    }

    const BufferObject* DescriptorSet::GetBufferByBinding(uint32_t binding) const {
        if (const auto* slot = FindSlot(binding)) {
            if (slot->IsBuffer()) {
                return GetBuffer(slot->name);
            }
        }
        return nullptr;
    }

    BufferObject* DescriptorSet::GetBufferByBinding(uint32_t binding) {
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

    std::vector<const BufferObject*> DescriptorSet::GetAllBuffers() const {
        std::vector<const BufferObject*> result;
        for (const auto& slot : slots) {
            if (slot.IsBuffer()) {
                if (auto* buf = GetBuffer(slot.name)) {
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

        // Collect all unique composite type definitions from all buffers
        std::set<std::string> definitions;
        for (const auto& [name, buffer] : buffers) {
            for (const auto& var : buffer.variables) {
                if (var.IsComposite()) {
                    std::string glsl = var.composite->GenerateGLSL();
                    if (!glsl.empty()) {
                        definitions.insert(glsl);
                    }
                }
            }
        }

        // Output struct definitions first
        for (const auto& def : definitions) {
            ss << def << "\n";
        }

        if (!definitions.empty() && !slots.empty()) {
            ss << "\n";
        }

        // Generate declarations for each slot in binding order
        std::vector<const DescriptorSlot*> sortedSlots;
        for (const auto& slot : slots) {
            sortedSlots.push_back(&slot);
        }
        std::sort(sortedSlots.begin(), sortedSlots.end(),
            [](const DescriptorSlot* a, const DescriptorSlot* b) {
                return a->binding < b->binding;
            });

        for (const auto* slot : sortedSlots) {
            if (slot->IsBuffer()) {
                // Generate buffer declaration
                auto* buffer = GetBuffer(slot->name);
                if (buffer) {
                    ss << GenerateBufferGLSL(*buffer, setNumber, slot->binding) << "\n";
                }
            }
            else {
                // Generate sampler/image declaration
                ss << GenerateSamplerGLSL(*slot, setNumber) << "\n";
            }
        }

        return ss.str();
    }

    // Helper function for buffer generation 
    std::string DescriptorSet::GenerateBufferGLSL(const BufferObject& buffer,
        uint32_t set,
        uint32_t binding) const {
        std::stringstream ss;

        const char* layoutKeyword = (buffer.layoutStandard == LayoutStandard::Std140)
            ? "std140" : "std430";
        const char* bufferKeyword = (buffer.bufferType == BufferType::Uniform)
            ? "uniform" : "buffer";

        std::string accessQualifier = buffer.GetAccessQualifier();
        if (!accessQualifier.empty()) {
            accessQualifier += " ";
        }

        ss << "layout(" << layoutKeyword << ", set = " << set
            << ", binding = " << binding << ") "
            << accessQualifier << bufferKeyword << " " << buffer.name << " {\n";

        for (const auto& var : buffer.variables) {
            ss << "    " << var.GenerateGLSLDeclaration() << ";\n";
        }

        ss << "}";

        if (buffer.useInstanceName) {
            std::string instanceName = buffer.name;
            if (!instanceName.empty()) {
                instanceName[0] = std::tolower(instanceName[0]);
            }
            ss << " " << instanceName;
        }

        ss << ";";

        return ss.str();
    }

    // Helper function for sampler/image generation
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
    // BufferObject Implementation
    // ============================================================================

    std::string BufferObject::GetAccessQualifier() const {
        if (bufferType == BufferType::Uniform) {
            return "";
        }

        switch (accessMode) {
        case BufferAccessMode::ReadOnly: return "readonly";
        case BufferAccessMode::WriteOnly: return "writeonly";
        case BufferAccessMode::ReadWrite: return "";
        default: return "";
        }
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

    const BufferObject* ShaderMetadata::FindBuffer(const std::string& name) const {
        for (const auto& set : descriptorSets) {
            if (auto* buffer = set.GetBuffer(name)) {
                return buffer;
            }
        }
        return nullptr;
    }

    BufferObject* ShaderMetadata::FindBuffer(const std::string& name) {
        for (auto& set : descriptorSets) {
            if (auto* buffer = set.GetBuffer(name)) {
                return buffer;
            }
        }
        return nullptr;
    }

    std::vector<const BufferObject*> ShaderMetadata::GetAllBuffers() const {
        std::vector<const BufferObject*> result;
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

    std::vector<BufferObject> ShaderMetadata::GetCustomUniformBuffers() const {
        std::vector<BufferObject> result;
        for (const auto& buf : customBuffers) {
            if (buf.IsUniformBuffer()) {
                result.push_back(buf);
            }
        }
        return result;
    }

    std::vector<BufferObject> ShaderMetadata::GetCustomStorageBuffers() const {
        std::vector<BufferObject> result;
        for (const auto& buf : customBuffers) {
            if (buf.IsStorageBuffer()) {
                result.push_back(buf);
            }
        }
        return result;
    }

} // namespace ShaderLib