#pragma once
#include "System.h"

class MeshRenderSystem : public System<> {
public:
    void update(ContextType& context) override;
    
};
