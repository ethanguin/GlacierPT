#pragma once

#include <glm/glm.hpp>
#include <vector>

struct SceneSphere {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
};

struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
};

struct AmbientLight {
    glm::vec3 color;
    float intensity;
};

class Scene {
public:
    void addSphere(const glm::vec3& pos, const float rad, const glm::vec3& color) {
        m_spheres.push_back({pos, rad, color});
    }
    void addSphere(const glm::vec3& pos, const float rad) {
        m_spheres.push_back({pos, rad, {0.2f, 0.2f, 0.2f}});
    }
    void setDirectionalLight(const glm::vec3& dir, const glm::vec3& color, const float intensity) {
        m_dirLight = {glm::normalize(dir), color, intensity};
    }
    void setAmbientLight(const glm::vec3& color, const float intensity) {
        m_ambLight = {color, intensity};
    }

    const std::vector<SceneSphere>& spheres() const {
        return m_spheres;
    }
    const DirectionalLight& directionalLight() const {
        return m_dirLight;
    }
    const AmbientLight& ambientLight() const {
        return m_ambLight;
    }

private:
    std::vector<SceneSphere> m_spheres;
    DirectionalLight m_dirLight{glm::normalize(glm::vec3(1.0f, 1.0f, -1.0f)), {1.0f, 1.0f, 1.0f}, 1.0f};
    AmbientLight m_ambLight{{0.7f, 0.7f, 0.86f}, 0.15f};
};