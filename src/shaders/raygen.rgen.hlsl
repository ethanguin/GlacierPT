#include "raycommon.hlsli"

static const uint PIXEL_SAMPLES = 1;

[shader("raygeneration")] void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    Camera cam = GetCamera();
    DirectionalLight dirLight = GetDirectionalLight();
    AmbientLight ambientLight = GetAmbientLight();

    // Direction toward the light (constant for the whole frame).
    float3 L = normalize(-dirLight.direction);

    float aspectRatio = float(resolution.x) / float(resolution.y);

    // Camera projection.
    float horizontalFov = 2.0 * atan(cam.sensorWidth / (2.0 * cam.focalLength));

    float verticalFov = 2.0 * atan(tan(horizontalFov * 0.5) / aspectRatio);

    float halfWidth = tan(horizontalFov * 0.5);
    float halfHeight = tan(verticalFov * 0.5);

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

        // +X screen right
        // +Y screen up
        // -Z camera forward
        float3 rayDirection = normalize(float3(ndc.x * halfWidth, ndc.y * halfHeight, -1.0));

        // PATH LOOP

        float3 radiance = 0.0;
        float3 throughput = 1.0;
        bool pathEnded = false;

        RayDesc ray;
        ray.Origin = cam.position;
        ray.Direction = rayDirection;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        for (uint bounce = 0; bounce <= MAX_BOUNCES; ++bounce) {
            RayPayload payload = (RayPayload)0;
            payload.hitT = -1.0;

            // Miss index 0 = regular miss
            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

            // Escaped the scene: add the sky, tinted by whatever the path has picked up.
            if (payload.hitT < 0.0) {
                radiance += throughput * GetSkyColor(ray.Direction);
                pathEnded = true;
                break;
            }

            float3 N = payload.normal;
            float3 V = -ray.Direction;
            float NoV = saturate(dot(N, V));
            float NoL = saturate(dot(N, L));

            // Direct light + shadow

            float visibility = 0.0;

            if (NoL > 0.0) {
                ShadowPayload shadowPayload;
                shadowPayload.isShadowed = true;

                RayDesc shadowRay;
                shadowRay.Origin = payload.position + N * 0.001;
                shadowRay.Direction = L;
                shadowRay.TMin = 0.001;
                shadowRay.TMax = 1000.0;

                uint shadowRayFlags = RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

                // Miss index 1 = shadow miss
                TraceRay(Scene, shadowRayFlags, 0xFF, 0, 0, 1, shadowRay, shadowPayload);

                visibility = shadowPayload.isShadowed ? 0.0 : 1.0;
            }

            float3 direct = EvaluateDirectLighting(N, V, L, payload.baseColor, payload.roughness, payload.metallic) * dirLight.color *
                            dirLight.intensity * NoL * visibility;

            // Ambient

            float3 F0 = lerp(float3(0.04, 0.04, 0.04), payload.baseColor, payload.metallic);
            float3 F_env = FresnelSchlick(F0, NoV);

            // Energy sent into the reflection isn't also counted as diffuse.
            float3 kD = (1.0 - F_env) * (1.0 - payload.metallic);
            float3 ambient = kD * payload.baseColor / PI * ambientLight.color * ambientLight.intensity;

            radiance += throughput * (direct + ambient);

            // Continue along the mirror direction

            // Stopgap until GGX sampling: fade the mirror ray out as roughness rises.
            float rough = max(payload.roughness, MIN_ROUGHNESS);
            float fade = (1.0 - rough) * (1.0 - rough);

            throughput *= F_env * fade;

            // Nothing left worth tracing.
            if (max(throughput.r, max(throughput.g, throughput.b)) < 0.01) {
                pathEnded = true;
                break;
            }

            ray.Origin = payload.position + N * 0.001;
            ray.Direction = reflect(ray.Direction, N);
        }

        // Ran out of bounces while still on a live path: fall back to the sky
        // (same as the recursive version did) instead of dropping the energy.
        if (!pathEnded) {
            radiance += throughput * GetSkyColor(ray.Direction);
        }

        accumulatedColor += radiance;
    }

    accumulatedColor /= float(totalSamples);

    Output[pixel] = float4(accumulatedColor, 1.0);
}