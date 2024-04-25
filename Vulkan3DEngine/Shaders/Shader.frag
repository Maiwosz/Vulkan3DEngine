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
    DirectionalLight directionalLight;
    PointLight pointLights[1024];
    int activePointLights;
} global;

layout(set = 1, binding = 0) uniform ObjectUniformBufferObject {
    mat4 model;
    float shininess;
} model;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 FragPos;
layout(location = 4) in vec3 directionToCamera;

layout(location = 0) out vec4 outColor;

void main() {
    //Directional Light
    // AMBIENT LIGHT
    vec3 ambient_light = global.directionalLight.ka * global.directionalLight.color * global.directionalLight.intensity;

    // DIFFUSE LIGHT
    float amount_diffuse_light = max(0.0, dot(normalize(global.directionalLight.direction), fragNormal));
    vec3 diffuse_light = global.directionalLight.kd * amount_diffuse_light * global.directionalLight.color * global.directionalLight.intensity;

    // SPECULAR LIGHT
    vec3 reflected_light = reflect(-normalize(global.directionalLight.direction), fragNormal);
    float amount_specular_light = pow(max(0.0, dot(reflected_light, normalize(directionToCamera))), model.shininess);
    vec3 specular_light = global.directionalLight.ks * amount_specular_light * global.directionalLight.color * global.directionalLight.intensity;

    vec3 final_light = ambient_light + diffuse_light + specular_light;

    // POINT LIGHTS
    for (int i = 0; i < global.activePointLights; i++) {
        vec3 lightDir = normalize(global.pointLights[i].position - FragPos);
        
        // DIFFUSE LIGHT
        float diff = max(dot(fragNormal, lightDir), 0.0);
        vec3 diffuse = diff * global.pointLights[i].color * global.pointLights[i].intensity;

        // SPECULAR LIGHT
        vec3 viewDir = normalize(global.cameraPosition - FragPos);
        vec3 reflectDir = reflect(-lightDir, fragNormal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), model.shininess);
        vec3 specular = global.pointLights[i].ks * spec * global.pointLights[i].color * global.pointLights[i].intensity;

        // ATTENUATION (using the constant-linear-quadratic model)
        float distance = length(global.pointLights[i].position - FragPos);
        float attenuation = 1.0 / (global.pointLights[i].constant + global.pointLights[i].linear * distance + global.pointLights[i].quadratic * (distance * distance));

        // AMBIENT LIGHT
        vec3 ambient = global.pointLights[i].ka * global.pointLights[i].color * global.pointLights[i].intensity;

        // Add contributions from this point light to the final color
        final_light += (ambient + (diffuse + specular) * attenuation);
    }

    outColor = vec4(final_light, 1.0) * texture(texSampler, fragTexCoord);
}

