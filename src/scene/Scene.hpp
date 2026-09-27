#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <filesystem>

#include "SceneLight.hpp"
#include "asset/Model.hpp"
#include "asset/GltfLoader.hpp"

struct SceneSphere {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
};

class Scene {
public:
    void addSphere(const glm::vec3& pos, const float rad, const glm::vec3& color) {
        m_spheres.push_back({pos, rad, color});
    }
    void addSphere(const glm::vec3& pos, const float rad) {
        m_spheres.push_back({pos, rad, {0.2f, 0.2f, 0.2f}});
    }
    void addLight(const SceneLight& light) {
        m_lights.push_back(light);
    }
    void setAmbLight(glm::vec3 color, float intensity) {
        m_ambLight = SceneAmbientLight(color, intensity);
    }
    void loadModel(const std::filesystem::path& path) {
        asset::loadGltf(path, m_geometry);
    }

    const std::vector<SceneSphere>& spheres() const {
        return m_spheres;
    }
    const std::vector<SceneLight>& lights() const {
        return m_lights;
    }
    const SceneAmbientLight& ambientLight() const {
        return m_ambLight;
    }
    const asset::Model& geometry() const {
        return m_geometry;
    }

private:
    std::vector<SceneSphere> m_spheres;
    std::vector<SceneLight> m_lights;
    SceneAmbientLight m_ambLight;
    asset::Model m_geometry;
};