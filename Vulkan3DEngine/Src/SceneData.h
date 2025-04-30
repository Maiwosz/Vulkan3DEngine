#pragma once

#include <cstdint>
#include <vector>
#include <glm/ext/matrix_float4x4.hpp>

constexpr uint32_t MAX_POINT_LIGHTS = 64;
constexpr uint32_t MAX_DIRECTIONAL_LIGHTS = 4;

struct PhongMaterial {
	alignas(4) float shininess;
	alignas(4) float diffuseCoefficient;
	alignas(4) float specularCoefficient;
	alignas(4) float ambientCoefficient;
};

struct MeshBatch {
	//MeshPtr mesh;
	//MaterialComponent* material;
	std::vector<glm::mat4> instances;
};

struct PointLight {
	alignas(16)glm::vec3 position;
	alignas(16)glm::vec4 color;
	alignas(4) float radius;
};

struct DirectionalLight {
	alignas(16) glm::vec3 direction;
	alignas(16) glm::vec4 color;
};
