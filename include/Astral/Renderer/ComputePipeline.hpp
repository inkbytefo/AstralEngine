#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "Astral/Renderer/ShaderInterop.hpp"

namespace Astral {

class ComputePipeline {
public:
    ComputePipeline(vk::Device device, const std::string& spvPath);
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    vk::Pipeline GetPipeline() const { return m_Pipeline.get(); }
    vk::PipelineLayout GetPipelineLayout() const { return m_PipelineLayout.get(); }
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout.get(); }

private:
    vk::Device m_Device;
    vk::UniqueShaderModule m_ShaderModule;
    vk::UniqueDescriptorSetLayout m_DescriptorSetLayout;
    vk::UniquePipelineLayout m_PipelineLayout;
    vk::UniquePipeline m_Pipeline;

    std::vector<char> ReadFile(const std::string& filepath);
    void CreateDescriptorSetLayout();
    void CreatePipeline(const std::string& spvPath);
};

} // namespace Astral
