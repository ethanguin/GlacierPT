#include "raycommon.hlsli"

static const uint PIXEL_SAMPLES = 10;

[shader("raygeneration")] void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    uint seed = pixel.x + pixel.y * resolution.x;

    seed ^= FrameIndex * 0x9E3779B9u;
    seed = PCGHash(seed);

    float aspectRatio = float(resolution.x) / float(resolution.y);

    float3 rayOrigin = float3(0.0, 0.0, 5.0);

    float3 accumulatedColor = 0.0;

    for (uint sample = 0; sample < PIXEL_SAMPLES; ++sample) {
        float2 jitter = Random2(seed);

        float2 uv = (float2(pixel) + jitter) / float2(resolution);

        float2 ndc = uv * 2.0 - 1.0;

        float3 rayDirection = normalize(float3(ndc.x * aspectRatio, -ndc.y, -1.0));

        RayPayload payload;
        payload.color = float4(0.0, 0.0, 0.0, 1.0);

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = rayDirection;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        accumulatedColor += payload.color.rgb;
    }

    accumulatedColor /= float(PIXEL_SAMPLES);

    Output[pixel] = float4(accumulatedColor, 1.0);
}