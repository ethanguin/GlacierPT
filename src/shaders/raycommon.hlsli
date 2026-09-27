#ifndef RAYCOMMON
#define RAYCOMMON

static const float PI = 3.14159265;

static const uint MAX_BOUNCES = 4;

// Below this, GGX gets numerically unstable (D spikes, denom cancels to 0).
static const float MIN_ROUGHNESS = 0.08;
static const float MAX_ROUGHNESS = 0.65;

// SHARED STRUCTS

struct RayPayload {
    float3 position;
    float3 normal;
    float3 baseColor;
    float roughness;
    float metallic;
    float hitT; // < 0 means the ray missed
};

struct ShadowPayload {
    bool isShadowed;
};

struct GPUSphere {
    float4 positionRadius;
    float4 color;
};

// SHARED BUFFERS
struct GPULight {
    float3 position;
    float range;

    float3 direction;
    uint type;

    float4 color; // .rgb = color, .a = intensity
};

struct GPUAmbientLight {
    float4 color; // .rgb = color, .a = intensity
};

struct GPUCamera {
    float3 position;
    float focalLength;
    float3 forward;
    float sensorWidth;
};

struct GPUVertex {
    float3 position;
    float u;
    float3 normal;
    float v;
};

struct GPUMaterial {
    float4 baseColor;
    float metallic;
    float roughness;
    float pad0;
    float pad1;
};

struct GPUMesh {
    uint firstVertex;
    uint vertexCount;
    uint firstIndex;
    uint indexCount;
    uint materialIndex;
};

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

// SHARED BUFFERS
struct FrameConstants {
    uint FrameIndex;
    uint LightCount;
};

[[vk::push_constant]]
ConstantBuffer<FrameConstants> frameConstants;

// VULKAN BINDINGS
[[vk::binding(0, 0)]]
RaytracingAccelerationStructure Scene;

[[vk::binding(1, 0)]]
RWTexture2D<float4> Output;

[[vk::binding(2, 0)]]
StructuredBuffer<GPUSphere> Spheres;

[[vk::binding(3, 0)]]
StructuredBuffer<GPULight> Lights;

[[vk::binding(4, 0)]]
ConstantBuffer<GPUAmbientLight> AmbientLight;

[[vk::binding(5, 0)]]
ConstantBuffer<GPUCamera> CameraData;

[[vk::binding(6, 0)]]
StructuredBuffer<GPUVertex> Vertices;

[[vk::binding(7, 0)]]
StructuredBuffer<uint> Indices;

[[vk::binding(8, 0)]]
StructuredBuffer<GPUMesh> Meshes;

[[vk::binding(9, 0)]]
StructuredBuffer<GPUMaterial> Materials;

// Now that AmbientLight is declared, GetSkyColor can reference it.
float3 GetSkyColor(float3 dir) {
    // return float3(0.0, 0.0, 0.0);
    return AmbientLight.color.rgb * AmbientLight.color.a;
}

// SHARED FUNCTIONS
uint PCGHash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Random(inout uint state) {
    state = PCGHash(state);
    return float(state) / 4294967296.0;
}

float2 Random2(inout uint state) {
    return float2(Random(state), Random(state));
}

// SHADING

float3 FresnelSchlick(float3 F0, float cosTheta) {
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// SAMPLING

// built orthonormal basis
void BuildONB(float3 n, out float3 t, out float3 b) {
    float sign = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (sign + n.z);
    float c = n.x * n.y * a;
    t = float3(1.0 + sign * n.x * n.x * a, sign * c, -sign * n.x);
    b = float3(c, sign + n.y * n.y * a, -n.y);
}

float3 SampleCosineHemisphere(float2 u) {
    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float z = sqrt(max(0.0, 1.0 - u.x));
    return float3(r * cos(phi), r * sin(phi), z);
}

// sample the GGX based on visible normals. formula comes from Heitz
float3 SampleGGXVNDF(float3 Ve, float alpha, float2 u) {
    float3 Vh = normalize(float3(alpha * Ve.x, alpha * Ve.y, Ve.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0.0 ? float3(-Vh.y, Vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    return normalize(float3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
}

// Smith GGX masking
float SmithG1GGX(float NoV, float alpha) {
    float alpha2 = alpha * alpha;
    return 2.0 * NoV / (NoV + sqrt(alpha2 + (1.0 - alpha2) * NoV * NoV));
}

float SmithG2GGX(float NoV, float NoL, float alpha) {
    float alpha2 = alpha * alpha;
    float lambdaV = NoL * sqrt(alpha2 + (1.0 - alpha2) * NoV * NoV);
    float lambdaL = NoV * sqrt(alpha2 + (1.0 - alpha2) * NoL * NoL);
    return 2.0 * NoV * NoL / (lambdaV + lambdaL);
}

// Cook-Torrance BRDF lighting model
float3 EvaluateDirectLighting(float3 N, float3 V, float3 L, float3 baseColor, float roughness, float metallic) {
    float NoL = saturate(dot(N, L));

    // Light is behind the surface: no contribution.
    if (NoL <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }

    roughness = max(roughness, MIN_ROUGHNESS);

    float3 H = normalize(V + L);

    float NoV = saturate(dot(N, V));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    // dielectrics ~4% reflectance, metals use baseColor as F0.
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);

    // GGX normal distribution
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float denom = NoH * NoH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (PI * denom * denom);

    // Schlick Fresnel (light lobe uses V.H)
    float3 F = FresnelSchlick(F0, VoH);

    // Smith geometry (Schlick-GGX)
    float k = alpha * 0.5;
    float G_V = NoV / (NoV * (1.0 - k) + k);
    float G_L = NoL / (NoL * (1.0 - k) + k);
    float G = G_V * G_L;

    // Specular
    float3 specular = (D * G * F) / max(4.0 * NoV * NoL, 0.001);

    // Diffuse: energy-conserving via (1 - F), and metals have none.
    float3 diffuse = (1.0 - F) * (1.0 - metallic) * baseColor / PI;

    return diffuse + specular;
}

#endif