#pragma once

#include "Model.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace asset {

struct HdrImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> rgba; // linear, 4 floats per pixel
};

// Decode PNG/JPG/... from memory or disk into RGBA8. Returns false on failure.
bool decodeImage(const uint8_t* bytes, size_t size, bool srgb, Image& out);
bool decodeImageFile(const std::filesystem::path& path, bool srgb, Image& out);

// Radiance .hdr equirectangular environment map. Throws std::runtime_error on failure.
HdrImage loadHdr(const std::filesystem::path& path);

} // namespace asset
