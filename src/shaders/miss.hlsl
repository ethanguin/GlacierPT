#include "raycommon.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float4(0.0, 0.0, 0.0, 1.0);
}