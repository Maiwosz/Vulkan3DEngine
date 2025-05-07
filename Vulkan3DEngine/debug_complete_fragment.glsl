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



layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Basic directional light calculation
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-directionalLight.direction);
    
    // Ambient and diffuse components
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * directionalLight.color.xyz;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * directionalLight.color.xyz * directionalLight.color.w;
    
    // Final color with object's base color
    vec3 result = (ambient + diffuse) * color.rgb;
    outColor = vec4(result, color.a);
}