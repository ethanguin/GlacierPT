#include "raycommon.hlsli"

static const uint PIXEL_SAMPLES = 1;

// One pending ray in the (depth-first) ray tree. Glass splits a hit into a
// reflected and a refracted ray; we keep going with one and stack the other.
struct PathRay {
    float3 origin;
    float3 direction;
    float3 throughput;
    uint depth;
};

float MaxComponent(float3 v) {
    return max(v.x, max(v.y, v.z));
}

// Direct lighting (all lights, hard shadows). N must face the viewer.
float3 EvaluateLights(RayPayload payload, float3 N, float3 V) {
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

    return direct;
}

// opaque surfaces get direct + ambient + mirror bounce,
// transmissive surfaces split into Fresnel-weighted reflection + refraction.
float3 TracePath(float3 origin, float3 direction) {
    PathRay stack[RAY_STACK_SIZE];
    uint sp = 0;

    PathRay cur;
    cur.origin = origin;
    cur.direction = direction;
    cur.throughput = 1.0;
    cur.depth = 0;

    float3 radiance = 0.0;
    uint raysTraced = 0;

    [loop] while (true) {
        bool alive = true;

        // Out of depth/ray budget: approximate the remainder with the sky.
        if (cur.depth > MAX_BOUNCES || raysTraced >= MAX_RAYS_PER_SAMPLE) {
            radiance += cur.throughput * GetSkyColor(cur.direction);
            alive = false;
        }

        if (alive) {
            RayDesc ray;
            ray.Origin = cur.origin;
            ray.Direction = cur.direction;
            ray.TMin = 0.001;
            ray.TMax = 1000.0;

            RayPayload payload = (RayPayload)0;
            payload.hitT = -1.0;

            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
            raysTraced++;

            if (payload.hitT < 0.0) {
                radiance += cur.throughput * GetSkyColor(cur.direction);
                alive = false;
            } else {
                float T = saturate(payload.transmission);
                float3 V = -cur.direction;

                // Flip the normal to face the incoming ray; remember which side we hit.
                bool inside = dot(cur.direction, payload.normal) > 0.0;
                float3 N = inside ? -payload.normal : payload.normal;
                float NoV = saturate(dot(N, V));

                // Beer-Lambert: the segment we just travelled was inside the glass.
                if (inside && T > 0.0) {
                    float3 sigma = -log(max(payload.baseColor, 0.001));
                    cur.throughput *= exp(-sigma * payload.hitT * ABSORPTION_SCALE);
                }

                // Opaque portion: direct + ambient

                float3 direct = 0.0;
                if (T < 1.0) {
                    direct = EvaluateLights(payload, N, V);
                }

                float3 F0 = lerp(float3(0.04, 0.04, 0.04), payload.baseColor, payload.metallic);
                float3 F_env = FresnelSchlick(F0, NoV);
                float3 kD = (1.0 - F_env) * (1.0 - payload.metallic);
                float3 ambient = kD * payload.baseColor / PI * AmbientLight.color.rgb * AmbientLight.color.a;

                radiance += cur.throughput * (1.0 - T) * (direct + ambient);

                // Continuation weights

                float rough = max(payload.roughness, MIN_ROUGHNESS);
                float fade = (1.0 - rough) * (1.0 - rough);

                // Opaque mirror term (previous behaviour, scaled by the opaque fraction).
                float3 reflectWeight = (1.0 - T) * F_env * fade;
                float3 refractWeight = 0.0;
                float3 refractDir = cur.direction;

                if (T > 0.0) {
                    float etaI = inside ? payload.ior : 1.0;
                    float etaT = inside ? 1.0 : payload.ior;

                    float F = FresnelDielectric(NoV, etaI, etaT);

                    reflectWeight += T * F;

                    if (F < 1.0) { // F == 1 means total internal reflection
                        refractDir = normalize(refract(cur.direction, N, etaI / etaT));
                        refractWeight = T * (1.0 - F);
                    }
                }

                PathRay reflected;
                reflected.origin = payload.position + N * 0.001;
                reflected.direction = reflect(cur.direction, N);
                reflected.throughput = cur.throughput * reflectWeight;
                reflected.depth = cur.depth + 1;

                PathRay refracted;
                refracted.origin = payload.position - N * 0.001;
                refracted.direction = refractDir;
                refracted.throughput = cur.throughput * refractWeight;
                refracted.depth = cur.depth + 1;

                bool hasReflect = MaxComponent(reflected.throughput) >= MIN_THROUGHPUT;
                bool hasRefract = MaxComponent(refracted.throughput) >= MIN_THROUGHPUT;

                if (hasRefract && hasReflect) {
                    if (sp < RAY_STACK_SIZE) {
                        stack[sp] = reflected;
                        sp++;
                    }
                    cur = refracted;
                } else if (hasRefract) {
                    cur = refracted;
                } else if (hasReflect) {
                    cur = reflected;
                } else {
                    alive = false;
                }
            }
        }

        if (!alive) {
            if (sp == 0) {
                break;
            }
            sp--;
            cur = stack[sp];
        }
    }

    return radiance;
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

    float3 accumulatedColor = 0.0;

    uint seed = 0;
    if (PIXEL_SAMPLES > 1) {
        seed = pixel.x + pixel.y * resolution.x;
        seed ^= 0x9E3779B9u;
        seed = PCGHash(seed);
    }

    uint totalSamples = PIXEL_SAMPLES * PIXEL_SAMPLES;

    for (uint sample = 0; sample < totalSamples; ++sample) {
        uint x = sample % PIXEL_SAMPLES;
        uint y = sample / PIXEL_SAMPLES;

        float2 randomOffset = Random2(seed);
        float2 jitter = (float2(x, y) + randomOffset) / float(PIXEL_SAMPLES);
        float2 uv = (float2(pixel) + jitter) / float2(resolution);
        float2 ndc = uv * 2.0 - 1.0;

        float3 rayDirection = normalize(camForward + camRight * (ndc.x * halfWidth) + camUp * (ndc.y * halfHeight));

        accumulatedColor += TracePath(camPos, rayDirection);
    }

    accumulatedColor /= float(totalSamples);

    Output[pixel] = float4(accumulatedColor, 1.0);
}
