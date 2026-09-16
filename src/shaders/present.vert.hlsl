struct VertexOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VertexOutput VSMain(uint vertexID : SV_VertexID) {
    VertexOutput output;

    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 uvs[3] = {
        float2(0.0, 1.0),
        float2(2.0, 1.0),
        float2(0.0, -1.0)
    };

    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.uv = uvs[vertexID];

    return output;
}