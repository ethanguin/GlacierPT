#pragma once
#include <cmath>
#include "GPUResources.hpp"

class SceneLight {
public:
    static SceneLight Directional(const glm::vec3& direction, const glm::vec3& color, float intensity) {
        SceneLight light;
        light.m_data.direction = glm::normalize(direction);
        light.m_data.color = glm::vec4(color, intensity);
        light.m_data.type = static_cast<uint32_t>(LightType::Directional);
        return light;
    }

    static SceneLight Point(const glm::vec3& position, const glm::vec3& color, float intensity, float range) {
        SceneLight light;
        light.m_data.position = position;
        light.m_data.color = glm::vec4(color, intensity);
        light.m_data.range = range;
        light.m_data.type = static_cast<uint32_t>(LightType::Point);
        return light;
    }

    static SceneLight Spot(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float range) {
        SceneLight light;
        light.m_data.position = position;
        light.m_data.direction = glm::normalize(direction);
        light.m_data.color = glm::vec4(color, intensity);
        light.m_data.range = range;
        light.m_data.type = static_cast<uint32_t>(LightType::Spot);
        return light;
    }

    static SceneLight Area(const glm::vec3& center, const glm::vec3& normal, float width, float height, const glm::vec3& color, float intensity,
                           bool twoSided = false) {
        const glm::vec3 n = glm::normalize(normal);
        const glm::vec3 ref = std::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 t = glm::normalize(glm::cross(ref, n));

        SceneLight light;
        light.m_data.position = center;
        light.m_data.direction = n;
        light.m_data.color = glm::vec4(color, intensity);
        light.m_data.edgeU = t * (width * 0.5f);
        light.m_data.edgeV = glm::cross(n, t) * (height * 0.5f); // cross(u, v) == n
        light.m_data.twoSided = twoSided ? 1.0f : 0.0f;
        light.m_data.type = static_cast<uint32_t>(LightType::Area);
        return light;
    }

    glm::vec3 color() const {
        return glm::vec3(m_data.color);
    }
    float intensity() const {
        return m_data.color.a;
    }
    LightType type() const {
        return static_cast<LightType>(m_data.type);
    }
    const GPULight& gpuData() const {
        return m_data;
    }

private:
    SceneLight() : m_data{} {}
    GPULight m_data{};
};

class SceneAmbientLight {
public:
    SceneAmbientLight() : SceneAmbientLight({0.7f, 0.7f, 0.86f}, 0.15f) {}

    SceneAmbientLight(const glm::vec3& color, float intensity) {
        m_data.color = glm::vec4(color, intensity);
    }

    glm::vec3 color() const {
        return glm::vec3(m_data.color);
    }
    float intensity() const {
        return m_data.color.a;
    }
    const GPUAmbientLight& gpuData() const {
        return m_data;
    }

private:
    GPUAmbientLight m_data{{0.0f, 0.0f, 0.0f, 0.0f}};
};