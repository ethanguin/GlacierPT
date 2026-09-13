#pragma once

struct Vertex {
    float position[2];
    float color[3];
};

// hardcoded values for a triangle
// TODO pull it from the input dynamically
inline constexpr Vertex TRIANGLE_VERTICES[] = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
};