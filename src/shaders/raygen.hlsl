#include "raycommon.hlsli"

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;

    Output[pixel] = float4(1.0, 0.0, 1.0, 1.0);
}