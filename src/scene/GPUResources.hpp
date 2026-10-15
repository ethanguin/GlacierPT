#pragma once
#include <glm/glm.hpp>
#include <cstdint>

enum class LightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2,
    Area = 3,
};

struct GPULight {
    glm::vec3 position; // point/spot/area center
    float range;        // point/spot falloff radius

    glm::vec3 direction; // directional/spot/area normal
    uint32_t type;

    glm::vec4 color; // .rgb = color, .a = intensity (area: emitted radiance)

    glm::vec3 edgeU; // area: half-extent vector along width
    float pad0;

    glm::vec3 edgeV; // area: half-extent vector along height (cross(edgeU, edgeV) = normal)
    float twoSided;  // area: > 0.5 emits from both faces
};
static_assert(sizeof(GPULight) == 80);

struct GPUAmbientLight {
    glm::vec4 color; // .rgb = color, .a = intensity
};
static_assert(sizeof(GPUAmbientLight) == 16);

struct GPUCamera {
    glm::vec3 position;
    float focalLength;

    glm::vec3 forward;
    float sensorWidth;
};
static_assert(sizeof(GPUCamera) == 32);

// explicit GPU sphere structure so it doesn't have to be connected to attributes for the scene sphere and add its own padding to map to HLSL
struct GPUSphere {
    glm::vec4 positionRadius; // .xyz = center, .w = radius
    glm::vec4 color;          // .rgb = color, .a = roughness
};
static_assert(sizeof(GPUSphere) == 32);