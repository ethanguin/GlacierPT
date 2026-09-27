#include "../src/shaders/raycommon.hlsli"

static const uint PIXEL_SAMPLES = 200;
static const uint SECONDARY_SAMPLES = 200;

struct HitShading {
    float3 lighting; // direct + ambient, no randomness involved
    float3 F0;
    float3 F_env;
    float specProb;
    float alpha;
};

// Deterministic given the hit: direct lighting (hard shadow rays, no sampling)
// plus the Fresnel/roughness terms needed to later sample a bounce.
HitShading EvaluateHit(RayPayload payload, float3 V) {
    HitShading s;
    float3 N = payload.normal;
    float NoV = saturate(dot(N, V));

    float3 direct = 0.0;
    for (uint lightIdx = 0; lightIdx < frameConstants.LightCount; ++lightIdx) {
        GPULight light = Lights[lightIdx];

        float3 L;
        float attenuation = 1.0;
        float shadowMaxT = 1000.0;

        if (light.type == LIGHT_TYPE_DIRECTIONAL) {
            L = normalize(-light.direction);
        } else {
            float3 toLight = light.position - payload.position;
            float dist = length(toLight);
            L = toLight / max(dist, 0.0001);
            shadowMaxT = dist;

            attenuation = saturate(1.0 - (dist * dist) / max(light.range * light.range, 0.0001));
            attenuation *= attenuation;

            if (light.type == LIGHT_TYPE_SPOT) {
                float cosAngle = dot(-L, normalize(light.direction));
                attenuation *= smoothstep(0.5, 0.9, cosAngle);
            }
        }

        float NoL = saturate(dot(N, L));
        if (NoL <= 0.0 || attenuation <= 0.0) {
            continue;
        }

        ShadowPayload shadowPayload;
        shadowPayload.isShadowed = true;

        RayDesc shadowRay;
        shadowRay.Origin = payload.position + N * 0.001;
        shadowRay.Direction = L;
        shadowRay.TMin = 0.001;
        shadowRay.TMax = shadowMaxT;

        uint shadowRayFlags = RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        TraceRay(Scene, shadowRayFlags, 0xFF, 0, 0, 1, shadowRay, shadowPayload);

        float visibility = shadowPayload.isShadowed ? 0.0 : 1.0;

        direct += EvaluateDirectLighting(N, V, L, payload.baseColor, payload.roughness, payload.metallic) * light.color.rgb * light.color.a * NoL *
                  visibility * attenuation;
    }

    s.F0 = lerp(float3(0.04, 0.04, 0.04), payload.baseColor, payload.metallic);
    s.F_env = FresnelSchlick(s.F0, NoV);

    float3 kD = (1.0 - s.F_env) * (1.0 - payload.metallic);
    float3 ambient = kD * payload.baseColor / PI * AmbientLight.color.rgb * AmbientLight.color.a;

    s.lighting = direct + ambient;

    float roughness = max(payload.roughness, MIN_ROUGHNESS);
    s.alpha = roughness * roughness;
    s.specProb = clamp(dot(s.F_env, float3(0.2126, 0.7152, 0.0722)), 0.05, 0.95);

    return s;
}

struct BounceResult {
    float3 direction;
    float3 weight;
    bool valid;
};

// Stochastic: samples one lobe (specular GGX-VNDF or cosine diffuse) using
// the deterministic terms already computed in EvaluateHit.
BounceResult SampleBounce(RayPayload payload, float3 V, HitShading s, inout uint seed) {
    BounceResult result;
    result.valid = true;

    float3 N = payload.normal;
    float NoV = saturate(dot(N, V));

    float3 T, B;
    BuildONB(N, T, B);
    float3 Vlocal = float3(dot(V, T), dot(V, B), dot(V, N));

    if (Random(seed) < s.specProb) {
        float3 Hlocal = SampleGGXVNDF(Vlocal, s.alpha, Random2(seed));
        float3 H = Hlocal.x * T + Hlocal.y * B + Hlocal.z * N;

        float3 L = 2.0 * dot(V, H) * H - V;
        float NoL = dot(N, L);

        if (NoL <= 0.0) {
            result.valid = false;
            return result;
        }

        float G1 = SmithG1GGX(NoV, s.alpha);
        float G2 = SmithG2GGX(NoV, NoL, s.alpha);
        float3 F = FresnelSchlick(s.F0, saturate(dot(V, H)));

        result.weight = (F * (G2 / max(G1, 0.001))) / s.specProb;
        result.direction = L;
    } else {
        float3 dirLocal = SampleCosineHemisphere(Random2(seed));
        result.direction = dirLocal.x * T + dirLocal.y * B + dirLocal.z * N;
        result.weight = ((1.0 - s.F_env) * (1.0 - payload.metallic) * payload.baseColor) / (1.0 - s.specProb);
    }

    return result;
}

[shader("raygeneration")] void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    float3 camPos = CameraData.position;
    float3 camForward = normalize(CameraData.forward);

    float aspectRatio = float(resolution.x) / float(resolution.y);
    float horizontalFov = 2.0 * atan(CameraData.sensorWidth / (2.0 * CameraData.focalLength));
    float verticalFov = 2.0 * atan(tan(horizontalFov * 0.5) / aspectRatio);
    float halfWidth = tan(horizontalFov * 0.5);
    float halfHeight = tan(verticalFov * 0.5);

    float3 worldUp = float3(0.0, 1.0, 0.0);
    float3 camRight = normalize(cross(camForward, worldUp));
    float3 camUp = cross(camRight, camForward);

    uint seed = pixel.x + pixel.y * resolution.x;
    seed ^= frameConstants.FrameIndex * 0x9E3779B9u;
    seed = PCGHash(seed);

    float2 uv = (float2(pixel) + 0.5) / float2(resolution); // single primary ray, no per-sample jitter needed
    float2 ndc = uv * 2.0 - 1.0;
    float3 primaryDir = normalize(camForward + camRight * (ndc.x * halfWidth) + camUp * (ndc.y * halfHeight));

    RayDesc primaryRay;
    primaryRay.Origin = camPos;
    primaryRay.Direction = primaryDir;
    primaryRay.TMin = 0.001;
    primaryRay.TMax = 1000.0;

    RayPayload primaryPayload = (RayPayload)0;
    primaryPayload.hitT = -1.0;

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, primaryRay, primaryPayload);

    float3 finalColor;

    if (primaryPayload.hitT < 0.0) {
        finalColor = GetSkyColor(primaryRay.Direction);
    } else {
        float3 V0 = -primaryRay.Direction;
        HitShading primaryShading = EvaluateHit(primaryPayload, V0);

        float3 accumulated = 0.0;

        for (uint sampleIdx = 0; sampleIdx < SECONDARY_SAMPLES; ++sampleIdx) {
            float3 radiance = primaryShading.lighting;

            BounceResult bounce = SampleBounce(primaryPayload, V0, primaryShading, seed);

            if (!bounce.valid) {
                accumulated += radiance;
                continue;
            }

            float3 throughput = bounce.weight;

            RayDesc ray;
            ray.Origin = primaryPayload.position + primaryPayload.normal * 0.001;
            ray.Direction = bounce.direction;
            ray.TMin = 0.001;
            ray.TMax = 1000.0;

            bool pathEnded = false;

            // MAX_BOUNCES total ray segments after the primary, matching the original path length.
            for (uint b = 1; b <= MAX_BOUNCES; ++b) {
                RayPayload payload = (RayPayload)0;
                payload.hitT = -1.0;

                TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

                if (payload.hitT < 0.0) {
                    radiance += throughput * GetSkyColor(ray.Direction);
                    pathEnded = true;
                    break;
                }

                float3 V = -ray.Direction;
                HitShading shading = EvaluateHit(payload, V);

                radiance += throughput * shading.lighting;

                BounceResult next = SampleBounce(payload, V, shading, seed);

                if (!next.valid) {
                    pathEnded = true;
                    break;
                }

                throughput *= next.weight;

                if (max(throughput.r, max(throughput.g, throughput.b)) < 0.01) {
                    pathEnded = true;
                    break;
                }

                ray.Origin = payload.position + payload.normal * 0.001;
                ray.Direction = next.direction;
            }

            if (!pathEnded) {
                radiance += throughput * GetSkyColor(ray.Direction);
            }

            accumulated += radiance;
        }

        finalColor = accumulated / float(SECONDARY_SAMPLES);
    }

    Output[pixel] = float4(finalColor, 1.0);
}