#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

// -----------------------------------------------------------------------------
// asset::Model is the interchange format: a renderer-agnostic in-memory
// representation of triangle geometry, produced by a loader (GltfLoader today,
// a future BinaryLoader later) and consumed by a renderer (VulkanGeometry today,
// possibly others later). Nothing in this file knows about Vulkan, HLSL, or any
// specific backend's shader bindings.
//
// Every struct here is a plain, tightly packed POD with no pointers, so a whole
// array can be memcpy'd wholesale -- into a GPU buffer, into a file, wherever.
// That packing is a convenience, not a promise to match any particular shader's
// binding layout; a backend that needs a different layout converts on upload.
// -----------------------------------------------------------------------------

namespace asset {

struct Vertex {
    glm::vec3 position;
    float u;
    glm::vec3 normal;
    float v;
};
static_assert(sizeof(Vertex) == 32);

struct Material {
    glm::vec4 baseColor; // linear
    float metallic;
    float roughness;
    float transmission; // KHR_materials_transmission
    float ior;          // KHR_materials_ior
};
static_assert(sizeof(Material) == 32);

// One record per mesh (= one glTF triangle primitive, or the equivalent from any
// other source). Indices are LOCAL to the mesh (0..vertexCount-1); add firstVertex
// to reach the global vertex index. A backend's acceleration-structure build and
// its vertex fetch both read this same record, so there is nothing else to keep
// in sync.
struct Mesh {
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t materialIndex;
};
static_assert(sizeof(Mesh) == 20);

// A placement of a mesh in the scene (glTF node -> mesh reference).
struct MeshInstance {
    uint32_t mesh;
    glm::mat4 transform;
};

// All triangle geometry for a scene, stored as pooled arrays. This shape is
// deliberately "chunk-friendly": each member is an independent, self-describing
// array (count + raw bytes), which is exactly what a future binary container
// format needs -- see BinaryFormat.hpp.
class Model {
public:
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

    const std::vector<Vertex>& vertices() const {
        return m_vertices;
    }
    const std::vector<uint32_t>& indices() const {
        return m_indices;
    }
    const std::vector<Mesh>& meshes() const {
        return m_meshes;
    }
    const std::vector<Material>& materials() const {
        return m_materials;
    }
    const std::vector<MeshInstance>& instances() const {
        return m_instances;
    }

    uint32_t materialCount() const {
        return static_cast<uint32_t>(m_materials.size());
    }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::vector<MeshInstance> m_instances;
};

} // namespace asset
