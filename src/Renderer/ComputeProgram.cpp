#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/ComputeProgram.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <array>
#include <cstdio>

namespace Astral {

std::vector<uint32_t> ComputeProgram::ReadSpirv(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("[Astral::ComputeProgram] SPIR-V dosyasi bulunamadi: " + path.string());
    }

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("[Astral::ComputeProgram] SPIR-V dosyasi acilamadi: " + path.string());
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        throw std::runtime_error("[Astral::ComputeProgram] SPIR-V dosyasi bos (0 bayt): " + path.string());
    }

    if (fileSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("[Astral::ComputeProgram] SPIR-V dosya boyutu (" + std::to_string(fileSize) +
                                 " bayt) 4 baytin kati olmalidir: " + path.string());
    }

    if (fileSize < 20) { // Minimum 5 uint32 words for header: Magic, Version, Generator, Bound, Reserved
        throw std::runtime_error("[Astral::ComputeProgram] Eksik SPIR-V basligi (< 20 bayt): " + path.string());
    }

    const size_t wordCount = fileSize / sizeof(uint32_t);
    std::vector<uint32_t> words(wordCount);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(fileSize));
    if (!file) {
        throw std::runtime_error("[Astral::ComputeProgram] SPIR-V dosyasi tam okunamadi: " + path.string());
    }

    constexpr uint32_t SPIRV_MAGIC_LITTLE_ENDIAN = 0x07230203u;
    constexpr uint32_t SPIRV_MAGIC_BIG_ENDIAN = 0x03022307u;
    if (words[0] != SPIRV_MAGIC_LITTLE_ENDIAN && words[0] != SPIRV_MAGIC_BIG_ENDIAN) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%08X", words[0]);
        throw std::runtime_error("[Astral::ComputeProgram] Gecersiz SPIR-V magic numarasi (0x" +
                                 std::string(buf) + "): " + path.string());
    }

    return words;
}

std::filesystem::path ComputeProgram::ResolveShaderPath(const std::filesystem::path& requestedPath,
                                                        const std::filesystem::path& hintBasePath) {
    if (requestedPath.empty()) {
        throw std::invalid_argument("[Astral::ComputeProgram] Shader dosya adi bos olamaz.");
    }

    if (std::filesystem::exists(requestedPath)) {
        return requestedPath;
    }

    const std::filesystem::path filename = requestedPath.filename();

    // 1. Hint ana yolun parent'i
    if (!hintBasePath.empty()) {
        std::filesystem::path cand = hintBasePath.parent_path() / filename;
        if (std::filesystem::exists(cand)) {
            return cand;
        }
    }

    // 2. Fallback dizinleri
#ifdef SHADER_BIN_DIR
    std::filesystem::path binDirCand = std::filesystem::path(SHADER_BIN_DIR) / filename;
    if (std::filesystem::exists(binDirCand)) {
        return binDirCand;
    }
#endif

    const std::array<std::filesystem::path, 3> searchDirs = {
        std::filesystem::path("build-release/shaders"),
        std::filesystem::path("build/shaders"),
        std::filesystem::path("shaders")
    };

    for (const auto& dir : searchDirs) {
        std::filesystem::path cand = dir / filename;
        if (std::filesystem::exists(cand)) {
            return cand;
        }
    }

    throw std::runtime_error("[Astral::ComputeProgram] Shader dosyasi hicbir arama yolunda bulunamadi: " +
                             requestedPath.string());
}

ComputeProgram::ComputeProgram(vk::Device device, const ComputeProgramDesc& desc)
    : m_Device(device) {
    if (!m_Device) {
        throw std::runtime_error("[Astral::ComputeProgram] Gecersiz Vulkan device handle'i!");
    }

    // 1. Descriptor Set Layout olustur
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(desc.bindings.size());
    layoutInfo.pBindings = desc.bindings.data();
    m_DescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);

    // 2. Pipeline Layout olustur
    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = desc.pushConstantBytes;

    vk::PipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.setLayoutCount = 1;
    auto rawLayout = m_DescriptorSetLayout.get();
    pipeLayoutInfo.pSetLayouts = &rawLayout;
    if (desc.pushConstantBytes > 0) {
        pipeLayoutInfo.pushConstantRangeCount = 1;
        pipeLayoutInfo.pPushConstantRanges = &pushRange;
    } else {
        pipeLayoutInfo.pushConstantRangeCount = 0;
        pipeLayoutInfo.pPushConstantRanges = nullptr;
    }
    m_PipelineLayout = m_Device.createPipelineLayoutUnique(pipeLayoutInfo);

    // 3. SPIR-V oku ve ShaderModule olustur
    auto spirvWords = ReadSpirv(desc.shaderPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = spirvWords.size() * sizeof(uint32_t);
    moduleInfo.pCode = spirvWords.data();
    m_ShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    // 4. Compute Pipeline olustur
    vk::ComputePipelineCreateInfo pipeInfo{};
    pipeInfo.layout = m_PipelineLayout.get();
    pipeInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
    pipeInfo.stage.module = m_ShaderModule.get();
    pipeInfo.stage.pName = "main";

    auto result = m_Device.createComputePipelineUnique(nullptr, pipeInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("[Astral::ComputeProgram] Compute pipeline olusturulamadi: " + desc.shaderPath.string());
    }
    m_Pipeline = std::move(result.value);
}

} // namespace Astral
