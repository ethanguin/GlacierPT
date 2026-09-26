#pragma once

#include "Model.hpp"

#include <filesystem>

namespace asset {

// Loads the default scene of a .gltf/.glb into `model` (materials, one mesh per
// triangle primitive, one instance per node->mesh reference with its world
// transform). Textures, cameras and lights are not imported yet. Throws
// std::runtime_error on failure.
void loadGltf(const std::filesystem::path& path, Model& model);

} // namespace asset
