#include "VulkanEnvironment.hpp"

#include "asset/ImageLoader.hpp"
#include "scene/GPUResources.hpp"

#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Real SH basis, l <= 2. Index order must match EnvIrradiance() in raycommon.hlsli.
void shBasis(double x, double y, double z, double out[9]) {
    out[0] = 0.282095;
    out[1] = 0.488603 * y;
    out[2] = 0.488603 * z;
    out[3] = 0.488603 * x;
    out[4] = 1.092548 * x * y;
    out[5] = 1.092548 * y * z;
    out[6] = 0.315392 * (3.0 * z * z - 1.0);
    out[7] = 1.092548 * x * z;
    out[8] = 0.546274 * (x * x - y * y);
}

// Project the environment onto SH9 and pre-multiply by the cosine-lobe convolution (Ramamoorthi & Hanrahan),
// so the shader only needs sum(coeff_i * basis_i(n)) to get irradiance E(n).
//
// Direction <-> equirect mapping (shared with GetSkyColor):
//   u = atan2(d.x, -d.z) / 2pi + 0.5,  v = acos(d.y) / pi   (v = 0 is straight up)
std::array<glm::vec4, 9> computeIrradianceSH(const asset::HdrImage& hdr) {
    std::array<glm::dvec3, 9> sh{};

    const double w = hdr.width;
    const double h = hdr.height;

    std::vector<double> sinPhi(hdr.width), cosPhi(hdr.width);
    for (uint32_t x = 0; x < hdr.width; ++x) {
        const double phi = ((x + 0.5) / w - 0.5) * 2.0 * kPi;
        sinPhi[x] = std::sin(phi);
        cosPhi[x] = std::cos(phi);
    }

    for (uint32_t y = 0; y < hdr.height; ++y) {
        const double theta = (y + 0.5) / h * kPi;
        const double sinT = std::sin(theta);
        const double cosT = std::cos(theta);
        const double dOmega = (2.0 * kPi / w) * (kPi / h) * sinT;

        for (uint32_t x = 0; x < hdr.width; ++x) {
            const double dx = sinT * sinPhi[x];
            const double dy = cosT;
            const double dz = -sinT * cosPhi[x];

            double basis[9];
            shBasis(dx, dy, dz, basis);

            const float* px = &hdr.rgba[(static_cast<size_t>(y) * hdr.width + x) * 4];
            const glm::dvec3 radiance(px[0], px[1], px[2]);

            for (int i = 0; i < 9; ++i) {
                sh[i] += radiance * (basis[i] * dOmega);
            }
        }
    }

    constexpr double A[3] = {kPi, 2.0 * kPi / 3.0, kPi / 4.0};

    std::array<glm::vec4, 9> out{};
    for (int i = 0; i < 9; ++i) {
        const int l = i == 0 ? 0 : (i < 4 ? 1 : 2);
        out[i] = glm::vec4(glm::vec3(sh[i] * A[l]), 0.0f);
    }
    return out;
}

} // namespace

void VulkanEnvironment::initialize(VulkanContext& context, VulkanCommands& commands, const std::filesystem::path& hdrPath, float intensity,
                                   float rotationRadians) {
    m_context = &context;

    GPUEnvironment params{};
    params.intensity = intensity;
    params.rotation = rotationRadians;

    constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    if (!hdrPath.empty()) {
        std::cout << "Loading environment map: " << hdrPath << '\n';

        const asset::HdrImage hdr = asset::loadHdr(hdrPath);

        const auto sh = computeIrradianceSH(hdr);
        for (int i = 0; i < 9; ++i) {
            params.sh[i] = sh[i];
        }
        params.enabled = 1;
        m_enabled = true;

        // float32 -> float16 (clamped to the half range; NaN -> 0).
        std::vector<uint64_t> half(static_cast<size_t>(hdr.width) * hdr.height);
        for (size_t i = 0; i < half.size(); ++i) {
            glm::vec4 c(hdr.rgba[i * 4 + 0], hdr.rgba[i * 4 + 1], hdr.rgba[i * 4 + 2], 1.0f);
            for (int k = 0; k < 3; ++k) {
                c[k] = (c[k] >= 0.0f) ? std::min(c[k], 65504.0f) : 0.0f;
            }
            half[i] = glm::packHalf4x16(c);
        }

        m_image = uploadTexture2D(context, commands, hdr.width, hdr.height, format, half.data(), half.size() * sizeof(uint64_t));
    } else {
        const uint64_t black = glm::packHalf4x16(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        m_image = uploadTexture2D(context, commands, 1, 1, format, &black, sizeof(black));
    }

    m_view = createTextureView(context.device(), m_image.image, format);

    // Wrap horizontally, clamp at the poles.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(context.device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create environment sampler.");
    }

    m_params = context.allocator().createBuffer(sizeof(GPUEnvironment), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    context.allocator().uploadBuffer(m_params, &params, sizeof(GPUEnvironment));
}

void VulkanEnvironment::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    VkDevice device = m_context->device();

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }

    m_context->allocator().destroyImage(m_image);
    m_context->allocator().destroyBuffer(m_params);

    m_enabled = false;
    m_context = nullptr;
}
