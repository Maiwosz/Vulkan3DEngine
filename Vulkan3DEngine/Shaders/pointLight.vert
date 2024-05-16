#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

const vec2 OFFSETS[6] = vec2[](
  vec2(-1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, -1.0),
  vec2(1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0)
);

layout (location = 0) out vec2 fragOffset;
layout (location = 1) flat out int lightIndex;

struct PointLight {
    vec3 position;
    float radius;
    vec4 color;  // w is for intensity
};

struct DirectionalLight {
    vec3 direction;
    vec4 color; // w is for intensity
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

void main() {
  fragOffset = OFFSETS[gl_VertexIndex];
  vec3 cameraRightWorld = {global.view[0][0], global.view[1][0], global.view[2][0]};
  vec3 cameraUpWorld = {global.view[0][1], global.view[1][1], global.view[2][1]};

  lightIndex = gl_InstanceIndex;
  vec3 positionWorld = global.pointLights[lightIndex].position.xyz
    + global.pointLights[lightIndex].radius * fragOffset.x * cameraRightWorld
    + global.pointLights[lightIndex].radius * fragOffset.y * cameraUpWorld;

  gl_Position = global.proj * global.view * vec4(positionWorld, 1.0);
}
