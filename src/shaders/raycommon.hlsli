#ifndef RAYCOMMON
#define RAYCOMMON

// SHARED STRUCTS
struct RayPayload {
    float4 color;
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

// Temp Data for cam/lights
Camera GetCamera() {
    Camera cam;
    cam.position = float3(0.0f, 0.0f, 60.0f);
    cam.focalLength = 50.0;
    cam.sensorWidth = 36.0;
    return cam;
}

DirectionalLight GetDirectionalLight() {
    DirectionalLight light;
    light.direction = normalize(float3(1.0f, -1.0f, -1.0f));
    light.color = float3(1.0f, 1.0f, 1.0f);
    light.intensity = 2.0f;
    return light;
}

AmbientLight GetAmbientLight() {
    AmbientLight ambLight;
    ambLight.color = float3(0.91f, 0.608f, 0.91f);
    ambLight.intensity = 0.2f;
    return ambLight;
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

// SHARED BUFFERS
struct FrameConstants {
    uint FrameIndex;
};

[[vk::push_constant]]
ConstantBuffer<FrameConstants> frameConstants;

// VULKAN BINGINDS
[[vk::binding(0, 0)]]
RaytracingAccelerationStructure Scene;

[[vk::binding(1, 0)]]
RWTexture2D<float4> Output;

[[vk::binding(2, 0)]]
StructuredBuffer<GPUSphere> Spheres;

#endif