#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("intersection")] void Intersection() {
    uint instanceIndex = InstanceID();

    GPUSphere sphere = Spheres[instanceIndex];

    float3 rayOrigin = ObjectRayOrigin();
    float3 rayDirection = ObjectRayDirection();

    float3 oc = rayOrigin - sphere.position;

    float a = dot(rayDirection, rayDirection);
    float b = 2.0 * dot(oc, rayDirection);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0) {
        return;
    }

    float sqrtDiscriminant = sqrt(discriminant);

    float t0 = (-b - sqrtDiscriminant) / (2.0 * a);
    float t1 = (-b + sqrtDiscriminant) / (2.0 * a);

    float hitT = t0;

    if (hitT < 0.001) {
        hitT = t1;
    }

    if (hitT >= 0.001) {
        float3 hitPosition = rayOrigin + rayDirection * hitT;

        HitAttributes attributes;
        attributes.normal = normalize(hitPosition - sphere.position);

        ReportHit(hitT, 0U, attributes);
    }
}