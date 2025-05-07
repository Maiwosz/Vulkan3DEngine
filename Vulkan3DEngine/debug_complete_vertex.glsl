#version 450

struct DirectionalLight {
    vec3 direction;
    vec4 color; // w is intensity
};

struct PointLight {
    vec3 position;
    float radius;
    vec4 color; // w is intensity
};

struct SpotLight {
    vec3 position;
    float innerCutoff;
    vec3 direction;
    float outerCutoff;
    vec4 color; // w is intensity
    float range;
    float padding[3]; // Explicit padding
};

layout(std140, set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
    DirectionalLight directionalLight;
    PointLight pointLights[64];
    SpotLight spotLights[16];
    int activePointLights;
    int activeSpotLights;
};



layout(std140, set = 1, binding = 0) uniform ObjectUBO {
    mat4 model;
    vec4 color;
};



layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;

void main() {
    vec4 posWorld = model * vec4(inPosition, 1.0);
    gl_Position = proj * view * posWorld;
    fragNormal = normalize(mat3(transpose(inverse(model))) * inNormal);
    fragPos = posWorld.xyz;
}

