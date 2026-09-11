#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <memory>
#include <string>
#include <vector>
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/SceneGpuData.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Renderer/ComputeProgram.hpp"

namespace Astral {

struct GBufferInputs {
    GBufferViews targets;
    SceneBufferViews scene;
    vk::DescriptorBufferInfo camera{};
    vk::DescriptorBufferInfo selection{};
};

class GBufferPass {
public:
    GBufferPass(vk::Device device, const std::string& spvPath);
    ~GBufferPass() = default;

    GBufferPass(const GBufferPass&) = delete;
    GBufferPass& operator=(const GBufferPass&) = delete;
    GBufferPass(GBufferPass&&) noexcept = default;
    GBufferPass& operator=(GBufferPass&&) noexcept = default;

    void BindResources(const GBufferInputs& inputs);
    void Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings);

    [[nodiscard]] vk::DescriptorSet GetDescriptorSet() const noexcept { return m_DescriptorSet; }
    [[nodiscard]] const ComputeProgram* GetProgram() const noexcept { return m_Program.get(); }

    static std::vector<vk::DescriptorSetLayoutBinding> GetBindingDescriptions();

private:
    vk::Device m_Device;
    std::unique_ptr<ComputeProgram> m_Program;
    vk::UniqueDescriptorPool m_DescriptorPool;
    vk::DescriptorSet m_DescriptorSet{};
    GBufferInputs m_Inputs{};
};

} // namespace Astral
