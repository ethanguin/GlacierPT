#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")] void ClosestHit(inout RayPayload payload, in HitAttributes attributes) {
    uint instanceIndex = InstanceID();
    GPUSphere sphere = Spheres[instanceIndex];

    // Lighting Math

    payload.color = sphere.color;
};