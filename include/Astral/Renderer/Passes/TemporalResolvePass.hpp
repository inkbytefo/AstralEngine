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
#include <array>
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Renderer/TemporalState.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Renderer/ComputeProgram.hpp"

namespace Astral {

struct ResolveInputs {
    GBufferViews gbuffer;
    ImageViewRef rawColor;
    ImageViewRef output;
    std::array<ImageViewRef, 2> historyColor;
    std::array<ImageViewRef, 2> historyExtra;
    vk::Sampler historySampler{};
};

class TemporalResolvePass {
public:
    TemporalResolvePass(vk::Device device, const std::string& spvPath);
    ~TemporalResolvePass() = default;

    TemporalResolvePass(const TemporalResolvePass&) = delete;
    TemporalResolvePass& operator=(const TemporalResolvePass&) = delete;
    TemporalResolvePass(TemporalResolvePass&&) noexcept = default;
    TemporalResolvePass& operator=(TemporalResolvePass&&) noexcept = default;

    void BindResources(const ResolveInputs& inputs);
    void Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings,
                const TemporalPlan& temporalPlan, const SDFChangeSet& changeSet);

    [[nodiscard]] vk::DescriptorSet GetDescriptorSet(uint32_t index) const {
        return m_DescriptorSets.at(index);
    }
    [[nodiscard]] const ComputeProgram* GetProgram() const noexcept { return m_Program.get(); }

    static std::vector<vk::DescriptorSetLayoutBinding> GetBindingDescriptions();

private:
    vk::Device m_Device;
    std::unique_ptr<ComputeProgram> m_Program;
    vk::UniqueDescriptorPool m_DescriptorPool;
    std::array<vk::DescriptorSet, 2> m_DescriptorSets{};
    ResolveInputs m_Inputs{};
};

} // namespace Astral
