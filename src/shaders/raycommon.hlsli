#ifndef RAYCOMMON
#define RAYCOMMON

struct RayPayload {
    float4 color;
};

struct GPUSphere {
    float4 positionRadius;
    float4 color;
};

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure Scene;

[[vk::binding(1, 0)]]
RWTexture2D<float4> Output;

[[vk::binding(2, 0)]]
StructuredBuffer<GPUSphere> Spheres;

#endif