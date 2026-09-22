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

    void addYawPitch(float deltaYawDeg, float deltaPitchDeg) {
        m_yaw += deltaYawDeg;
        m_pitch = glm::clamp(m_pitch + deltaPitchDeg, -89.0f, 89.0f);
        updateForward();
    }

    glm::vec3 right() const {
        // Must match the raygen shader's basis exactly (worldUp cross, no roll)
        return glm::normalize(glm::cross(m_forward, {0.0f, 1.0f, 0.0f}));
    }

private:
    void updateForward() {
        float yawRad = glm::radians(m_yaw);
        float pitchRad = glm::radians(m_pitch);
        m_forward = glm::normalize(glm::vec3(cos(pitchRad) * sin(yawRad), sin(pitchRad), -cos(pitchRad) * cos(yawRad)));
    }

    float m_yaw = 0.0f; // degrees, 0 = looking down -Z
    float m_pitch = 0.0f;
    glm::vec3 m_position = {0.0f, 0.0f, 60.0f};
    glm::vec3 m_forward = {0.0f, 0.0f, -1.0f};

    float m_focalLength = 50.0f;
    float m_sensorWidth = 36.0f;
};