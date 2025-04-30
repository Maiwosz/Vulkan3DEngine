#pragma once
#include "Component.h"
#include "AssetHandle.h"

struct MeshComponent : public Component {
public:
    void setMesh(AssetHandle mesh) {
        m_mesh = mesh;
        incrementVersion();
    }

    AssetHandle getMesh() {
        return m_mesh;
    }

private:
    AssetHandle m_mesh;

};