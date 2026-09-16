#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")]
void ClosestHit(
    inout RayPayload payload,
    in HitAttributes attributes) {
    payload.color = float4(1.0, 1.0, 1.0, 1.0);
}