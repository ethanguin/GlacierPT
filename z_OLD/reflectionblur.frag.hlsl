[[vk::binding(0, 0)]]
Texture2D<float4> ReflectionInput; // rgb = mirror radiance, a = roughness

[[vk::binding(1, 0)]]
Texture2D<float4> DirectInput; // .a = primary hit distance (depth proxy)

[[vk::binding(2, 0)]]
SamplerState LinearSampler;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float PI2 = 6.28318530718;
static const int TAP_COUNT = 12;
static const float MAX_BLUR_RADIUS_PIXELS = 24.0;
static const float DEPTH_REJECT_SIGMA = 0.15;

float4 PSMain(VertexOutput input) : SV_Target {
    uint width, height;
    ReflectionInput.GetDimensions(width, height);
    float2 texel = 1.0 / float2(width, height);

    float4 centerRefl = ReflectionInput.SampleLevel(LinearSampler, input.uv, 0);
    float centerDepth = DirectInput.SampleLevel(LinearSampler, input.uv, 0).a;

    float roughness = centerRefl.a;

    if (roughness <= 0.02 || centerDepth <= 0.0) {
        return float4(centerRefl.rgb, 1.0);
    }

    // roughness^2 ramp roughly matches how a GGX alpha term widens the lobe.
    float radiusPixels = MAX_BLUR_RADIUS_PIXELS * (roughness * roughness);

    float3 accumulated = centerRefl.rgb;
    float totalWeight = 1.0;

    float angleOffset = frac(sin(dot(input.position.xy, float2(12.9898, 78.233))) * 43758.5453) * PI2;

    for (int i = 0; i < TAP_COUNT; ++i) {
        float t = (float(i) + 0.5) / float(TAP_COUNT);
        float angle = angleOffset + t * PI2 * 3.0;
        float radius = radiusPixels * sqrt(t); // uniform disk density

        float2 sampleUv = input.uv + float2(cos(angle), sin(angle)) * radius * texel;

        float4 tapRefl = ReflectionInput.SampleLevel(LinearSampler, sampleUv, 0);
        float tapDepth = DirectInput.SampleLevel(LinearSampler, sampleUv, 0).a;

        if (tapDepth <= 0.0) {
            continue;
        }

        float depthDiff = abs(tapDepth - centerDepth) / max(centerDepth, 0.001);
        float weight = exp(-(depthDiff * depthDiff) / (2.0 * DEPTH_REJECT_SIGMA * DEPTH_REJECT_SIGMA));

        accumulated += tapRefl.rgb * weight;
        totalWeight += weight;
    }

    return float4(accumulated / max(totalWeight, 0.001), 1.0);
}