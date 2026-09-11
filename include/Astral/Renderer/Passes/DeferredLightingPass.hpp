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

struct LightingInputs {
    GBufferViews gbuffer;
    ImageViewRef output;
    SceneBufferViews scene;
    vk::DescriptorImageInfo irradiance{};
    vk::DescriptorImageInfo prefiltered{};
    vk::DescriptorImageInfo brdf{};
    uint32_t prefilteredMipLevels = 0;
};

class DeferredLightingPass {
public:
    DeferredLightingPass(vk::Device device, const std::string& spvPath);
    ~DeferredLightingPass() = default;

    DeferredLightingPass(const DeferredLightingPass&) = delete;
    DeferredLightingPass& operator=(const DeferredLightingPass&) = delete;
    DeferredLightingPass(DeferredLightingPass&&) noexcept = default;
    DeferredLightingPass& operator=(DeferredLightingPass&&) noexcept = default;

    void BindResources(const LightingInputs& inputs);
    void Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings);

    [[nodiscard]] vk::DescriptorSet GetDescriptorSet() const noexcept { return m_DescriptorSet; }
    [[nodiscard]] const ComputeProgram* GetProgram() const noexcept { return m_Program.get(); }

    static std::vector<vk::DescriptorSetLayoutBinding> GetBindingDescriptions();

private:
    vk::Device m_Device;
    std::unique_ptr<ComputeProgram> m_Program;
    vk::UniqueDescriptorPool m_DescriptorPool;
    vk::DescriptorSet m_DescriptorSet{};
    LightingInputs m_Inputs{};
};

} // namespace Astral
