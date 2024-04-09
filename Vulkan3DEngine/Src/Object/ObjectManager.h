#pragma once
#include "../Prerequisites.h"
#include "Object.h"
#include <vector>

class ObjectManager
{
public:
    ObjectManager();
    ~ObjectManager();

    void addObject(MeshPtr mesh, TexturePtr texture);
    void updateObjects();
    void drawObjects();

private:
    std::vector<std::shared_ptr<Object>> m_objects;
};