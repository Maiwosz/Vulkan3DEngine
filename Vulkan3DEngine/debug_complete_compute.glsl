#version 450

layout(std430, set = 2, binding = 2) buffer InputOutputData {
    float values[256];
} inputOutputData;

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    // Simple operation: double the value
    inputOutputData.values[idx] = inputOutputData.values[idx] * 2.0;
}