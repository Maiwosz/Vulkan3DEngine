#pragma once
#include "Component.h"
#include "AssetHandle.h"

struct MaterialComponent : public Component {
public:
    void setMaterial(AssetHandle material) {
        m_material = material;
        incrementVersion();
    }

    AssetHandle getMaterial() {
        return m_material;
    }

private:
    AssetHandle m_material;

};