#include "raycommon.hlsli"

[shader("raygeneration")] void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    float2 uv = (float2(pixel) + 0.5) / float2(resolution);

    float2 ndc = uv * 2.0 - 1.0;

    float aspectRatio = float(resolution.x) / float(resolution.y);

    float3 rayOrigin = float3(0.0, 0.0, 5.0);

    float3 rayDirection = normalize(float3(ndc.x * aspectRatio, -ndc.y, -1.0));

    RayPayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDirection;
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    Output[pixel] = payload.color;
}