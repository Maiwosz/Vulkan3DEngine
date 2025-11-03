#pragma once
#include <cstddef>

namespace ShaderLib {

    // Interfejs dla bufora, który może być mapowany
    class IBufferMapping {
    public:
        virtual ~IBufferMapping() = default;

        // Mapowanie bufora - zwraca wskaźnik do danych lub nullptr
        virtual void* map() = 0;

        // Odmapowanie bufora
        virtual void unmap() = 0;

        // Sprawdzenie czy bufor jest obecnie zmapowany
        virtual bool isMapped() const = 0;
    };

} // namespace ShaderLib