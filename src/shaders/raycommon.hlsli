#ifndef RAYCOMMON
#define RAYCOMMON

struct RayPayload {
    float4 color;
};

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure Scene;

[[vk::binding(1, 0)]]
RWTexture2D<float4> Output;

#endif