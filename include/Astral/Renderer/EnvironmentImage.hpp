#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Astral {

/// Linear, scene-referred radiance. Supports Radiance RGBE (-Y +X), raw or scanline RLE.
/// Loading is an offline/startup operation; no tone mapping or exposure is baked in.
class EnvironmentImage {
public:
    static EnvironmentImage LoadRadiance(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open HDR environment: " + path.string());
        auto line = [&]() {
            std::string result;
            if (!std::getline(input, result)) throw std::runtime_error("Truncated HDR header");
            if (!result.empty() && result.back() == '\r') result.pop_back();
            return result;
        };
        const auto signature = line();
        if (signature != "#?RADIANCE" && signature != "#?RGBE")
            throw std::runtime_error("Unsupported HDR signature");
        bool format = false;
        for (auto header = line(); !header.empty(); header = line()) {
            if (header == "FORMAT=32-bit_rle_rgbe") format = true;
        }
        EnvironmentImage image;
        std::istringstream resolution(line());
        std::string yAxis, xAxis;
        if (!format || !(resolution >> yAxis >> image.m_Height >> xAxis >> image.m_Width) ||
            yAxis != "-Y" || xAxis != "+X" || image.m_Width < 1 || image.m_Height < 1 ||
            image.m_Width > 16384 || image.m_Height > 8192 ||
            uint64_t(image.m_Width) * image.m_Height > 32 * 1024 * 1024)
            throw std::runtime_error("Unsupported HDR format, orientation or dimensions");
        auto byte = [&]() -> unsigned char {
            const int value = input.get();
            if (value == EOF) throw std::runtime_error("Truncated HDR pixels");
            return static_cast<unsigned char>(value);
        };
        image.m_Pixels.resize(size_t(image.m_Width) * image.m_Height);
        std::vector<std::array<unsigned char, 4>> scanline(image.m_Width);
        for (int y = 0; y < image.m_Height; ++y) {
            std::array<unsigned char, 4> first{byte(), byte(), byte(), byte()};
            if (image.m_Width >= 8 && first[0] == 2 && first[1] == 2 && !(first[2] & 128)) {
                if ((int(first[2]) * 256 + first[3]) != image.m_Width)
                    throw std::runtime_error("Invalid HDR scanline width");
                for (int channel = 0; channel < 4; ++channel) {
                    int x = 0;
                    while (x < image.m_Width) {
                        const int code = byte();
                        const int count = code > 128 ? code - 128 : code;
                        if (!count || count > image.m_Width - x)
                            throw std::runtime_error("Invalid HDR RLE run");
                        if (code > 128) {
                            const auto value = byte();
                            for (int i = 0; i < count; ++i) scanline[x++][channel] = value;
                        } else {
                            for (int i = 0; i < count; ++i) scanline[x++][channel] = byte();
                        }
                    }
                }
            } else {
                scanline[0] = first;
                for (int x = 1; x < image.m_Width; ++x) scanline[x] = {byte(), byte(), byte(), byte()};
            }
            for (int x = 0; x < image.m_Width; ++x) {
                const auto& rgbe = scanline[x];
                const float scale = rgbe[3] ? std::ldexp(1.0f, int(rgbe[3]) - 136) : 0.0f;
                const glm::vec3 radiance = glm::vec3(rgbe[0], rgbe[1], rgbe[2]) * scale;
                if (!std::isfinite(radiance.x) || !std::isfinite(radiance.y) || !std::isfinite(radiance.z) ||
                    glm::any(glm::greaterThan(radiance, glm::vec3(65504.0f))))
                    throw std::runtime_error("HDR radiance exceeds RGBA16F storage range");
                image.m_Pixels[size_t(y) * image.m_Width + x] = radiance;
            }
        }
        return image;
    }

    [[nodiscard]] bool Empty() const noexcept { return m_Pixels.empty(); }
    [[nodiscard]] glm::vec3 Sample(glm::vec3 direction) const noexcept {
        if (Empty()) return glm::vec3(0.0f);
        direction = glm::normalize(direction);
        constexpr float pi = 3.14159265358979323846f;
        const float x = (std::atan2(direction.z, direction.x) / (2 * pi) + 0.5f) * m_Width - 0.5f;
        const float y = std::acos(std::clamp(direction.y, -1.0f, 1.0f)) / pi * m_Height - 0.5f;
        const int ix = static_cast<int>(std::floor(x));
        const int iy = static_cast<int>(std::floor(y));
        auto texel = [&](int u, int v) {
            u = (u % m_Width + m_Width) % m_Width;
            v = std::clamp(v, 0, m_Height - 1);
            return m_Pixels[size_t(v) * m_Width + u];
        };
        return glm::mix(glm::mix(texel(ix, iy), texel(ix + 1, iy), x - ix),
                        glm::mix(texel(ix, iy + 1), texel(ix + 1, iy + 1), x - ix), y - iy);
    }
private:
    int m_Width = 0;
    int m_Height = 0;
    std::vector<glm::vec3> m_Pixels;
};
} // namespace Astral
