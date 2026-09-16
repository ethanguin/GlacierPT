#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")] void ClosestHit(inout RayPayload payload, in HitAttributes attributes) {
    uint instanceIndex = InstanceID();

    GPUSphere sphere = Spheres[instanceIndex];

    payload.color = float4(sphere.color, 1.0);
}