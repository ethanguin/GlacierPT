#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")] void ClosestHit(inout RayPayload payload, in HitAttributes attributes) {
    GPUSphere sphere = Spheres[InstanceID()];

    payload.hitT = RayTCurrent();
    payload.position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = normalize(attributes.normal);
    payload.geoNormal = payload.normal;
    payload.baseColor = sphere.color.xyz;
    payload.roughness = sphere.color.w;
    payload.metallic = 0.0f;
    payload.transmission = 1.0;
    payload.ior = 1.5;
    payload.emission = 0.0;
};

[shader("closesthit")] void ClosestHitMesh(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    uint meshIndex = InstanceID();
    uint primitiveIndex = PrimitiveIndex();

    GPUMesh mesh = Meshes[meshIndex];

    uint i0 = Indices[mesh.firstIndex + primitiveIndex * 3 + 0];
    uint i1 = Indices[mesh.firstIndex + primitiveIndex * 3 + 1];
    uint i2 = Indices[mesh.firstIndex + primitiveIndex * 3 + 2];

    GPUVertex v0 = Vertices[mesh.firstVertex + i0];
    GPUVertex v1 = Vertices[mesh.firstVertex + i1];
    GPUVertex v2 = Vertices[mesh.firstVertex + i2];

    float3 bary = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    float2 uv0 = float2(v0.u, v0.v);
    float2 uv1 = float2(v1.u, v1.v);
    float2 uv2 = float2(v2.u, v2.v);
    float2 uv = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;

    GPUMaterial material = Materials[mesh.materialIndex];

    // Interpolated vertex normal (object space)
    float3 N = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);

    // assumes uniform transforms
    float3x3 normalMatrix = (float3x3)ObjectToWorld3x4();
    float3 vertexNormalWorld = normalize(mul(normalMatrix, N));

    // Geometric normal, oriented to the same hemisphere as the vertex normal. Used for inside/outside tests
    // so a normal map can never flip a surface from "entering" to "exiting".
    float3 geoNormal = normalize(mul(normalMatrix, cross(v1.position - v0.position, v2.position - v0.position)));
    if (dot(geoNormal, vertexNormalWorld) < 0.0) {
        geoNormal = -geoNormal;
    }

    // ---- Normal map ---------------------------------------------------------
    // Tangent frame from the triangle's UV derivatives (no tangent attribute needed).
    // glTF UV v points DOWN the image while normal-map green points UP, hence the flip of the v-direction.
    float3 shadingNormal = vertexNormalWorld;

    if (material.normalTexture >= 0) {
        float3 dp1 = v1.position - v0.position;
        float3 dp2 = v2.position - v0.position;
        float2 duv1 = uv1 - uv0;
        float2 duv2 = uv2 - uv0;

        float det = duv1.x * duv2.y - duv2.x * duv1.y;

        if (abs(det) > 1e-12) {
            float r = 1.0 / det;
            float3 T = (dp1 * duv2.y - dp2 * duv1.y) * r;      // d(pos)/du
            float3 Bv = (dp2 * duv1.x - dp1 * duv2.x) * r;     // d(pos)/dv (points down the image)

            T = T - N * dot(N, T);

            if (dot(T, T) > 1e-20) {
                T = normalize(T);
                float3 B = cross(N, T);
                if (dot(B, -Bv) < 0.0) {
                    B = -B; // mirrored UVs
                }

                float3 tn = SampleTexture(material.normalTexture, uv).xyz * 2.0 - 1.0;
                tn.xy *= material.normalScale;

                float3 perturbed = normalize(T * tn.x + B * tn.y + N * tn.z);
                shadingNormal = normalize(mul(normalMatrix, perturbed));
            }
        }
    }

    // ---- Material textures --------------------------------------------------
    float4 baseColor = material.baseColor;
    if (material.baseColorTexture >= 0) {
        baseColor *= SampleTexture(material.baseColorTexture, uv);
    }

    float roughness = material.roughness;
    float metallic = material.metallic;
    if (material.metallicRoughnessTexture >= 0) {
        float3 mr = SampleTexture(material.metallicRoughnessTexture, uv).rgb;
        roughness *= mr.g;
        metallic *= mr.b;
    }

    float3 emission = material.emissive;
    if (material.emissiveTexture >= 0) {
        emission *= SampleTexture(material.emissiveTexture, uv).rgb;
    }

    payload.hitT = RayTCurrent();
    payload.position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    payload.normal = shadingNormal;
    payload.geoNormal = geoNormal;
    payload.baseColor = baseColor.rgb;
    payload.roughness = roughness;
    payload.metallic = metallic;
    payload.transmission = material.transmission;
    payload.ior = material.ior;
    payload.emission = emission;
}
