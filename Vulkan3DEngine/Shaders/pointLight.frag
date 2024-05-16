#version 450

layout (location = 0) in vec2 fragOffset;
layout (location = 1) flat in int lightIndex;
layout (location = 0) out vec4 outColor;

struct PointLight {
  vec4 position; // ignore w
  vec4 color; // w is intensity
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

const float M_PI = 3.1415926538;

void main() {
  float dis = sqrt(dot(fragOffset, fragOffset));
  if (dis >= 1.0) {
    discard;
  }

  float cosDis = 0.5 * (cos(dis * M_PI) + 1.0); // ranges from 1 -> 0
  outColor = vec4(global.pointLights[lightIndex].color.xyz + 0.5 * cosDis, cosDis);
}
