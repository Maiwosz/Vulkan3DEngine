#version 450

// Simple compute shader for testing
// This shader doubles all values in the buffer

ShaderData {
    InputOutputData:storage(
        cpu{EveryFewFrames, ReadWrite, Small},
        gpu{OncePerFrame, ReadWrite, Small}
    ) {
        float values[256];  // Array of 256 float values
    };
};

#stage compute

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    // Simple operation: double the value
    inputOutputData.values[idx] = inputOutputData.values[idx] * 2.0;
}