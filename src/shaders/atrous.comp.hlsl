[[vk::binding(0, 0)]] RWTexture2D<float4> Src;  // rgb = radiance, a = variance
[[vk::binding(1, 0)]] RWTexture2D<float4> Dst;
[[vk::binding(2, 0)]] RWTexture2D<float4> GuidePos;
[[vk::binding(3, 0)]] RWTexture2D<float4> GuideNormal;

struct Params { int stepSize; };
[[vk::push_constant]] ConstantBuffer<Params> pc;

static const float KERNEL[3] = {3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0}; // B3 spline: center, +-1, +-2
static const float SIGMA_L = 3.0;     // luminance edge-stop, in std-devs
static const float SIGMA_N = 16.0;    // normal exponent
static const float SIGMA_P = 0.25;    // plane distance tolerance, world units (tune to scene scale)

float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    uint2 dim;
    Src.GetDimensions(dim.x, dim.y);
    if (any(id.xy >= dim)) return;

    int2 p = int2(id.xy);
    float4 c0 = Src[p];

    // Clean pixel: pass through.
    if (c0.a <= 0.0) { Dst[p] = c0; return; }

    float3 n0 = GuideNormal[p].xyz;
    float3 pos0 = GuidePos[p].xyz;
    float l0 = Luma(c0.rgb);
    float sigmaL = SIGMA_L * sqrt(c0.a) + 1e-4;

    float3 colorSum = c0.rgb * KERNEL[0] * KERNEL[0];
    float varSum = c0.a * pow(KERNEL[0] * KERNEL[0], 2);
    float wSum = KERNEL[0] * KERNEL[0];

    [unroll] for (int dy = -2; dy <= 2; ++dy) {
        [unroll] for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;

            int2 q = p + int2(dx, dy) * pc.stepSize;
            if (any(q < 0) || any(q >= int2(dim))) continue;

            float4 c1 = Src[q];
            float3 n1 = GuideNormal[q].xyz;
            float3 pos1 = GuidePos[q].xyz;

            float wN = pow(saturate(dot(n0, n1)), SIGMA_N);          // 0 for sky/mismatch
            float wP = exp(-abs(dot(n0, pos1 - pos0)) / SIGMA_P);     // plane distance
            float wL = exp(-abs(l0 - Luma(c1.rgb)) / sigmaL);

            float w = KERNEL[abs(dx)] * KERNEL[abs(dy)] * wN * wP * wL;

            colorSum += c1.rgb * w;
            varSum += c1.a * w * w;
            wSum += w;
        }
    }

    float invW = 1.0 / wSum;
    Dst[p] = float4(colorSum * invW, varSum * invW * invW);
}