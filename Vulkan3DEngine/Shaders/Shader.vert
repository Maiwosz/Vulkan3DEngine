#version 450

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
    DirectionalLight directionalLight;
    PointLight pointLights[64];
    int activePointLights;
    float ka; // Ambient coefficient
} global;


layout(set = 1, binding = 0) uniform ObjectUniformBufferObject {
    mat4 model;
    float shininess;
    float kd; // Diffuse coefficient
    float ks; // Specular coefficient
} model;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;
layout(location = 4) out vec3 directionToCamera;

void main() {
    vec4 posWorld = model.model * vec4(inPosition, 1.0);
    gl_Position = global.proj * global.view * posWorld;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = normalize(mat3(transpose(inverse(model.model))) * inNormal);
    fragPos = posWorld.xyz;
    directionToCamera = global.cameraPosition - fragPos;
}
