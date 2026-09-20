// Presentation pass fragment shader.
//
// Samples the linear HDR ray-traced image and applies FXAA 3.11 (Quality).
// The swapchain is *_SRGB, so this shader outputs LINEAR color and the
// hardware performs the sRGB encode on write (same as before).
//
// FXAA design notes for this renderer:
//   * Edge DETECTION is done on perceptual luma: luma(sRGB(ToLDR(color))).
//   * The final BLEND is the hardware bilinear filter on the linear data,
//     i.e. gamma-correct coverage blending.
//   * ToLDR() is where a tonemapper goes once you have one.

#define FXAA_ENABLED 1

// ---- Tunables -------------------------------------------------------------
// Absolute luma contrast below which a pixel is never touched (dark areas).
//   0.0833 = fast, 0.0625 = default, 0.0312 = high quality
static const float FXAA_EDGE_THRESHOLD_MIN = 0.0312;

// Contrast relative to the local max luma needed to count as an edge.
//   0.250 = fast, 0.166 = default, 0.125 = high quality, 0.063 = ultra
static const float FXAA_EDGE_THRESHOLD = 0.063;

// Amount of sub-pixel aliasing removal (thin lines, isolated bright pixels).
//   0.0 = off, 0.75 = default, 1.0 = softest
static const float FXAA_SUBPIX_QUALITY = 1.0;

// Max steps when walking along an edge (see QualityStep for the step sizes).
static const int FXAA_ITERATIONS = 12;

// Multiplier for the bilinear sample
static const float FXAA_EDGE_BLUR = 1;

// ---- Bindings -------------------------------------------------------------
[[vk::binding(0, 0)]]
Texture2D<float4> InputTexture;

[[vk::binding(1, 0)]]
SamplerState InputSampler;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// ---- Helpers --------------------------------------------------------------

// Placeholder "tonemap": clamp. Replace with a real tonemapper later.
float3 ToLDR(float3 hdr) {
    return saturate(hdr);
}

float3 LinearToSRGB(float3 c) {
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, 0.0031308), 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}

float Luma(float3 srgb) {
    return dot(srgb, float3(0.299, 0.587, 0.114));
}

// SampleLevel (not Sample) so it's legal inside divergent control flow.
float3 SampleLinear(float2 uv) {
    return InputTexture.SampleLevel(InputSampler, uv, 0).rgb;
}

float SampleLuma(float2 uv) {
    return Luma(LinearToSRGB(ToLDR(SampleLinear(uv))));
}

// Edge-search step multipliers: small steps near the pixel for accuracy,
// growing steps further out so long edges are still found in few taps.
float QualityStep(int i) {
    if (i < 5)
        return 1.0;
    if (i == 5)
        return 1.5;
    if (i < 10)
        return 2.0;
    if (i == 10)
        return 4.0;
    return 8.0;
}

// ---- FXAA 3.11 (Quality) --------------------------------------------------
// Returns LINEAR color. `texel` is 1 / (size of the input texture).
// "Up/Down/Left/Right" are in UV space; the algorithm is orientation-agnostic.
float3 FXAA(float2 uv, float2 texel) {
    float3 colorCenter = ToLDR(SampleLinear(uv));
    float lumaCenter = Luma(LinearToSRGB(colorCenter));

    float lumaDown = SampleLuma(uv + float2(0, -1) * texel);
    float lumaUp = SampleLuma(uv + float2(0, 1) * texel);
    float lumaLeft = SampleLuma(uv + float2(-1, 0) * texel);
    float lumaRight = SampleLuma(uv + float2(1, 0) * texel);

    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    // Not an edge: leave the pixel alone.
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD)) {
        return colorCenter;
    }

    // Corners
    float lumaDownLeft = SampleLuma(uv + float2(-1, -1) * texel);
    float lumaUpRight = SampleLuma(uv + float2(1, 1) * texel);
    float lumaUpLeft = SampleLuma(uv + float2(-1, 1) * texel);
    float lumaDownRight = SampleLuma(uv + float2(1, -1) * texel);

    float lumaDownUp = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;
    float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
    float lumaDownCorners = lumaDownLeft + lumaDownRight;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners = lumaUpRight + lumaUpLeft;

    // Is the local edge horizontal or vertical? (3x3 second-derivative estimate)
    float edgeHorizontal =
        abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 + abs(-2.0 * lumaRight + lumaRightCorners);

    float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + abs(-2.0 * lumaDown + lumaDownCorners);

    bool isHorizontal = edgeHorizontal >= edgeVertical;

    // The two neighbours across the edge; the edge lies toward the steeper one.
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;

    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;

    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    // Step (perpendicular to the edge) toward the steepest side.
    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumaLocalAverage;

    if (is1Steepest) {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    } else {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }

    // Move to the edge boundary (half a texel toward the steep side).
    float2 currentUv = uv;
    if (isHorizontal) {
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }

    // Walk along the edge in both directions.
    float2 offset = isHorizontal ? float2(texel.x, 0.0) : float2(0.0, texel.y);

    float2 uv1 = currentUv - offset;
    float2 uv2 = currentUv + offset;

    float lumaEnd1 = SampleLuma(uv1) - lumaLocalAverage;
    float lumaEnd2 = SampleLuma(uv2) - lumaLocalAverage;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;

    if (!reached1)
        uv1 -= offset;
    if (!reached2)
        uv2 += offset;

    if (!(reached1 && reached2)) {
        [loop] for (int i = 2; i < FXAA_ITERATIONS; i++) {
            if (!reached1)
                lumaEnd1 = SampleLuma(uv1) - lumaLocalAverage;
            if (!reached2)
                lumaEnd2 = SampleLuma(uv2) - lumaLocalAverage;

            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;

            if (reached1 && reached2)
                break;

            float s = QualityStep(i);
            if (!reached1)
                uv1 -= offset * s;
            if (!reached2)
                uv2 += offset * s;
        }
    }

    // Distances from this pixel to both ends of the edge.
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;

    // 0.5 at the middle of the edge, approaching 0 at its ends.
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;

    // Only apply the offset if the nearest end's luma variation is consistent
    // with this pixel being on the wrong side of the edge.
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;

    float finalOffset = correctVariation ? pixelOffset * FXAA_EDGE_BLUR : 0.0;

    // Sub-pixel aliasing: compare the pixel against its 3x3 weighted average.
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);

    float subPixelOffset1 = saturate(abs(lumaAverage - lumaCenter) / lumaRange);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * FXAA_SUBPIX_QUALITY;

    finalOffset = max(finalOffset, subPixelOffsetFinal);

    // Re-sample shifted perpendicular to the edge; the bilinear filter does the blend.
    float2 finalUv = uv;
    if (isHorizontal) {
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }

    return ToLDR(SampleLinear(finalUv));
}

// ---- Entry point ----------------------------------------------------------
float4 PSMain(VertexOutput input) : SV_Target {
#if FXAA_ENABLED
    uint width, height;
    InputTexture.GetDimensions(width, height);

    float2 texel = 1.0 / float2(width, height);

    return float4(FXAA(input.uv, texel), 1.0);
#else
    return InputTexture.SampleLevel(InputSampler, input.uv, 0);
#endif
}
