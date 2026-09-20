#include "raycommon.hlsli"

[shader("miss")] void MissShadow(inout ShadowPayload payload) { payload.isShadowed = false; }