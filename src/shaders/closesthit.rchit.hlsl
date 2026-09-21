#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")] void ClosestHit(inout RayPayload payload, in HitAttributes attributes) {
    uint i = InstanceID();
    GPUSphere sphere = Spheres[i];

    uint seed = PCGHash(i * 0x9E3779B9u);
    uint seed2 = PCGHash(i * 0x1F2639A9u);

    payload.hitT = RayTCurrent();
    payload.position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(attributes.normal);
    payload.baseColor = sphere.color.xyz;
    payload.roughness = Random(seed);
    payload.metallic = 0.0f;
}