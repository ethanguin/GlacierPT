#ifndef RAYCOMMON
#define RAYCOMMON

// SHARED STRUCTS
struct RayPayload {
    float4 color;
};

struct GPUSphere {
    float4 positionRadius;
    float4 color;
};

// SHARED FUNCTIONS
uint PCGHash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Random(inout uint state) {
    state = PCGHash(state);
    return float(state) / 4294967296.0;
}

float2 Random2(inout uint state) {
    return float2(Random(state), Random(state));
}

// SHARED BUFFERS
cbuffer FrameConstants : register(b0) {
    uint FrameIndex;
};

// VULKAN BINGINDS
[[vk::binding(0, 0)]]
RaytracingAccelerationStructure Scene;

[[vk::binding(1, 0)]]
RWTexture2D<float4> Output;

[[vk::binding(2, 0)]]
StructuredBuffer<GPUSphere> Spheres;

#endif