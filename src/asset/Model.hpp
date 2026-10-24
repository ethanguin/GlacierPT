#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace asset {

struct Vertex {
    glm::vec3 position;
    float u;
    glm::vec3 normal;
    float v;
};
static_assert(sizeof(Vertex) == 32);

// Decoded 8-bit RGBA image. `srgb` decides the GPU format (sRGB for color data, UNORM for data textures).
struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    bool srgb = false;
    std::vector<uint8_t> pixels; // tightly packed RGBA8
};

// Texture fields are indices into Model::images(), or -1 for "no texture".
// Must match GPUMaterial in raycommon.hlsli (64 bytes).
struct Material {
    glm::vec4 baseColor{1.0f}; // linear factor
    float metallic = 0.0f;
    float roughness = 1.0f;
    float transmission = 0.0f; // KHR_materials_transmission
    float ior = 1.5f;          // KHR_materials_ior

    int32_t baseColorTexture = -1;         // sRGB
    int32_t metallicRoughnessTexture = -1; // G = roughness, B = metallic
    int32_t normalTexture = -1;            // tangent space, +Y up
    int32_t emissiveTexture = -1;          // sRGB

    glm::vec3 emissive{0.0f}; // emissiveFactor * KHR_materials_emissive_strength
    float normalScale = 1.0f;
};
static_assert(sizeof(Material) == 64);

// One record per mesh (= one glTF triangle primitive). Indices are LOCAL to the mesh.
struct Mesh {
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t materialIndex;
};
static_assert(sizeof(Mesh) == 20);

struct MeshInstance {
    uint32_t mesh;
    glm::mat4 transform;
};

class Model {
public:
    uint32_t addImage(Image image) {
        m_images.push_back(std::move(image));
        return static_cast<uint32_t>(m_images.size() - 1);
    }

    uint32_t addMaterial(const Material& material) {
        m_materials.push_back(material);
        return static_cast<uint32_t>(m_materials.size() - 1);
    }

    uint32_t addMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices, uint32_t materialIndex) {
        if (indices.size() < 3 || vertices.empty()) {
            throw std::runtime_error("Model::addMesh: mesh needs at least one triangle.");
        }
        if (materialIndex >= m_materials.size()) {
            throw std::runtime_error("Model::addMesh: material index out of range.");
        }

        Mesh mesh{};
        mesh.firstVertex = static_cast<uint32_t>(m_vertices.size());
        mesh.vertexCount = static_cast<uint32_t>(vertices.size());
        mesh.firstIndex = static_cast<uint32_t>(m_indices.size());
        mesh.indexCount = static_cast<uint32_t>(indices.size() - indices.size() % 3);
        mesh.materialIndex = materialIndex;

        m_vertices.insert(m_vertices.end(), vertices.begin(), vertices.end());
        m_indices.insert(m_indices.end(), indices.begin(), indices.begin() + mesh.indexCount);
        m_meshes.push_back(mesh);

        return static_cast<uint32_t>(m_meshes.size() - 1);
    }

    void addInstance(uint32_t meshIndex, const glm::mat4& transform) {
        if (meshIndex >= m_meshes.size()) {
            throw std::runtime_error("Model::addInstance: mesh index out of range.");
        }
        m_instances.push_back({meshIndex, transform});
    }

    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<uint32_t>& indices() const { return m_indices; }
    const std::vector<Mesh>& meshes() const { return m_meshes; }
    const std::vector<Material>& materials() const { return m_materials; }
    const std::vector<MeshInstance>& instances() const { return m_instances; }
    const std::vector<Image>& images() const { return m_images; }

    uint32_t materialCount() const { return static_cast<uint32_t>(m_materials.size()); }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::vector<MeshInstance> m_instances;
    std::vector<Image> m_images;
};

} // namespace asset
