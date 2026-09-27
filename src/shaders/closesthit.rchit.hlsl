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
};

[shader("closesthit")] void ClosestHitMesh(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    uint meshIndex = InstanceID();
    uint primitiveIndex = PrimitiveIndex();

    GPUMesh mesh = Meshes[meshIndex];

    // index into global index for each mesh
    uint i0 = Indices[mesh.firstIndex + primitiveIndex * 3 + 0];
    uint i1 = Indices[mesh.firstIndex + primitiveIndex * 3 + 1];
    uint i2 = Indices[mesh.firstIndex + primitiveIndex * 3 + 2];

    GPUVertex v0 = Vertices[mesh.firstVertex + i0];
    GPUVertex v1 = Vertices[mesh.firstVertex + i1];
    GPUVertex v2 = Vertices[mesh.firstVertex + i2];

    float3 bary = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    float3 localNormal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;

    // assumes uniform transforms
    float3x3 normalMatrix = (float3x3)ObjectToWorld3x4();
    float3 worldNormal = normalize(mul(normalMatrix, localNormal));

    GPUMaterial material = Materials[mesh.materialIndex];

    payload.hitT = RayTCurrent();
    payload.position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = worldNormal;
    payload.baseColor = material.baseColor.rgb;
    payload.roughness = material.roughness;
    payload.metallic = material.metallic;
}