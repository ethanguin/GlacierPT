#pragma once
#include <glm/glm.hpp>
#include <cstdint>

enum class LightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2,
};

struct GPULight {
    glm::vec3 position; // point/spot

    glm::vec3 direction; // directional/spot
    float range;         // point/spot falloff radius

    glm::vec4 color; // .rgb = color, .a = intensity
    uint32_t type;
};
static_assert(sizeof(GPULight) == 48);

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
    glm::vec4 positionRadius;
    glm::vec4 color;
};
static_assert(sizeof(GPUSphere) == 32);