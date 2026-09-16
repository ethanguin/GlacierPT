#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("intersection")]
void Intersection() {
    HitAttributes attributes;
    attributes.normal = float3(0.0, 1.0, 0.0);

    ReportHit(0.0f, 0U, attributes);
}