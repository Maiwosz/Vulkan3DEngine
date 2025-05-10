#version 450

#use global_ubo
#use object_ubo

InputData {
    float shininess;
    float ka;
    float kd;
    float ks;
	sampler albedo;
};

#stage vertex
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

    vec4 posWorld = model * vec4(inPosition, 1.0);
    gl_Position = proj * view * posWorld;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = normalize(mat3(transpose(inverse(model))) * inNormal);
    fragPos = posWorld.xyz;
    directionToCamera = cameraPosition - fragPos;
}

#stage fragment
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 directionToCamera;

layout(location = 0) out vec4 outColor;

void main() {

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(directionToCamera);

    vec3 directional = vec3(0.0);
    vec3 lightDir = normalize(-directionalLight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), inputData.shininess);
    directional += directionalLight.color.w * (inputData.kd * diff + inputData.ks * spec) * directionalLight.color.xyz;

    vec3 point = vec3(0.0);
    for(int i = 0; i < activePointLights; ++i) {
        vec3 L = pointLights[i].position - fragPos;
        float distance = length(L);
        vec3 lightDir = normalize(L);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), inputData.shininess);

        float d = max(distance - pointLights[i].radius, 0.0) / pointLights[i].color.w;
        float denom = d/pointLights[i].radius + 1;
        float attenuation = 1 / (denom*denom);
        attenuation = max((attenuation - 0.0001) / (1 - 0.0001), 0.0);

        point += pointLights[i].color.xyz * (inputData.kd * diff + inputData.ks * spec) * pointLights[i].color.w * attenuation;
    }

    vec4 texColor = texture(albedo, fragTexCoord);
    vec3 result = (inputData.ka + directional + point) * texColor.rgb;
    outColor = vec4(result, 1.0);
}