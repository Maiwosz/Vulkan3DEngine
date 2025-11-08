#pragma once
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include <memory>
#include <string>
#include <vector>

namespace ShaderLib {

    // ============================================================================
    // BUFFER VALIDATOR - Weryfikacja i synchronizacja instancji z definicją
    // ============================================================================
    // Logika: Shader ma BufferObjectDefinition, Materiał ma BufferObjectInstance.
    // Validator sprawdza zgodność i tworzy nową instancję dopasowaną do definicji
    // shadera, kopiując pasujące pola z materiału.
    // ============================================================================

    class BufferValidator {
    public:
        // Raport walidacji
        struct ValidationReport {
            bool isCompatible = true;
            std::vector<std::string> errors;
            std::vector<std::string> warnings;
            std::vector<std::string> copiedFields;    // Pola skopiowane z materiału
            std::vector<std::string> defaultFields;   // Pola zainicjalizowane domyślnie

            bool HasErrors() const { return !errors.empty(); }
            bool HasWarnings() const { return !warnings.empty(); }

            std::string GetSummary() const;
            void Clear();
        };

        // ========================================================================
        // GŁÓWNE API
        // ========================================================================

        // Weryfikuje i synchronizuje instancję z definicją shadera
        // Zwraca:
        // - Tę samą instancję jeśli jest w pełni zgodna
        // - Nową instancję z skopiowanymi pasującymi polami
        // - nullptr jeśli definicja jest nieprawidłowa
        static std::shared_ptr<BufferObjectInstance> ValidateAndSync(
            std::shared_ptr<const BufferObjectDefinition> shaderDefinition,
            std::shared_ptr<const BufferObjectInstance> materialInstance,
            ValidationReport* outReport = nullptr
        );

        // Tylko walidacja bez tworzenia nowej instancji
        static ValidationReport Validate(
            std::shared_ptr<const BufferObjectDefinition> shaderDefinition,
            std::shared_ptr<const BufferObjectInstance> materialInstance
        );

    private:
        // Wewnętrzne helpery
        static bool ValidateDefinition(
            std::shared_ptr<const BufferObjectDefinition> definition,
            ValidationReport& report
        );

        static bool CheckCompatibility(
            std::shared_ptr<const BufferObjectDefinition> shaderDef,
            std::shared_ptr<const BufferObjectDefinition> materialDef,
            ValidationReport& report
        );

        static void CopyCompatibleFields(
            const std::vector<FieldDescriptor>& shaderFields,
            const std::vector<FieldDescriptor>& materialFields,
            std::shared_ptr<BufferObjectInstance> targetInstance,
            std::shared_ptr<const BufferObjectInstance> sourceInstance,
            ValidationReport& report
        );

        static bool AreFieldsCompatible(
            const FieldDescriptor& shaderField,
            const FieldDescriptor& materialField
        );
    };

} // namespace ShaderLib
