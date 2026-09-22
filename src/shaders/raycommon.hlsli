#ifndef RAYCOMMON
#define RAYCOMMON

static const float PI = 3.14159265;

// Iterative bounce count, owned by raygen. No longer tied to
// maxPipelineRayRecursionDepth (which is now 1).
static const uint MAX_BOUNCES = 4;

// Below this, GGX gets numerically unstable (D spikes, denom cancels to 0).
static const float MIN_ROUGHNESS = 0.05;

// SHARED STRUCTS

// Filled by ClosestHit, consumed by raygen's bounce loop.
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

struct DirectionalLight {
    float3 direction;
    float3 color;
    float intensity;
};

struct Camera {
    float3 position;
    float focalLength;
    float sensorWidth;
};

struct AmbientLight {
    float3 color;
    float intensity;
};

float3 GetSkyColor(float3 dir) {
    return float3(0.1, 0.1, 0.2);
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

#endif