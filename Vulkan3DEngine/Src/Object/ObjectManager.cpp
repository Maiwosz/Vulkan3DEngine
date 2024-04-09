#pragma once
#include "Object.h"
#include "ObjectManager.h"

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::addObject(MeshPtr mesh, TexturePtr texture) {
    m_objects.push_back(std::make_shared<Object>(mesh, texture));
}

void ObjectManager::updateObjects() {
    for (auto& object : m_objects) {
        object->update();
    }
}

void ObjectManager::drawObjects() {
    for (auto& object : m_objects) {
        object->draw();
    }
}


