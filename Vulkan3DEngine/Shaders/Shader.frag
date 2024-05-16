#version 450

struct DirectionalLight {
    vec3 direction;
    vec4 color; // w is for intensity
};

struct PointLight {
    vec3 position;
    float radius;
    vec4 color;  // w is for intensity
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

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 directionToCamera;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(directionToCamera);

    // Directional light
    vec3 directional = vec3(0.0, 0.0, 0.0);
    vec3 lightDir = normalize(-global.directionalLight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), model.shininess);
    directional += global.directionalLight.color.w * (model.kd * diff + model.ks * spec) * global.directionalLight.color.xyz;

    // Point light
    vec3 point = vec3(0.0, 0.0, 0.0);
    for(int i = 0; i < global.activePointLights; ++i) {
        vec3 L = global.pointLights[i].position - fragPos;
        float distance = length(L);
        vec3 lightDir = normalize(L);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), model.shininess);

        float d = max(distance - global.pointLights[i].radius, 0.0) / global.pointLights[i].color.w;
        L /= distance;

        // Attenuation
        // calculate basic attenuation
        float denom = d/global.pointLights[i].radius + 1;
        float attenuation = 1 / (denom*denom);
         
        // scale and bias attenuation such that:
        //   attenuation == 0 at extent of max influence
        //   attenuation == 1 when d == 0
        float cutoff = 0.0001;
        attenuation = (attenuation - cutoff) / (1 - cutoff);
        attenuation = max(attenuation, 0.0);
         
        //diff = max(dot(normal, L), 0.0);

        point += global.pointLights[i].color.xyz * (model.kd * diff + model.ks * spec) * global.pointLights[i].color.w * attenuation;
    }


    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 result = (global.ka + directional + point) * fragColor * texColor.rgb;
    outColor = vec4(result, 1.0);
}