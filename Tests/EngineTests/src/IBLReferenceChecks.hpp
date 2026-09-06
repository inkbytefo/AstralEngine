#pragma once
#include "Astral/Renderer/IBLManager.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include <glm/gtc/packing.hpp>
#include <fstream>
#include <iostream>

namespace Astral::Test {
inline std::vector<uint16_t> ReadIBL(VulkanContext& context, vk::Image image,
                                    uint32_t size, uint32_t layers, uint32_t channels, uint32_t mip = 0) {
    const size_t elements = size_t(size) * size * layers * channels;
    Buffer buffer(context.GetDevice(), context.GetPhysicalDevice(), elements * sizeof(uint16_t),
                  vk::BufferUsageFlagBits::eTransferDst,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, mip, 1, 0, layers};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                            {}, {}, {}, barrier);
        vk::BufferImageCopy region{};
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, mip, 0, layers};
        region.imageExtent = vk::Extent3D(size, size, 1);
        cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer.GetBuffer(), region);
        std::swap(barrier.oldLayout, barrier.newLayout);
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands,
                            {}, {}, {}, barrier);
    });
    const auto* data = static_cast<const uint16_t*>(buffer.GetMappedData());
    return {data, data + elements};
}

// Independent integration over uniform solid angle in L, rather than production GGX H sampling.
inline glm::dvec2 ReferenceBRDF(double nv, double roughness) {
    constexpr int samples = 65536;
    constexpr double pi = 3.14159265358979323846;
    glm::dvec2 sum(0);
    const glm::dvec3 v(std::sqrt(1 - nv * nv), 0, nv);
    const double a2 = std::pow(roughness, 4);
    const double k = roughness * roughness * 0.5;
    for (int i = 0; i < samples; ++i) {
        const double nl = (i + 0.5) / samples;
        const double phi = 2 * pi * std::fmod(i * 0.6180339887498949, 1.0);
        const double radius = std::sqrt(1 - nl * nl);
        const glm::dvec3 l(radius * std::cos(phi), radius * std::sin(phi), nl);
        const auto h = glm::normalize(v + l);
        const double d = a2 / (pi * std::pow(h.z * h.z * (a2 - 1) + 1, 2));
        const double g = nv / (nv * (1 - k) + k) * nl / (nl * (1 - k) + k);
        const double fc = std::pow(1 - glm::dot(v, h), 5);
        sum += glm::dvec2(1 - fc, fc) * (d * g / (4 * nv));
    }
    return sum * (2 * pi / samples);
}

inline std::filesystem::path CheckIBLReferences(VulkanContext& context, const std::filesystem::path& output) {
    const auto path = output / "constant_radiance.hdr";
    {
        std::ofstream file(path, std::ios::binary);
        file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n";
        for (int y = 0; y < 2; ++y) {
            const unsigned char row[]{2, 2, 0, 8, 136, 128, 136, 64, 136, 32, 136, 130};
            file.write(reinterpret_cast<const char*>(row), sizeof(row));
        }
    }
    IBLManager ibl(context, path);
    float maxError = 0;
    auto checkConstant = [&](const std::vector<uint16_t>& pixels) {
        const float expected[]{2, 1, 0.5f, 1};
        for (size_t i = 0; i < pixels.size(); ++i) {
            const float value = glm::unpackHalf1x16(pixels[i]);
            if (!std::isfinite(value)) throw std::runtime_error("Nonfinite IBL output");
            maxError = std::max(maxError, std::abs(value - expected[i % 4]));
        }
    };
    checkConstant(ReadIBL(context, ibl.GetIrradianceImage(), 32, 6, 4));
    for (uint32_t mip = 0; mip < ibl.GetPrefilteredMipLevels(); ++mip)
        checkConstant(ReadIBL(context, ibl.GetPrefilteredImage(), 128 >> mip, 6, 4, mip));
    std::cout << "IBL constant radiance max error=" << maxError << '\n';
    if (maxError > 0.003f) throw std::runtime_error("IBL radiance conservation failed");
    const auto lut = ReadIBL(context, ibl.GetBRDFLutImage(), 256, 1, 2);
    double brdfError = 0;
    std::ofstream report(output / "brdf_reference.csv");
    report << "NdotV,roughness,gpuA,gpuB,referenceA,referenceB\n";
    for (int y : {76, 153, 230}) for (int x : {51, 128, 230}) {
        const double nv = (x + 0.5) / 256;
        const double roughness = (y + 0.5) / 256;
        const auto reference = ReferenceBRDF(nv, roughness);
        const glm::dvec2 actual(glm::unpackHalf1x16(lut[(y * 256 + x) * 2]),
                                glm::unpackHalf1x16(lut[(y * 256 + x) * 2 + 1]));
        brdfError = std::max({brdfError, std::abs(reference.x - actual.x), std::abs(reference.y - actual.y)});
        report << nv << ',' << roughness << ',' << actual.x << ',' << actual.y << ',' << reference.x << ',' << reference.y << '\n';
    }
    std::cout << "BRDF independent quadrature max error=" << brdfError << '\n';
    if (brdfError > 0.025) throw std::runtime_error("BRDF reference tolerance exceeded");
    // A directional fixture also detects flipped cubemap faces; a constant alone cannot.
    const auto gradientPath = output / "directional_radiance.hdr";
    {
        std::ofstream file(gradientPath, std::ios::binary);
        file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 32 +X 64\n";
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
            const double theta = (y + 0.5) / 32 * 3.141592653589793;
            const double phi = ((x + 0.5) / 64 - 0.5) * 6.283185307179586;
            const unsigned char pixel[]{static_cast<unsigned char>(128 + 100 * std::cos(theta)),
                static_cast<unsigned char>(128 + 100 * std::sin(phi) * std::sin(theta)), 64, 129};
            file.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
        }
    }
    IBLManager gradient(context, gradientPath);
    const auto radiance = ReadIBL(context, gradient.GetPrefilteredImage(), 128, 6, 4);
    auto center = [&](int face) { return glm::unpackHalf1x16(radiance[((face * 128 + 64) * 128 + 64) * 4]); };
    if (center(2) < 1.7f || center(3) > 0.3f) throw std::runtime_error("Cubemap vertical orientation failed");
    const auto diffuse = ReadIBL(context, gradient.GetIrradianceImage(), 32, 6, 4);
    // Lambert convolution of a constant + linear directional signal scales the latter by 2/3.
    const float upper = glm::unpackHalf1x16(diffuse[((2 * 32 + 16) * 32 + 16) * 4]);
    const float lower = glm::unpackHalf1x16(diffuse[((3 * 32 + 16) * 32 + 16) * 4]);
    const float directionalError = std::max(std::abs(upper - (1.0f + 100.0f / 128 * 2 / 3)),
                                            std::abs(lower - (1.0f - 100.0f / 128 * 2 / 3)));
    std::cout << "IBL directional analytic reference max error=" << directionalError << '\n';
    if (directionalError > 0.015f) throw std::runtime_error("Directional irradiance reference failed");
    return path;
}
}
