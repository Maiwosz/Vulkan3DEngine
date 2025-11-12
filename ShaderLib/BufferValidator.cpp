#include "pch.h"
#include "BufferValidator.h"
#include <sstream>
#include <unordered_map>

namespace ShaderLib {

    // ============================================================================
    // VALIDATION REPORT
    // ============================================================================

    std::string BufferValidator::ValidationReport::GetSummary() const {
        std::stringstream ss;

        if (isCompatible && !HasWarnings()) {
            ss << "✓ Validation passed - instance fully compatible\n";
            return ss.str();
        }

        if (!isCompatible) {
            ss << "✗ Validation failed - instance incompatible\n\n";
        }
        else {
            ss << "⚠ Validation passed with warnings\n\n";
        }

        if (HasErrors()) {
            ss << "Errors (" << errors.size() << "):\n";
            for (const auto& error : errors) {
                ss << "  ✗ " << error << "\n";
            }
            ss << "\n";
        }

        if (HasWarnings()) {
            ss << "Warnings (" << warnings.size() << "):\n";
            for (const auto& warning : warnings) {
                ss << "  ⚠ " << warning << "\n";
            }
            ss << "\n";
        }

        if (!copiedFields.empty()) {
            ss << "Copied fields (" << copiedFields.size() << "):\n";
            for (const auto& field : copiedFields) {
                ss << "  ✓ " << field << "\n";
            }
            ss << "\n";
        }

        if (!defaultFields.empty()) {
            ss << "Default initialized (" << defaultFields.size() << "):\n";
            for (const auto& field : defaultFields) {
                ss << "  • " << field << "\n";
            }
        }

        return ss.str();
    }

    void BufferValidator::ValidationReport::Clear() {
        isCompatible = true;
        errors.clear();
        warnings.clear();
        copiedFields.clear();
        defaultFields.clear();
    }

    // ============================================================================
    // GŁÓWNE API
    // ============================================================================

    std::shared_ptr<BufferObjectInstance> BufferValidator::ValidateAndSync(
        std::shared_ptr<const BufferObjectDefinition> shaderDefinition,
        std::shared_ptr<const BufferObjectInstance> materialInstance,
        ValidationReport* outReport
    ) {
        ValidationReport localReport;
        ValidationReport& report = outReport ? *outReport : localReport;
        report.Clear();

        // 1. Walidacja definicji shadera
        if (!ValidateDefinition(shaderDefinition, report)) {
            return nullptr;
        }

        // 2. Jeśli brak instancji materiału - utwórz nową z wartościami domyślnymi
        if (!materialInstance) {
            report.warnings.push_back("Material instance is null - creating new instance with defaults");
            auto newInstance = shaderDefinition->CreateInstance();

            // Wszystkie pola zainicjalizowane domyślnie
            const auto& allFields = shaderDefinition->GetAllFields();
            for (const auto& field : allFields) {
                if (field.isBaseType) {
                    report.defaultFields.push_back(field.path);
                }
            }

            return newInstance;
        }

        // 3. Sprawdź kompatybilność definicji
        auto materialDefinition = materialInstance->GetDefinition();
        bool compatible = CheckCompatibility(shaderDefinition, materialDefinition, report);

        // 4. Jeśli w pełni zgodne - zwróć oryginalną instancję
        if (compatible && !report.HasWarnings()) {
            report.isCompatible = true;
            return std::const_pointer_cast<BufferObjectInstance>(materialInstance);
        }

        // 5. Utwórz nową instancję dopasowaną do definicji shadera
        auto synchronizedInstance = shaderDefinition->CreateInstance();

        // 6. Skopiuj pasujące pola z materiału
        const auto& shaderFields = shaderDefinition->GetAllFields();
        const auto& materialFields = materialDefinition->GetAllFields();

        CopyCompatibleFields(
            shaderFields,
            materialFields,
            synchronizedInstance,
            materialInstance,
            report
        );

        // 7. Zaznacz pola zainicjalizowane domyślnie
        for (const auto& shaderField : shaderFields) {
            if (!shaderField.isBaseType) {
                continue; // Pomijamy struktury
            }

            bool wasCopied = std::find(
                report.copiedFields.begin(),
                report.copiedFields.end(),
                shaderField.path
            ) != report.copiedFields.end();

            if (!wasCopied) {
                report.defaultFields.push_back(shaderField.path);
            }
        }

        return synchronizedInstance;
    }

    BufferValidator::ValidationReport BufferValidator::Validate(
        std::shared_ptr<const BufferObjectDefinition> shaderDefinition,
        std::shared_ptr<const BufferObjectInstance> materialInstance
    ) {
        ValidationReport report;

        if (!ValidateDefinition(shaderDefinition, report)) {
            return report;
        }

        if (!materialInstance) {
            report.warnings.push_back("Material instance is null");
            return report;
        }

        auto materialDefinition = materialInstance->GetDefinition();
        CheckCompatibility(shaderDefinition, materialDefinition, report);

        return report;
    }

    // ============================================================================
    // WEWNĘTRZNE HELPERY
    // ============================================================================

    bool BufferValidator::ValidateDefinition(
        std::shared_ptr<const BufferObjectDefinition> definition,
        ValidationReport& report
    ) {
        if (!definition) {
            report.errors.push_back("Shader definition is null");
            report.isCompatible = false;
            return false;
        }

        if (!definition->GetLayout()) {
            report.errors.push_back("Shader definition has no layout");
            report.isCompatible = false;
            return false;
        }

        if (!definition->GetLayout()->GetStructure()) {
            report.errors.push_back("Shader definition has no structure");
            report.isCompatible = false;
            return false;
        }

        return true;
    }

    bool BufferValidator::CheckCompatibility(
        std::shared_ptr<const BufferObjectDefinition> shaderDef,
        std::shared_ptr<const BufferObjectDefinition> materialDef,
        ValidationReport& report
    ) {
        bool fullyCompatible = true;

        // Sprawdź typ bufora
        if (shaderDef->GetBufferType() != materialDef->GetBufferType()) {
            const char* shaderType = (shaderDef->GetBufferType() == BufferType::Uniform)
                ? "Uniform" : "Storage";
            const char* materialType = (materialDef->GetBufferType() == BufferType::Uniform)
                ? "Uniform" : "Storage";

            report.warnings.push_back(
                std::string("Buffer type mismatch: shader expects ") + shaderType +
                ", material has " + materialType
            );
            fullyCompatible = false;
        }

        // Sprawdź standard layoutu
        if (shaderDef->GetLayoutStandard() != materialDef->GetLayoutStandard()) {
            const char* shaderStd = (shaderDef->GetLayoutStandard() == LayoutStandard::Std140)
                ? "Std140" : "Std430";
            const char* materialStd = (materialDef->GetLayoutStandard() == LayoutStandard::Std140)
                ? "Std140" : "Std430";

            report.warnings.push_back(
                std::string("Layout standard mismatch: shader expects ") + shaderStd +
                ", material has " + materialStd
            );
            fullyCompatible = false;
        }

        // Pobierz tylko base type fields do porównania
        const auto& shaderAllFields = shaderDef->GetAllFields();
        const auto& materialAllFields = materialDef->GetAllFields();

        std::vector<const FieldDescriptor*> shaderFields;
        std::vector<const FieldDescriptor*> materialFields;

        for (const auto& field : shaderAllFields) {
            if (field.isBaseType) {
                shaderFields.push_back(&field);
            }
        }

        for (const auto& field : materialAllFields) {
            if (field.isBaseType) {
                materialFields.push_back(&field);
            }
        }

        // Porównaj liczbę pól base type
        if (shaderFields.size() != materialFields.size()) {
            report.warnings.push_back(
                "Field count mismatch: shader expects " + std::to_string(shaderFields.size()) +
                " fields, material has " + std::to_string(materialFields.size()) + " fields"
            );
            fullyCompatible = false;
        }

        // KLUCZOWE: Sprawdź kolejność pól - jeśli nazwy się nie zgadzają na tych samych pozycjach,
        // to layout jest inny i trzeba utworzyć nową instancję
        size_t minFieldCount = std::min(shaderFields.size(), materialFields.size());
        for (size_t i = 0; i < minFieldCount; ++i) {
            const auto* shaderField = shaderFields[i];
            const auto* materialField = materialFields[i];

            if (shaderField->path != materialField->path) {
                report.warnings.push_back(
                    "Field order mismatch at index " + std::to_string(i) +
                    ": shader has '" + shaderField->path +
                    "', material has '" + materialField->path + "'"
                );
                fullyCompatible = false;
                // Można przerwać od razu - kolejność się nie zgadza
                break;
            }

            // Sprawdź też czy typy się zgadzają
            if (!AreFieldsCompatible(*shaderField, *materialField)) {
                report.warnings.push_back(
                    "Incompatible field at index " + std::to_string(i) + ": " + shaderField->path
                );
                fullyCompatible = false;
            }
        }

        // Jeśli mamy różną liczbę pól, zgłoś brakujące lub nadmiarowe
        if (shaderFields.size() > materialFields.size()) {
            for (size_t i = materialFields.size(); i < shaderFields.size(); ++i) {
                report.warnings.push_back(
                    "Missing field in material: " + shaderFields[i]->path
                );
            }
        }
        else if (materialFields.size() > shaderFields.size()) {
            for (size_t i = shaderFields.size(); i < materialFields.size(); ++i) {
                report.warnings.push_back(
                    "Extra field in material (will be ignored): " + materialFields[i]->path
                );
            }
        }

        return fullyCompatible;
    }

    void BufferValidator::CopyCompatibleFields(
        const std::vector<FieldDescriptor>& shaderFields,
        const std::vector<FieldDescriptor>& materialFields,
        std::shared_ptr<BufferObjectInstance> targetInstance,
        std::shared_ptr<const BufferObjectInstance> sourceInstance,
        ValidationReport& report
    ) {
        // Zbuduj mapę pól materiału dla szybkiego lookup
        std::unordered_map<std::string, const FieldDescriptor*> materialFieldMap;
        for (const auto& field : materialFields) {
            if (field.isBaseType) {
                materialFieldMap[field.path] = &field;
            }
        }

        // Dla każdego pola w shaderze, spróbuj skopiować z materiału
        for (const auto& shaderField : shaderFields) {
            if (!shaderField.isBaseType) {
                continue; // Pomijamy struktury, kopiujemy tylko base types
            }

            auto it = materialFieldMap.find(shaderField.path);

            if (it == materialFieldMap.end()) {
                // Pole nie istnieje w materiale - zostanie zainicjalizowane domyślnie
                continue;
            }

            const auto* materialField = it->second;

            // Sprawdź kompatybilność
            if (!AreFieldsCompatible(shaderField, *materialField)) {
                report.warnings.push_back(
                    "Cannot copy field '" + shaderField.path +
                    "' - type mismatch (will use default value)"
                );
                continue;
            }

            // Skopiuj wartość przez surowy bufor bajtów
            try {
                const uint8_t* srcPtr = sourceInstance->GetRawBuffer() + materialField->offset;
                uint8_t* dstPtr = targetInstance->GetRawBuffer() + shaderField.offset;

                // Dla tablic kopiujemy całą tablicę (używamy totalSize zamiast size)
                uint32_t copySize = shaderField.isArray ? shaderField.totalSize : shaderField.size;

                std::memcpy(dstPtr, srcPtr, copySize);

                report.copiedFields.push_back(shaderField.path);
            }
            catch (const std::exception& e) {
                report.warnings.push_back(
                    "Failed to copy field '" + shaderField.path + "': " + e.what()
                );
            }
        }
    }

    bool BufferValidator::AreFieldsCompatible(
        const FieldDescriptor& shaderField,
        const FieldDescriptor& materialField
    ) {
        // Sprawdź typ bazowy
        if (shaderField.baseType != materialField.baseType) {
            return false;
        }

        // Sprawdź rozmiar elementu
        if (shaderField.size != materialField.size) {
            return false;
        }

        // Sprawdź czy oba są/nie są tablicami
        if (shaderField.isArray != materialField.isArray) {
            return false;
        }

        // Jeśli to tablice, sprawdź rozmiar tablicy
        if (shaderField.isArray && shaderField.arraySize != materialField.arraySize) {
            return false;
        }

        return true;
    }

} // namespace ShaderLib
