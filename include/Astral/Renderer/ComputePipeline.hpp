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

struct alignas(16) SelectionDataGPU {
    int32_t hitIndex = -1;
    int32_t pad0 = 0;
    int32_t pad1 = 0;
    int32_t pad2 = 0;
    glm::vec4 hitPoint{0.0f}; // xyz: hitPoint, w: hitDistance
};
static_assert(sizeof(SelectionDataGPU) == 32, "SelectionDataGPU boyutu 32 bayt olmalidir!");

struct SDFPushConstants {
    glm::vec4 camPos;      // xyz: pos, w: time
    glm::vec4 camDir;      // xyz: dir, w: normalMode (0=central, 1=tetrahedron)
    glm::vec4 screenRes;   // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    glm::vec4 gridParams;  // x: dimX, y: dimY, z: optShadow, w: cellSize
    glm::vec4 taaParams;   // x: jitterX, y: jitterY, z: taaEnabled, w: blendAlpha
    glm::vec4 mouseParams; // x: mouseX, y: mouseY, z: pickRequested (0/1), w: selectedHitIndex (-1 = none)
};
static_assert(sizeof(SDFPushConstants) == 96, "SDFPushConstants boyutu 96 bayt olmalidir!");

struct TAAPushConstants {
    glm::vec4 screenRes; // x: width, y: height, z: frameIndex, w: blendAlpha
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
