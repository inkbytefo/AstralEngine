#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace Astral {

struct ComputeProgramDesc {
    std::filesystem::path shaderPath;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    uint32_t pushConstantBytes = 0;
};

class ComputeProgram {
public:
    ComputeProgram(vk::Device device, const ComputeProgramDesc& desc);
    ~ComputeProgram() = default;

    ComputeProgram(const ComputeProgram&) = delete;
    ComputeProgram& operator=(const ComputeProgram&) = delete;

    ComputeProgram(ComputeProgram&&) noexcept = default;
    ComputeProgram& operator=(ComputeProgram&&) noexcept = default;

    [[nodiscard]] vk::Pipeline GetPipeline() const noexcept { return m_Pipeline.get(); }
    [[nodiscard]] vk::PipelineLayout GetLayout() const noexcept { return m_PipelineLayout.get(); }
    [[nodiscard]] vk::DescriptorSetLayout GetSetLayout() const noexcept { return m_DescriptorSetLayout.get(); }

    /// SPIR-V dosyasini uint32_t dizisi olarak okur; boyut, 4 bayt hizasi, baslik ve magic kontrolu yapar.
    static std::vector<uint32_t> ReadSpirv(const std::filesystem::path& path);

    /// Istenen shader yolunu arama sirasina gore cozer (acik yol -> hint parent_path -> build/shaders -> shaders fallback).
    static std::filesystem::path ResolveShaderPath(const std::filesystem::path& requestedPath,
                                                   const std::filesystem::path& hintBasePath = "");

private:
    vk::Device m_Device;
    vk::UniqueDescriptorSetLayout m_DescriptorSetLayout;
    vk::UniquePipelineLayout m_PipelineLayout;
    vk::UniqueShaderModule m_ShaderModule;
    vk::UniquePipeline m_Pipeline;
};

} // namespace Astral
