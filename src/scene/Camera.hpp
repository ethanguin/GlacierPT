#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;

    Camera(const glm::vec3& position, const glm::vec3& forward, float focalLength, float sensorWidth)
        : m_position(position), m_forward(glm::normalize(forward)), m_focalLength(focalLength), m_sensorWidth(sensorWidth) {}

    glm::vec3 position() const {
        return m_position;
    }
    glm::vec3 forward() const {
        return m_forward;
    }
    float focalLength() const {
        return m_focalLength;
    }
    float sensorWidth() const {
        return m_sensorWidth;
    }

    void setPosition(const glm::vec3& position) {
        m_position = position;
    }
    void setForward(const glm::vec3& forward) {
        m_forward = glm::normalize(forward);
    }

private:
    glm::vec3 m_position = {0.0f, 0.0f, 60.0f};
    glm::vec3 m_forward = {0.0f, 0.0f, -1.0f};

    float m_focalLength = 50.0f;
    float m_sensorWidth = 36.0f;
};