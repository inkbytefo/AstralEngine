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

namespace Astral {

struct SDFPushConstants {
    glm::vec4 camPos;     // xyz: pos, w: time
    glm::vec4 camDir;     // xyz: dir, w: normalMode (0=central, 1=tetrahedron)
    glm::vec4 screenRes;  // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    glm::vec4 gridParams; // x: dimX, y: dimY, z: dimZ, w: cellSize
};

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
