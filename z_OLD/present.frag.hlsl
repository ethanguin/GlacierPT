[[vk::binding(0, 0)]]
Texture2D<float4> InputTexture;

[[vk::binding(1, 0)]]
SamplerState InputSampler;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VertexOutput input) : SV_Target {
    return InputTexture.Sample(InputSampler, input.uv);
}
