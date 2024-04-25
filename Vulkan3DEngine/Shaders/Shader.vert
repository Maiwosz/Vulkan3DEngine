#version 450

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float ka; // Ambient coefficient
    float kd; // Diffuse coefficient
    float ks; // Specular coefficient
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float ka; // Ambient coefficient
    float kd; // Diffuse coefficient
    float ks; // Specular coefficient
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
    DirectionalLight directionaLight;
    PointLight pointLights[1024];
    int activePointLights;
} global;


layout(set = 1, binding = 0) uniform ObjectUniformBufferObject {
    mat4 model;
    float shininess;
} model;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 FragPos;
layout(location = 4) out vec3 directionToCamera;

void main() {
    gl_Position = global.proj * global.view * model.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = mat3(transpose(inverse(model.model))) * inNormal;
    gl_Position = global.proj * global.view * model.model * vec4(inPosition, 1.0);
    FragPos = vec3(model.model * vec4(inPosition, 1.0));
    directionToCamera = global.cameraPosition - (model.model * vec4(inPosition, 1.0)).xyz;
}
