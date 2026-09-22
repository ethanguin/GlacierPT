#include "raycommon.hlsli"

static const uint PIXEL_SAMPLES = 1;

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

    // Basis relative to camForward, so the camera can point anywhere
    // (not just -Z). world up = (0,1,0).
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

        float3 radiance = 0.0;
        float3 throughput = 1.0;
        bool pathEnded = false;

        RayDesc ray;
        ray.Origin = camPos;
        ray.Direction = rayDirection;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        for (uint bounce = 0; bounce <= MAX_BOUNCES; ++bounce) {
            RayPayload payload = (RayPayload)0;
            payload.hitT = -1.0;

            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

            if (payload.hitT < 0.0) {
                radiance += throughput * GetSkyColor(ray.Direction);
                pathEnded = true;
                break;
            }

            float3 N = payload.normal;
            float3 V = -ray.Direction;
            float NoV = saturate(dot(N, V));

            // Direct lighting

            float3 direct = 0.0;

            // Loop through each light
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

                direct += EvaluateDirectLighting(N, V, L, payload.baseColor, payload.roughness, payload.metallic) * light.color.rgb * light.color.a *
                          NoL * visibility * attenuation;
            }

            // Ambient

            float3 F0 = lerp(float3(0.04, 0.04, 0.04), payload.baseColor, payload.metallic);
            float3 F_env = FresnelSchlick(F0, NoV);
            float3 kD = (1.0 - F_env) * (1.0 - payload.metallic);
            float3 ambient = kD * payload.baseColor / PI * AmbientLight.color.rgb * AmbientLight.color.a;

            radiance += throughput * (direct + ambient);

            float rough = max(payload.roughness, MIN_ROUGHNESS);
            float fade = (1.0 - rough) * (1.0 - rough);

            throughput *= F_env * fade;

            if (max(throughput.r, max(throughput.g, throughput.b)) < 0.01) {
                pathEnded = true;
                break;
            }

            ray.Origin = payload.position + N * 0.001;
            ray.Direction = reflect(ray.Direction, N);
        }

        if (!pathEnded) {
            radiance += throughput * GetSkyColor(ray.Direction);
        }

        accumulatedColor += radiance;
    }

    accumulatedColor /= float(totalSamples);

    Output[pixel] = float4(accumulatedColor, 1.0);
}