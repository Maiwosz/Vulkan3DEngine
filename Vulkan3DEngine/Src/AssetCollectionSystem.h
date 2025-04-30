#pragma once
#include "System.h"
#include "Registry.h"

class AssetCollectionSystem : public System<> {
public:
    void update(ContextType& context) override;
};