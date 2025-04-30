// Shader.glsl
#version 450

// Common structures
struct DirectionalLight {
    vec3 direction;
    vec4 color; // w is for intensity
};

struct PointLight {
    vec3 position;
    float radius;
    vec4 color;  // w is for intensity
};

// Common uniform blocks
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

#pragma stage vertex
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

#pragma stage fragment
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 directionToCamera;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(directionToCamera);

    // Directional light calculations
    vec3 directional = vec3(0.0);
    vec3 lightDir = normalize(-global.directionalLight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), model.shininess);
    directional += global.directionalLight.color.w * (model.kd * diff + model.ks * spec) * global.directionalLight.color.xyz;

    // Point light calculations
    vec3 point = vec3(0.0);
    for(int i = 0; i < global.activePointLights; ++i) {
        vec3 L = global.pointLights[i].position - fragPos;
        float distance = length(L);
        vec3 lightDir = normalize(L);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), model.shininess);

        float d = max(distance - global.pointLights[i].radius, 0.0) / global.pointLights[i].color.w;
        float denom = d/global.pointLights[i].radius + 1;
        float attenuation = 1 / (denom*denom);
        attenuation = max((attenuation - 0.0001) / (1 - 0.0001), 0.0);

        point += global.pointLights[i].color.xyz * (model.kd * diff + model.ks * spec) * global.pointLights[i].color.w * attenuation;
    }

    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 result = (global.ka + directional + point) * fragColor * texColor.rgb;
    outColor = vec4(result, 1.0);
}