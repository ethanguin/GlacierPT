#pragma once

#include <glm/vec3.hpp>
#include <cmath>

inline glm::vec3 srgbToLinear(const glm::vec3& color) {
    auto convert = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };

    return {convert(color.r), convert(color.g), convert(color.b)};
}