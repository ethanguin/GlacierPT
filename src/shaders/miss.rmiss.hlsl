#include "raycommon.hlsli"

[shader("miss")] void Miss(inout RayPayload payload) { payload.hitT = -1.0; }