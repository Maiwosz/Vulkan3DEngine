#version 450

// Specify the descriptor set for the global UBO
layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
    mat4 view;
    mat4 proj;
} global;

// Specify the descriptor set for the model UBO
layout(set = 1, binding = 0) uniform ObjectUniformBufferObject {
    mat4 model;
} model;

// Specify the descriptor set for the sampler and texture
layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = global.proj * global.view * model.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
