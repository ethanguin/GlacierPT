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