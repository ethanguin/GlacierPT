#include "raycommon.hlsli"

struct HitAttributes {
    float3 normal;
};

[shader("closesthit")] void ClosestHit(inout RayPayload payload, in HitAttributes attributes) {
    uint instanceIndex = InstanceID();

    GPUSphere sphere = Spheres[instanceIndex];

    Camera cam = GetCamera();
    DirectionalLight dirLight = GetDirectionalLight();
    AmbientLight ambientLight = GetAmbientLight();

    // Surface

    float3 N = normalize(attributes.normal);

    float3 hitPosition = ObjectRayOrigin() + RayTCurrent() * ObjectRayDirection();

    float3 V = normalize(cam.position - hitPosition);

    float3 L = normalize(-dirLight.direction);

    float3 H = normalize(V + L);

    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    // Material

    float3 baseColor = sphere.color.xyz;

    // Temporary test values.
    uint roughnessSeed = PCGHash(instanceIndex * 0x9E3779B9u);
    uint metallicSeed = PCGHash(instanceIndex * 0x85EBCA6Bu);

    float roughness = Random(roughnessSeed);
    float metallic = 0.0f; // Random(metallicSeed) < 0.5 ? 0.0 : 1.0;

    // Metalness workflow

    // Dielectric reflectance is approximately 4%.
    // Metals use their base color as the specular F0.
    float3 dielectricF0 = float3(0.04, 0.04, 0.04);

    float3 F0 = lerp(dielectricF0, baseColor, metallic);

    // GGX Normal Distribution Function
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float denom = NoH * NoH * (alpha2 - 1.0) + 1.0;

    float D = alpha2 / (3.14159265 * denom * denom);

    // Schlick Fresnel
    float3 F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);

    // Smith Geometry
    float k = alpha * 0.5;

    float G_V = NoV / (NoV * (1.0 - k) + k);

    float G_L = NoL / (NoL * (1.0 - k) + k);

    float G = G_V * G_L;

    // Specular
    float3 specular = (D * G * F) / max(4.0 * NoV * NoL, 0.001);

    // Diffuse
    // Metallic surfaces have no diffuse component.
    float3 diffuse = (1.0 - F) * (1.0 - metallic) * baseColor / 3.14159265;

    ShadowPayload shadowPayload;
    shadowPayload.isShadowed = true;

    RayDesc shadowRay;
    shadowRay.Origin = hitPosition + N * 0.001;
    shadowRay.Direction = L;
    shadowRay.TMin = 0.001;
    shadowRay.TMax = 1000.0;

    uint shadowRayFlags = RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    TraceRay(Scene, shadowRayFlags, 0xFF, 0, 0, 1, shadowRay, shadowPayload);

    float shadowFactor = shadowPayload.isShadowed ? 0.0 : 1.0;

    float3 directLighting = (diffuse + specular) * dirLight.color * dirLight.intensity * NoL * shadowFactor;

    // Ambient Lighting
    float3 ambientLighting = diffuse * ambientLight.color * ambientLight.intensity;

    // Combined Lighting
    float3 lighting = directLighting + ambientLighting;

    payload.color = float4(lighting, 1.0);
    return;
}