#pragma once
#include "System.h"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>

class TextureManagementSystem;

class RenderSystem : public System<TextureManagementSystem> {
public:
    void update(ContextType& context) override;
    
};

//namespace std {
//    template <>
//    struct hash<std::pair<MeshPtr, unsigned int>> {
//        size_t operator()(const std::pair<MeshPtr, unsigned int>& key) const {
//            return hash<MeshPtr>()(key.first) ^ (hash<unsigned int>()(key.second) << 16);
//        }
//    };
//}