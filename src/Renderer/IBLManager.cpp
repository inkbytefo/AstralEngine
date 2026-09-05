#include "Astral/Renderer/IBLManager.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace Astral {

namespace {

static uint16_t FloatToHalf(float val) {
    uint32_t x;
    std::memcpy(&x, &val, sizeof(float));
    uint32_t sign = (x >> 31) & 0x1;
    uint32_t exp = (x >> 23) & 0xFF;
    uint32_t mant = x & 0x7FFFFF;
    if (exp == 0) return static_cast<uint16_t>(sign << 15);
    if (exp == 0xFF) return static_cast<uint16_t>((sign << 15) | 0x7C00 | (mant ? 1 : 0));
    int newExp = static_cast<int>(exp) - 127 + 15;
    if (newExp >= 31) return static_cast<uint16_t>((sign << 15) | 0x7C00);
    if (newExp <= 0) return static_cast<uint16_t>(sign << 15);
    return static_cast<uint16_t>((sign << 15) | (newExp << 10) | (mant >> 13));
}

static float RadicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

static glm::vec2 Hammersley(uint32_t i, uint32_t N) {
    return glm::vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
}

static glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * 3.14159265359f * Xi.x;
    float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

    glm::vec3 H;
    H.x = std::cos(phi) * sinTheta;
    H.y = std::sin(phi) * sinTheta;
    H.z = cosTheta;

    glm::vec3 up = std::abs(N.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, N));
    glm::vec3 bitangent = glm::cross(N, tangent);

    glm::vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return glm::normalize(sampleVec);
}

static float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness * roughness) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

static float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

static glm::vec2 IntegrateBRDF(float NdotV, float roughness) {
    glm::vec3 V;
    V.x = std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV));
    V.y = 0.0f;
    V.z = NdotV;

    float A = 0.0f;
    float B = 0.0f;
    glm::vec3 N = glm::vec3(0.0f, 0.0f, 1.0f);

    const uint32_t SAMPLE_COUNT = 128u;
    for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
        glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

        float NdotL = std::max(L.z, 0.0f);
        float NdotH = std::max(H.z, 0.0f);
        float VdotH = std::max(glm::dot(V, H), 0.0f);

        if (NdotL > 0.0f) {
            float G = GeometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = std::pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= static_cast<float>(SAMPLE_COUNT);
    B /= static_cast<float>(SAMPLE_COUNT);
    return glm::vec2(A, B);
}

static glm::vec3 GetCubeRayDir(int face, float u, float v) {
    switch (face) {
        case 0: return glm::normalize(glm::vec3( 1.0f,   -v,   -u)); // +X
        case 1: return glm::normalize(glm::vec3(-1.0f,   -v,    u)); // -X
        case 2: return glm::normalize(glm::vec3(    u, 1.0f,    v)); // +Y
        case 3: return glm::normalize(glm::vec3(    u,-1.0f,   -v)); // -Y
        case 4: return glm::normalize(glm::vec3(    u,   -v, 1.0f)); // +Z
        case 5: return glm::normalize(glm::vec3(   -u,   -v,-1.0f)); // -Z
        default: return glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

static glm::vec3 EvaluateSkyRadiance(const glm::vec3& dir) {
    glm::vec3 d = glm::normalize(dir);

    glm::vec3 zenithColor  = glm::vec3(0.18f, 0.38f, 0.72f);
    glm::vec3 horizonColor = glm::vec3(0.65f, 0.75f, 0.85f);
    glm::vec3 groundColor  = glm::vec3(0.10f, 0.09f, 0.08f);

    glm::vec3 sky = (d.y >= 0.0f)
        ? glm::mix(horizonColor, zenithColor, std::pow(d.y, 0.65f))
        : glm::mix(horizonColor, groundColor, std::pow(-d.y, 0.45f));

    // Gunes parlakligi
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 0.8f, -0.4f));
    float sunDot = std::max(glm::dot(d, sunDir), 0.0f);
    glm::vec3 sunRadiance = glm::vec3(1.2f, 1.05f, 0.85f) * 4.0f;
    sky += sunRadiance * std::pow(sunDot, 64.0f);

    return sky;
}

static glm::vec3 EvaluateSkyIrradiance(const glm::vec3& dir) {
    glm::vec3 n = glm::normalize(dir);
    float t = 0.5f * (n.y + 1.0f);

    glm::vec3 groundAmbient = glm::vec3(0.10f, 0.10f, 0.12f);
    glm::vec3 skyAmbient    = glm::vec3(0.30f, 0.42f, 0.60f);
    glm::vec3 irradiance    = glm::mix(groundAmbient, skyAmbient, t);

    // Gunes difuz aydinlatmasi
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 0.8f, -0.4f));
    float sunDot = std::max(glm::dot(n, sunDir), 0.0f);
    irradiance += glm::vec3(1.0f, 0.9f, 0.75f) * (sunDot * 0.45f);

    return irradiance;
}

} // namespace

IBLManager::IBLManager(VulkanContext& context)
    : m_Context(context) {
    auto tTotalStart = std::chrono::high_resolution_clock::now();
    std::cout << "[Astral::IBLManager] IBL ortami (BRDF LUT + Cubemaps) baslatiliyor...\n";
    CreateSamplers();
    GenerateBRDFLUT();
    GenerateProceduralCubemaps();
    auto tTotalEnd = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(tTotalEnd - tTotalStart).count();
    std::cout << "[Astral::IBLManager] IBL baslatma tamamlandi (Toplam sure: " << totalMs << " ms).\n";
}

IBLManager::~IBLManager() {
    Cleanup();
}

void IBLManager::Cleanup() {
    VmaAllocator allocator = m_Context.GetAllocator();
    auto destroyImg = [allocator](VmaImage& img, vk::UniqueImageView& view) {
        view.reset();
        if (img.image && allocator != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, img.image, img.allocation);
            img.reset();
        }
    };

    destroyImg(m_PrefilteredImage, m_PrefilteredView);
    destroyImg(m_IrradianceImage, m_IrradianceView);
    destroyImg(m_BrdfLutImage, m_BrdfLutView);

    m_CubemapSampler.reset();
    m_BrdfLutSampler.reset();
}

void IBLManager::CreateSamplers() {
    vk::Device device = m_Context.GetDevice();

    // 1. 2D BRDF LUT Sampler (Linear, Clamp to edge)
    vk::SamplerCreateInfo lutSamplerInfo{};
    lutSamplerInfo.magFilter = vk::Filter::eLinear;
    lutSamplerInfo.minFilter = vk::Filter::eLinear;
    lutSamplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    lutSamplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    lutSamplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    lutSamplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    lutSamplerInfo.minLod = 0.0f;
    lutSamplerInfo.maxLod = 1.0f;
    m_BrdfLutSampler = device.createSamplerUnique(lutSamplerInfo);

    // 2. Cubemap Sampler (Linear, Clamp to edge, Mipmapped)
    vk::SamplerCreateInfo cubeSamplerInfo{};
    cubeSamplerInfo.magFilter = vk::Filter::eLinear;
    cubeSamplerInfo.minFilter = vk::Filter::eLinear;
    cubeSamplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    cubeSamplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    cubeSamplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    cubeSamplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    cubeSamplerInfo.minLod = 0.0f;
    cubeSamplerInfo.maxLod = static_cast<float>(m_PrefilteredMipLevels);
    m_CubemapSampler = device.createSamplerUnique(cubeSamplerInfo);
}

void IBLManager::GenerateBRDFLUT() {
    auto tStart = std::chrono::high_resolution_clock::now();
    constexpr uint32_t LUT_SIZE = 256;
    const size_t totalElements = LUT_SIZE * LUT_SIZE * 2;
    const size_t dataSize = totalElements * sizeof(uint16_t);
    std::vector<uint16_t> lutData(totalElements);

    // 1. Disk onbellegini kontrol et
    const std::filesystem::path cachePath = "assets/cache/brdf_lut_256.bin";
    bool loadedFromCache = false;

    if (std::filesystem::exists(cachePath) && std::filesystem::file_size(cachePath) == dataSize) {
        std::ifstream file(cachePath, std::ios::binary);
        if (file.is_open()) {
            file.read(reinterpret_cast<char*>(lutData.data()), dataSize);
            if (file.gcount() == static_cast<std::streamsize>(dataSize)) {
                loadedFromCache = true;
            }
        }
    }

    // 2. Eger onbellek yoksa coklu is parcacigi (multi-threading) ile hesapla
    if (!loadedFromCache) {
        uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> workers;
        uint32_t rowsPerThread = (LUT_SIZE + numThreads - 1) / numThreads;

        for (uint32_t t = 0; t < numThreads; ++t) {
            uint32_t startY = t * rowsPerThread;
            uint32_t endY = std::min(startY + rowsPerThread, LUT_SIZE);
            if (startY >= endY) break;

            workers.emplace_back([&lutData, startY, endY]() {
                for (uint32_t y = startY; y < endY; ++y) {
                    float roughness = std::max((static_cast<float>(y) + 0.5f) / static_cast<float>(LUT_SIZE), 0.001f);
                    for (uint32_t x = 0; x < LUT_SIZE; ++x) {
                        float NdotV = std::max((static_cast<float>(x) + 0.5f) / static_cast<float>(LUT_SIZE), 0.001f);
                        glm::vec2 integrated = IntegrateBRDF(NdotV, roughness);

                        size_t idx = (y * LUT_SIZE + x) * 2;
                        lutData[idx + 0] = FloatToHalf(integrated.x);
                        lutData[idx + 1] = FloatToHalf(integrated.y);
                    }
                }
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        // Onbellegi diske kaydet
        std::error_code ec;
        std::filesystem::create_directories(cachePath.parent_path(), ec);
        std::ofstream outFile(cachePath, std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(lutData.data()), dataSize);
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "[Astral::IBLManager] 2D BRDF LUT hazirlandi (" << ms << " ms, " 
              << (loadedFromCache ? "Disk Onbelleginden yuklendi" : "Coklu is parcacigiyla uretildi ve onbelgeye yazildi") << ").\n";

    VkDeviceSize uploadDataSize = dataSize;

    // Staging Buffer
    Buffer stagingBuffer(
        m_Context.GetAllocator(),
        uploadDataSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        true
    );
    stagingBuffer.UpdateData(lutData.data(), uploadDataSize);

    // GPU Hedef Görüntüsü
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.extent = { LUT_SIZE, LUT_SIZE, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage rawImg = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkResult res = vmaCreateImage(m_Context.GetAllocator(), &imageInfo, &allocInfo, &rawImg, &alloc, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Astral::IBLManager] BRDF LUT vmaCreateImage basarisiz! Res: " + std::to_string(res));
    }
    m_BrdfLutImage.image = rawImg;
    m_BrdfLutImage.allocation = alloc;

    // GPU'ya Aktar ve Layout Gecisi Yap
    m_Context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier toDstBarrier{};
        toDstBarrier.oldLayout = vk::ImageLayout::eUndefined;
        toDstBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDstBarrier.image = m_BrdfLutImage.get();
        toDstBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        toDstBarrier.srcAccessMask = {};
        toDstBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr, 0, nullptr, 1, &toDstBarrier
        );

        vk::BufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        copyRegion.imageExtent = vk::Extent3D(LUT_SIZE, LUT_SIZE, 1);

        cmd.copyBufferToImage(
            stagingBuffer.GetBuffer(),
            m_BrdfLutImage.get(),
            vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion
        );

        vk::ImageMemoryBarrier toSampledBarrier{};
        toSampledBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        toSampledBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toSampledBarrier.image = m_BrdfLutImage.get();
        toSampledBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        toSampledBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        toSampledBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
            {},
            0, nullptr, 0, nullptr, 1, &toSampledBarrier
        );
    });

    // Image View
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_BrdfLutImage.get();
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR16G16Sfloat;
    viewInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    m_BrdfLutView = m_Context.GetDevice().createImageViewUnique(viewInfo);
}

void IBLManager::GenerateProceduralCubemaps() {
    auto tStart = std::chrono::high_resolution_clock::now();
    // ---------------------------------------------------------
    // 1. Irradiance Cubemap (32x32, 6 yuz, 1 mip)
    // ---------------------------------------------------------
    constexpr uint32_t IRRAD_SIZE = 32;
    std::vector<uint16_t> irradData(IRRAD_SIZE * IRRAD_SIZE * 6 * 4); // 4 kanalli R16G16B16A16

    for (int face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < IRRAD_SIZE; ++y) {
            float v = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(IRRAD_SIZE);
            for (uint32_t x = 0; x < IRRAD_SIZE; ++x) {
                float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(IRRAD_SIZE) - 1.0f;
                glm::vec3 dir = GetCubeRayDir(face, u, v);
                glm::vec3 col = EvaluateSkyIrradiance(dir);

                size_t idx = ((face * IRRAD_SIZE + y) * IRRAD_SIZE + x) * 4;
                irradData[idx + 0] = FloatToHalf(col.r);
                irradData[idx + 1] = FloatToHalf(col.g);
                irradData[idx + 2] = FloatToHalf(col.b);
                irradData[idx + 3] = FloatToHalf(1.0f);
            }
        }
    }

    VkDeviceSize irradDataSize = irradData.size() * sizeof(uint16_t);
    Buffer irradStaging(
        m_Context.GetAllocator(),
        irradDataSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        true
    );
    irradStaging.UpdateData(irradData.data(), irradDataSize);

    VkImageCreateInfo irradImageInfo{};
    irradImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    irradImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    irradImageInfo.imageType = VK_IMAGE_TYPE_2D;
    irradImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    irradImageInfo.extent = { IRRAD_SIZE, IRRAD_SIZE, 1 };
    irradImageInfo.mipLevels = 1;
    irradImageInfo.arrayLayers = 6;
    irradImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    irradImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    irradImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    irradImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    irradImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo irradAllocInfo{};
    irradAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage rawIrrad = VK_NULL_HANDLE;
    VmaAllocation allocIrrad = VK_NULL_HANDLE;
    VkResult resIrrad = vmaCreateImage(m_Context.GetAllocator(), &irradImageInfo, &irradAllocInfo, &rawIrrad, &allocIrrad, nullptr);
    if (resIrrad != VK_SUCCESS) {
        throw std::runtime_error("[Astral::IBLManager] Irradiance Cubemap vmaCreateImage basarisiz!");
    }
    m_IrradianceImage.image = rawIrrad;
    m_IrradianceImage.allocation = allocIrrad;

    m_Context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier toDst{};
        toDst.oldLayout = vk::ImageLayout::eUndefined;
        toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDst.image = m_IrradianceImage.get();
        toDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 };
        toDst.srcAccessMask = {};
        toDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr, 0, nullptr, 1, &toDst
        );

        std::vector<vk::BufferImageCopy> copyRegions(6);
        for (uint32_t f = 0; f < 6; ++f) {
            copyRegions[f].bufferOffset = f * IRRAD_SIZE * IRRAD_SIZE * 4 * sizeof(uint16_t);
            copyRegions[f].imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, f, 1 };
            copyRegions[f].imageExtent = vk::Extent3D(IRRAD_SIZE, IRRAD_SIZE, 1);
        }

        cmd.copyBufferToImage(
            irradStaging.GetBuffer(),
            m_IrradianceImage.get(),
            vk::ImageLayout::eTransferDstOptimal,
            static_cast<uint32_t>(copyRegions.size()), copyRegions.data()
        );

        vk::ImageMemoryBarrier toSampled{};
        toSampled.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        toSampled.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toSampled.image = m_IrradianceImage.get();
        toSampled.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 };
        toSampled.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        toSampled.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
            {},
            0, nullptr, 0, nullptr, 1, &toSampled
        );
    });

    vk::ImageViewCreateInfo irradViewInfo{};
    irradViewInfo.image = m_IrradianceImage.get();
    irradViewInfo.viewType = vk::ImageViewType::eCube;
    irradViewInfo.format = vk::Format::eR16G16B16A16Sfloat;
    irradViewInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 };
    m_IrradianceView = m_Context.GetDevice().createImageViewUnique(irradViewInfo);

    // ---------------------------------------------------------
    // 2. Prefiltered Environment Cubemap (128x128, 5 mips, 6 yuz)
    // ---------------------------------------------------------
    constexpr uint32_t PREFILTER_BASE_SIZE = 128;
    m_PrefilteredMipLevels = 5;

    // Toplam veri boyutunu hesapla
    size_t totalPrefilterFloats = 0;
    for (uint32_t mip = 0; mip < m_PrefilteredMipLevels; ++mip) {
        uint32_t mipSize = std::max(1u, PREFILTER_BASE_SIZE >> mip);
        totalPrefilterFloats += mipSize * mipSize * 6 * 4;
    }

    std::vector<uint16_t> prefilterData(totalPrefilterFloats);
    std::vector<vk::BufferImageCopy> prefilterCopyRegions;

    size_t currentOffsetBytes = 0;
    size_t currentFloatIdx = 0;

    for (uint32_t mip = 0; mip < m_PrefilteredMipLevels; ++mip) {
        uint32_t mipSize = std::max(1u, PREFILTER_BASE_SIZE >> mip);
        float roughness = static_cast<float>(mip) / static_cast<float>(m_PrefilteredMipLevels - 1);

        for (int face = 0; face < 6; ++face) {
            vk::BufferImageCopy region{};
            region.bufferOffset = currentOffsetBytes;
            region.imageSubresource = { vk::ImageAspectFlagBits::eColor, mip, static_cast<uint32_t>(face), 1 };
            region.imageExtent = vk::Extent3D(mipSize, mipSize, 1);
            prefilterCopyRegions.push_back(region);

            for (uint32_t y = 0; y < mipSize; ++y) {
                float v = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(mipSize);
                for (uint32_t x = 0; x < mipSize; ++x) {
                    float u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(mipSize) - 1.0f;
                    glm::vec3 dir = GetCubeRayDir(face, u, v);

                    // Mip seviyesi arttikca radiance difuz aydinlatmaya dogru yumusatilir (Roughness simülasyonu)
                    glm::vec3 sharp = EvaluateSkyRadiance(dir);
                    glm::vec3 diffuse = EvaluateSkyIrradiance(dir);
                    glm::vec3 col = glm::mix(sharp, diffuse, roughness);

                    prefilterData[currentFloatIdx + 0] = FloatToHalf(col.r);
                    prefilterData[currentFloatIdx + 1] = FloatToHalf(col.g);
                    prefilterData[currentFloatIdx + 2] = FloatToHalf(col.b);
                    prefilterData[currentFloatIdx + 3] = FloatToHalf(1.0f);
                    currentFloatIdx += 4;
                }
            }
            currentOffsetBytes += mipSize * mipSize * 4 * sizeof(uint16_t);
        }
    }

    VkDeviceSize prefilterDataSize = prefilterData.size() * sizeof(uint16_t);
    Buffer prefilterStaging(
        m_Context.GetAllocator(),
        prefilterDataSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        true
    );
    prefilterStaging.UpdateData(prefilterData.data(), prefilterDataSize);

    VkImageCreateInfo prefilterImageInfo{};
    prefilterImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    prefilterImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    prefilterImageInfo.imageType = VK_IMAGE_TYPE_2D;
    prefilterImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    prefilterImageInfo.extent = { PREFILTER_BASE_SIZE, PREFILTER_BASE_SIZE, 1 };
    prefilterImageInfo.mipLevels = m_PrefilteredMipLevels;
    prefilterImageInfo.arrayLayers = 6;
    prefilterImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    prefilterImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    prefilterImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    prefilterImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    prefilterImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo prefilterAllocInfo{};
    prefilterAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage rawPrefilter = VK_NULL_HANDLE;
    VmaAllocation allocPrefilter = VK_NULL_HANDLE;
    VkResult resPrefilter = vmaCreateImage(m_Context.GetAllocator(), &prefilterImageInfo, &prefilterAllocInfo, &rawPrefilter, &allocPrefilter, nullptr);
    if (resPrefilter != VK_SUCCESS) {
        throw std::runtime_error("[Astral::IBLManager] Prefiltered Cubemap vmaCreateImage basarisiz!");
    }
    m_PrefilteredImage.image = rawPrefilter;
    m_PrefilteredImage.allocation = allocPrefilter;

    m_Context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier toDst{};
        toDst.oldLayout = vk::ImageLayout::eUndefined;
        toDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toDst.image = m_PrefilteredImage.get();
        toDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, m_PrefilteredMipLevels, 0, 6 };
        toDst.srcAccessMask = {};
        toDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr, 0, nullptr, 1, &toDst
        );

        cmd.copyBufferToImage(
            prefilterStaging.GetBuffer(),
            m_PrefilteredImage.get(),
            vk::ImageLayout::eTransferDstOptimal,
            static_cast<uint32_t>(prefilterCopyRegions.size()), prefilterCopyRegions.data()
        );

        vk::ImageMemoryBarrier toSampled{};
        toSampled.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        toSampled.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        toSampled.image = m_PrefilteredImage.get();
        toSampled.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, m_PrefilteredMipLevels, 0, 6 };
        toSampled.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        toSampled.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
            {},
            0, nullptr, 0, nullptr, 1, &toSampled
        );
    });

    vk::ImageViewCreateInfo prefilterViewInfo{};
    prefilterViewInfo.image = m_PrefilteredImage.get();
    prefilterViewInfo.viewType = vk::ImageViewType::eCube;
    prefilterViewInfo.format = vk::Format::eR16G16B16A16Sfloat;
    prefilterViewInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, m_PrefilteredMipLevels, 0, 6 };
    m_PrefilteredView = m_Context.GetDevice().createImageViewUnique(prefilterViewInfo);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "[Astral::IBLManager] Procedural Cubemap'ler hazirlandi (" << ms << " ms).\n";
}

} // namespace Astral
