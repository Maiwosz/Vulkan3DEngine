#pragma once
#include "../../Prerequisites.h"

class Scene {
public:
    Scene();
    ~Scene();
    void addModel(MeshPtr mesh, TexturePtr texture);
    void update();
    //void render(CameraPtr camera);
private:
    std::shared_ptr<ModelManager> m_modelManager;
};

