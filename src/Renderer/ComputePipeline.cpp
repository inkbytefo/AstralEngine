#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/ComputePipeline.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Astral {

ComputePipeline::ComputePipeline(vk::Device device, const std::string& spvPath)
    : m_Device(device) {
    CreateDescriptorSetLayout();
    CreatePipeline(spvPath);
}

ComputePipeline::~ComputePipeline() {
    m_Pipeline.reset();
    m_PipelineLayout.reset();
    m_DescriptorSetLayout.reset();
    m_ShaderModule.reset();
}

std::vector<char> ComputePipeline::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("SPIR-V shader dosyasi acilamadi: " + filepath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void ComputePipeline::CreateDescriptorSetLayout() {
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: Storage Image (outImage)
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 1: Storage Buffer (EditBuffer SSBO)
    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 2: Storage Buffer (GridBuffer SSBO - PR-6)
    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    m_DescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);
}

void ComputePipeline::CreatePipeline(const std::string& spvPath) {
    auto code = ReadFile(spvPath);

    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    m_ShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = sizeof(SDFPushConstants);

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setLayoutCount = 1;
    auto rawLayout = m_DescriptorSetLayout.get();
    layoutInfo.pSetLayouts = &rawLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    m_PipelineLayout = m_Device.createPipelineLayoutUnique(layoutInfo);

    vk::PipelineShaderStageCreateInfo stageInfo{};
    stageInfo.stage = vk::ShaderStageFlagBits::eCompute;
    stageInfo.module = m_ShaderModule.get();
    stageInfo.pName = "main";

    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = m_PipelineLayout.get();

    auto result = m_Device.createComputePipelineUnique(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Compute Pipeline olusturulamadi: " + spvPath);
    }
    m_Pipeline = std::move(result.value);

    std::cout << "[Astral::ComputePipeline] Compute pipeline basariyla olusturuldu: " << spvPath << "\n";
}

} // namespace Astral
