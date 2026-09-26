#include "GltfLoader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace asset {
namespace {

struct LoadContext {
    const fastgltf::Asset& gltfAsset;
    Model& model;

    uint32_t defaultMaterial = 0;
    uint32_t materialBase = 0; // glTF material i -> model material (materialBase + i)

    // glTF mesh index -> model mesh ids (one per triangle primitive). Filled lazily so
    // meshes that no node references are never added, and shared meshes are instanced.
    std::vector<std::optional<std::vector<uint32_t>>> meshCache;
};

// Primitive::attributes changed from std::pair<name, index> to a struct with
// .accessorIndex between fastgltf releases; this keeps the loader working on both.
template <typename Attribute>
size_t accessorIndexOf(const Attribute& attribute) {
    if constexpr (requires { attribute.accessorIndex; }) {
        return attribute.accessorIndex;
    } else {
        return attribute.second;
    }
}

std::optional<size_t> findAttribute(const fastgltf::Primitive& primitive, std::string_view name) {
    auto it = primitive.findAttribute(name);

    if (it == primitive.attributes.end()) {
        return std::nullopt;
    }

    return accessorIndexOf(*it);
}

// Area-weighted smooth normals, used when the file has no NORMAL attribute.
void computeSmoothNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (Vertex& vertex : vertices) {
        vertex.normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i + 0]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];

        const glm::vec3 faceNormal = glm::cross(b.position - a.position, c.position - a.position);

        a.normal += faceNormal;
        b.normal += faceNormal;
        c.normal += faceNormal;
    }

    for (Vertex& vertex : vertices) {
        vertex.normal = glm::dot(vertex.normal, vertex.normal) > 0.0f ? glm::normalize(vertex.normal) : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

std::optional<uint32_t> addPrimitive(LoadContext& ctx, const fastgltf::Primitive& primitive) {
    if (primitive.type != fastgltf::PrimitiveType::Triangles) {
        return std::nullopt; // points/lines/strips/fans aren't supported
    }

    const auto positionIndex = findAttribute(primitive, "POSITION");

    if (!positionIndex) {
        return std::nullopt;
    }

    const fastgltf::Asset& gltfAsset = ctx.gltfAsset;

    // POSITION

    const fastgltf::Accessor& positionAccessor = gltfAsset.accessors[*positionIndex];

    std::vector<Vertex> vertices(positionAccessor.count, Vertex{});

    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, positionAccessor,
                                                  [&](glm::vec3 position, size_t i) { vertices[i].position = position; });

    // NORMAL (optional)

    bool hasNormals = false;

    if (const auto normalIndex = findAttribute(primitive, "NORMAL")) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, gltfAsset.accessors[*normalIndex],
                                                      [&](glm::vec3 normal, size_t i) { vertices[i].normal = normal; });
        hasNormals = true;
    }

    // TEXCOORD_0 (optional, stored for later texture support)

    if (const auto uvIndex = findAttribute(primitive, "TEXCOORD_0")) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltfAsset, gltfAsset.accessors[*uvIndex], [&](glm::vec2 uv, size_t i) {
            vertices[i].u = uv.x;
            vertices[i].v = uv.y;
        });
    }

    // INDICES (glTF allows non-indexed primitives; synthesize 0,1,2,... for those)

    std::vector<uint32_t> indices;

    if (primitive.indicesAccessor.has_value()) {
        const fastgltf::Accessor& indexAccessor = gltfAsset.accessors[*primitive.indicesAccessor];

        indices.resize(indexAccessor.count);

        fastgltf::iterateAccessorWithIndex<uint32_t>(gltfAsset, indexAccessor, [&](uint32_t index, size_t i) { indices[i] = index; });
    } else {
        indices.resize(vertices.size());

        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = static_cast<uint32_t>(i);
        }
    }

    for (const uint32_t index : indices) {
        if (index >= vertices.size()) {
            throw std::runtime_error("glTF primitive has an index outside its vertex range.");
        }
    }

    if (indices.size() < 3) {
        return std::nullopt;
    }

    if (!hasNormals) {
        computeSmoothNormals(vertices, indices);
    }

    const uint32_t materialIndex =
        primitive.materialIndex.has_value() ? ctx.materialBase + static_cast<uint32_t>(*primitive.materialIndex) : ctx.defaultMaterial;

    return ctx.model.addMesh(vertices, indices, materialIndex);
}

const std::vector<uint32_t>& getMeshPrimitives(LoadContext& ctx, size_t gltfMeshIndex) {
    auto& cached = ctx.meshCache[gltfMeshIndex];

    if (!cached) {
        cached.emplace();

        for (const fastgltf::Primitive& primitive : ctx.gltfAsset.meshes[gltfMeshIndex].primitives) {
            if (const auto meshId = addPrimitive(ctx, primitive)) {
                cached->push_back(*meshId);
            }
        }
    }

    return *cached;
}

void visitNode(LoadContext& ctx, size_t nodeIndex, const glm::mat4& parentTransform) {
    const fastgltf::Node& node = ctx.gltfAsset.nodes[nodeIndex];

    // getTransformMatrix handles both the matrix and the TRS forms of node.transform.
    const auto local = fastgltf::getTransformMatrix(node);
    const glm::mat4 world = parentTransform * glm::make_mat4(local.data());

    if (node.meshIndex.has_value()) {
        for (const uint32_t meshId : getMeshPrimitives(ctx, *node.meshIndex)) {
            ctx.model.addInstance(meshId, world);
        }
    }

    for (const size_t child : node.children) {
        visitNode(ctx, child, world);
    }
}

} // namespace

void loadGltf(const std::filesystem::path& path, Model& model) {
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None) {
        throw std::runtime_error("Failed to read glTF file '" + path.string() + "': " + std::string(fastgltf::getErrorMessage(data.error())));
    }

    fastgltf::Parser parser;

    // LoadExternalBuffers is needed for .gltf + .bin; images are intentionally not loaded yet.
    auto gltfAsset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);

    if (gltfAsset.error() != fastgltf::Error::None) {
        throw std::runtime_error("Failed to parse glTF file '" + path.string() + "': " + std::string(fastgltf::getErrorMessage(gltfAsset.error())));
    }

    LoadContext ctx{gltfAsset.get(), model};

    // Materials

    Material fallback{};
    fallback.baseColor = {0.8f, 0.8f, 0.8f, 1.0f};
    fallback.metallic = 0.0f;
    fallback.roughness = 0.5f;

    ctx.defaultMaterial = model.addMaterial(fallback);
    ctx.materialBase = model.materialCount();

    for (const fastgltf::Material& gltfMaterial : ctx.gltfAsset.materials) {
        const auto& pbr = gltfMaterial.pbrData;

        Material material{};
        material.baseColor = {pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
        material.metallic = pbr.metallicFactor;
        material.roughness = pbr.roughnessFactor;

        model.addMaterial(material);
    }

    // Scene graph -> instances

    if (ctx.gltfAsset.scenes.empty()) {
        throw std::runtime_error("glTF file has no scenes: " + path.string());
    }

    const size_t sceneIndex = ctx.gltfAsset.defaultScene.value_or(0);

    if (sceneIndex >= ctx.gltfAsset.scenes.size()) {
        throw std::runtime_error("glTF default scene index is out of range: " + path.string());
    }

    ctx.meshCache.resize(ctx.gltfAsset.meshes.size());

    for (const size_t rootNode : ctx.gltfAsset.scenes[sceneIndex].nodeIndices) {
        visitNode(ctx, rootNode, glm::mat4(1.0f));
    }
}

} // namespace asset
