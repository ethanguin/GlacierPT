#include "ImageLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <stdexcept>

namespace asset {
namespace {

std::vector<uint8_t> readBinary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamsize size = file.tellg();
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

} // namespace

bool decodeImage(const uint8_t* bytes, size_t size, bool srgb, Image& out) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* data = stbi_load_from_memory(bytes, static_cast<int>(size), &w, &h, &channels, 4);
    if (!data) {
        return false;
    }
    out.width = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.srgb = srgb;
    out.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

bool decodeImageFile(const std::filesystem::path& path, bool srgb, Image& out) {
    const auto bytes = readBinary(path);
    return !bytes.empty() && decodeImage(bytes.data(), bytes.size(), srgb, out);
}

HdrImage loadHdr(const std::filesystem::path& path) {
    const auto bytes = readBinary(path);
    if (bytes.empty()) {
        throw std::runtime_error("Failed to read environment map: " + path.string());
    }

    int w = 0, h = 0, channels = 0;
    float* data = stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!data) {
        throw std::runtime_error("Failed to decode environment map (expected Radiance .hdr): " + path.string());
    }

    HdrImage image;
    image.width = static_cast<uint32_t>(w);
    image.height = static_cast<uint32_t>(h);
    image.rgba.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return image;
}

} // namespace asset
