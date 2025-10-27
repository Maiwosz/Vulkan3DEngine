#pragma once
#include "RenderOrder.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <vulkan/vulkan.h>
#include "DrawCall.h"
#include <memory>

// Forward declarations
class Buffer;
class EngineCore;

class MeshRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Mesh; }

    // Asset resolution stage
    MeshHandle meshHandle;
    MaterialHandle materialHandle;

    // Uniform buffer stage - używamy SmartHandle dla automatycznego zarządzania
    SmartHandle<BufferHandle, Buffer> objectUBOHandle;

    // DrawCall containing pipeline configuration and mesh data
    std::unique_ptr<DrawCall> drawCall;

    // Constructor - inicjalizuje DrawCall z domyślną ilością instancji
    MeshRenderOrder() : drawCall(std::make_unique<DrawCall>(1)) {}

    // Metody pomocnicze do sprawdzania ważności zasobów
    bool hasValidObjectUBO() const { return objectUBOHandle.isValid(); }

    // Metoda do sprawdzenia czy wszystkie krytyczne zasoby są dostępne
    bool isReadyForRendering() const {
        return meshHandle.isValid() &&
            materialHandle.isValid() &&
            hasValidObjectUBO() &&
            drawCall &&
            drawCall->hasMeshData();
    }
};