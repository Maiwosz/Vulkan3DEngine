#version 450

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 directionToCamera;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 lightDirection;
    vec3 lightColor;
    vec3 cameraPosition;
} global;

layout(location = 0) out vec4 outColor;

void main() {
    // AMBIENT LIGHT
    float ka = 0.15f;
    vec3 ia = vec3(1.0, 1.0, 1.0);
    vec3 ambient_light = ka * ia;

    // DIFFUSE LIGHT
    float kd = 0.5;
    vec3 id = vec3(1.0, 1.0, 1.0);
    float amount_diffuse_light = max(0.0, dot(normalize(global.lightDirection), fragNormal));
    vec3 diffuse_light = kd * amount_diffuse_light * id;

    // SPECULAR LIGHT
    float ks = 0.1;
    vec3 is = vec3(1.0, 1.0, 1.0);
    vec3 reflected_light = reflect(-normalize(global.lightDirection), fragNormal);
    float shininess = 0.3;
    float amount_specular_light = pow(max(0.0, dot(reflected_light, normalize(directionToCamera))), shininess);
    vec3 specular_light = ks * amount_specular_light * is;

    vec3 final_light = ambient_light + diffuse_light + specular_light;

    outColor = vec4(final_light, 1.0) * texture(texSampler, fragTexCoord);
}
