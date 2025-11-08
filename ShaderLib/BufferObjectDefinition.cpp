#include "pch.h"
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include <stdexcept>
#include <sstream>

namespace ShaderLib {

    // ============================================================================
    // CONSTRUCTION
    // ============================================================================

    BufferObjectDefinition::BufferObjectDefinition(
        std::shared_ptr<BufferLayout> layout,
        BufferType bufferType
    )
        : m_layout(layout)
        , m_bufferType(bufferType)
        , m_useInstanceName(true)
    {
        if (!layout) {
            throw std::runtime_error("Buffer layout cannot be null");
        }

        ValidateBufferConfiguration();
    }

    BufferObjectDefinition::BufferObjectDefinition(
        std::shared_ptr<const StructureDefinition> structure,
        BufferType bufferType,
        LayoutStandard layoutStandard
    )
        : m_bufferType(bufferType)
        , m_useInstanceName(true)
    {
        if (!structure) {
            throw std::runtime_error("Structure definition cannot be null");
        }

        m_layout = std::make_shared<BufferLayout>(structure, layoutStandard);

        ValidateBufferConfiguration();
    }

    // ============================================================================
    // VALIDATION
    // ============================================================================

    void BufferObjectDefinition::ValidateBufferConfiguration() const {
        // Uniform buffers must use std140
        if (m_bufferType == BufferType::Uniform) {
            if (m_layout->GetLayoutStandard() != LayoutStandard::Std140) {
                throw std::runtime_error(
                    "Uniform buffers must use Std140 layout standard"
                );
            }
        }
    }

    // ============================================================================
    // ACCESS MODE ANALYSIS
    // ============================================================================

    BufferAccessMode BufferObjectDefinition::ComputeEffectiveAccessMode() const {
        if (m_bufferType == BufferType::Uniform) {
            return BufferAccessMode::ReadOnly;
        }

        bool hasReadOnly = false;
        bool hasWriteOnly = false;
        bool hasReadWrite = false;

        const auto& fields = m_layout->GetAllFields();
        for (const auto& field : fields) {
            if (!field.isBaseType) {
                continue;
            }

            switch (field.accessMode) {
            case BufferAccessMode::ReadOnly:
                hasReadOnly = true;
                break;
            case BufferAccessMode::WriteOnly:
                hasWriteOnly = true;
                break;
            case BufferAccessMode::ReadWrite:
                hasReadWrite = true;
                break;
            }
        }

        if (hasReadWrite || (hasReadOnly && hasWriteOnly)) {
            return BufferAccessMode::ReadWrite;
        }

        if (hasWriteOnly && !hasReadOnly) {
            return BufferAccessMode::WriteOnly;
        }

        return BufferAccessMode::ReadOnly;
    }

    // ============================================================================
    // GLSL GENERATION
    // ============================================================================

    std::string BufferObjectDefinition::GetAccessQualifier() const {
        if (m_bufferType == BufferType::Uniform) {
            return "";
        }

        BufferAccessMode effectiveMode = ComputeEffectiveAccessMode();

        switch (effectiveMode) {
        case BufferAccessMode::ReadOnly:
            return "readonly";
        case BufferAccessMode::WriteOnly:
            return "writeonly";
        case BufferAccessMode::ReadWrite:
        default:
            return "";
        }
    }

    std::string BufferObjectDefinition::GenerateBufferGLSL(
        uint32_t set,
        uint32_t binding
    ) const {
        std::stringstream ss;

        // Layout qualifier
        const char* layoutKeyword = (m_layout->GetLayoutStandard() == LayoutStandard::Std140)
            ? "std140" : "std430";

        // Buffer keyword
        const char* bufferKeyword = (m_bufferType == BufferType::Uniform)
            ? "uniform" : "buffer";

        // Access qualifier (for storage buffers only)
        std::string accessQualifier = GetAccessQualifier();
        if (!accessQualifier.empty()) {
            accessQualifier += " ";
        }

        // Generate buffer declaration
        ss << "layout(" << layoutKeyword
            << ", set = " << set
            << ", binding = " << binding << ") "
            << accessQualifier << bufferKeyword << " " << GetName() << " {\n";

        // Generate field declarations (top-level only)
        const auto& topLevelIndices = m_layout->GetTopLevelIndices();
        const auto& allFields = m_layout->GetAllFields();

        for (size_t index : topLevelIndices) {
            const auto& field = allFields[index];

            ss << "    ";

            // Type name
            if (field.isBaseType) {
                ss << BaseTypeToString(field.baseType);
            }
            else {
                ss << field.structTypeName;
            }

            ss << " " << field.name;

            // Array suffix
            if (field.isArray) {
                ss << "[" << field.arraySize << "]";
            }

            ss << ";\n";
        }

        ss << "}";

        // Instance name
        if (m_useInstanceName) {
            std::string instanceName = GetName();
            if (!instanceName.empty()) {
                instanceName[0] = std::tolower(instanceName[0]);
            }
            ss << " " << instanceName;
        }

        ss << ";";

        return ss.str();
    }

    void BufferObjectDefinition::CollectNestedStructDefinitions(
        std::set<std::string>& outStructDefs,
        std::set<std::string>& processedNames
    ) const {
        // Zbierz z oryginalnej StructureDefinition (nie z layout)
        auto structDef = m_layout->GetStructure();
        CollectNestedStructDefinitionsRecursive(
            structDef,
            outStructDefs,
            processedNames
        );
    }

    void BufferObjectDefinition::CollectNestedStructDefinitionsRecursive(
        std::shared_ptr<const StructureDefinition> structDef,
        std::set<std::string>& outStructDefs,
        std::set<std::string>& processedNames
    ) const {
        if (!structDef) {
            return;
        }

        const auto& fields = structDef->GetFields();

        // Najpierw rekurencyjnie zbierz wszystkie zagnieżdżone struktury
        for (const auto& field : fields) {
            if (!field.isBaseType() && field.structDef) {
                const std::string& nestedName = field.structDef->GetName();

                // Skip jeśli już przetworzona
                if (processedNames.find(nestedName) != processedNames.end()) {
                    continue;
                }

                processedNames.insert(nestedName);

                // Rekurencyjnie zbierz struktury z zagnieżdżonej struktury
                CollectNestedStructDefinitionsRecursive(
                    field.structDef,
                    outStructDefs,
                    processedNames
                );

                // Dodaj definicję GLSL tej zagnieżdżonej struktury
                outStructDefs.insert(field.structDef->GenerateGLSL());
            }
        }
    }

    // ============================================================================
    // INSTANCE FACTORY
    // ============================================================================

    std::shared_ptr<BufferObjectInstance> BufferObjectDefinition::CreateInstance() const {
        auto self = const_cast<BufferObjectDefinition*>(this)->shared_from_this();
        auto constSelf = std::const_pointer_cast<const BufferObjectDefinition>(self);

        return std::make_shared<BufferObjectInstance>(constSelf);
    }

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    json BufferObjectDefinition::ToJson() const {
        json j;
        j["layout"] = m_layout->ToJson();
        j["bufferType"] = static_cast<int>(m_bufferType);
        j["useInstanceName"] = m_useInstanceName;
        return j;
    }

    std::shared_ptr<BufferObjectDefinition> BufferObjectDefinition::FromJson(const json& j) {
        if (!j.contains("layout")) {
            throw std::runtime_error("Missing 'layout' field in JSON");
        }

        auto layout = BufferLayout::FromJson(j.at("layout"));
        BufferType bufferType = static_cast<BufferType>(
            j.value("bufferType", static_cast<int>(BufferType::Uniform))
            );

        auto definition = std::make_shared<BufferObjectDefinition>(layout, bufferType);

        if (j.contains("useInstanceName")) {
            definition->SetUseInstanceName(j.at("useInstanceName").get<bool>());
        }

        return definition;
    }

} // namespace ShaderLib
