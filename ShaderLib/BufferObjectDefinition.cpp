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
        BufferType bufferType,
        const BufferAccessPatterns& accessPatterns
    )
        : m_layout(layout)
        , m_bufferType(bufferType)
        , m_accessPatterns(accessPatterns)
        , m_useInstanceName(true)
    {
        if (!layout) {
            throw std::runtime_error("Buffer layout cannot be null");
        }

        ValidateBufferConfiguration();

        if (!ValidateAccessPatterns()) {
            throw std::runtime_error(
                "Invalid access patterns: " + GetAccessPatternsValidationError()
            );
        }
    }

    BufferObjectDefinition::BufferObjectDefinition(
        std::shared_ptr<const StructureDefinition> structure,
        BufferType bufferType,
        const BufferAccessPatterns& accessPatterns,
        LayoutStandard layoutStandard
    )
        : m_bufferType(bufferType)
        , m_accessPatterns(accessPatterns)
        , m_useInstanceName(true)
    {
        if (!structure) {
            throw std::runtime_error("Structure definition cannot be null");
        }

        m_layout = std::make_shared<BufferLayout>(structure, layoutStandard);

        ValidateBufferConfiguration();

        if (!ValidateAccessPatterns()) {
            throw std::runtime_error(
                "Invalid access patterns: " + GetAccessPatternsValidationError()
            );
        }
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

        // Walidacja zgodności BufferType z AccessPatterns
        if (m_bufferType == BufferType::Uniform) {
            if (m_accessPatterns.gpuAccess.CanWrite()) {
                throw std::runtime_error(
                    "Uniform buffers cannot be GPU-writable"
                );
            }
        }
    }

    bool BufferObjectDefinition::ValidateAccessPatterns() const {
        return m_accessPatterns.IsValid();
    }

    std::string BufferObjectDefinition::GetAccessPatternsValidationError() const {
        return m_accessPatterns.GetValidationError();
    }

    // ============================================================================
    // ACCESS QUALIFIER RESOLUTION - HIERARCHICAL PRIORITY
    // ============================================================================

    AccessOperation BufferObjectDefinition::GetEffectiveGPUAccessOperation() const {
        // PRIORITY 1: BufferType - Uniform buffers are ALWAYS readonly for GPU
        if (m_bufferType == BufferType::Uniform) {
            return AccessOperation::ReadOnly;
        }

        // PRIORITY 2: BufferAccessPatterns - defines buffer-level access
        return m_accessPatterns.gpuAccess.operation;
    }

    AccessOperation BufferObjectDefinition::GetEffectiveFieldAccessOperation(
        const FieldDescriptor& field
    ) const {
        // Start with effective buffer-level access
        AccessOperation bufferAccess = GetEffectiveGPUAccessOperation();
        AccessOperation fieldAccess = field.accessOperation;

        // PRIORITY HIERARCHY:
        // 1. BufferType (already applied in bufferAccess)
        // 2. BufferAccessPatterns (already applied in bufferAccess)
        // 3. Field AccessOperation (can only further restrict)

        // If buffer is readonly, field MUST be readonly
        if (bufferAccess == AccessOperation::ReadOnly) {
            return AccessOperation::ReadOnly;
        }

        // If buffer is writeonly, field can be writeonly or none
        if (bufferAccess == AccessOperation::WriteOnly) {
            if (fieldAccess == AccessOperation::ReadOnly ||
                fieldAccess == AccessOperation::ReadWrite) {
                // Field cannot be more permissive than buffer
                return AccessOperation::WriteOnly;
            }
            return fieldAccess; // WriteOnly or None
        }

        // If buffer is readwrite, field can specify any restriction
        if (bufferAccess == AccessOperation::ReadWrite) {
            return fieldAccess; // Any access level is valid
        }

        // Default: use field access
        return fieldAccess;
    }

    std::string BufferObjectDefinition::AccessOperationToGLSLQualifier(
        AccessOperation operation
    ) const {
        switch (operation) {
        case AccessOperation::ReadOnly:
            return "readonly";
        case AccessOperation::WriteOnly:
            return "writeonly";
        case AccessOperation::ReadWrite:
        case AccessOperation::None:
        default:
            return "";
        }
    }

    // ============================================================================
    // GLSL GENERATION
    // ============================================================================

    std::string BufferObjectDefinition::GetAccessQualifier() const {
        if (m_bufferType == BufferType::Uniform) {
            // Uniform buffers don't use explicit qualifiers (implicitly readonly)
            return "";
        }

        // Get effective GPU access operation
        AccessOperation effectiveAccess = GetEffectiveGPUAccessOperation();
        return AccessOperationToGLSLQualifier(effectiveAccess);
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

            // Field-level access qualifier (only for storage buffers)
            if (m_bufferType == BufferType::Storage) {
                AccessOperation effectiveFieldAccess = GetEffectiveFieldAccessOperation(field);
                std::string fieldQualifier = AccessOperationToGLSLQualifier(effectiveFieldAccess);
                if (!fieldQualifier.empty()) {
                    ss << fieldQualifier << " ";
                }
            }

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
        j["accessPatterns"] = m_accessPatterns.ToJson();
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

        BufferAccessPatterns accessPatterns;
        if (j.contains("accessPatterns")) {
            accessPatterns = BufferAccessPatterns::FromJson(j.at("accessPatterns"));
        }

        auto definition = std::make_shared<BufferObjectDefinition>(
            layout,
            bufferType,
            accessPatterns
        );

        if (j.contains("useInstanceName")) {
            definition->SetUseInstanceName(j.at("useInstanceName").get<bool>());
        }

        return definition;
    }

} // namespace ShaderLib
