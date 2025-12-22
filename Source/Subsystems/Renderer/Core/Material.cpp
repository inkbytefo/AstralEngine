#include "Material.h"
#include <fstream>
#include <iostream>

namespace AstralEngine {

    Material::Material(IRHIDevice* device, const MaterialData& data, IRHIDescriptorSetLayout* globalLayout)
        : m_device(device), m_data(data) {
        
        CreateUniformBuffer();
        CreatePipeline(data.vertexShaderPath, data.fragmentShaderPath, globalLayout);
        CreateDescriptorSet();
    }

    Material::~Material() {
    }

    void Material::SetAlbedoMap(std::shared_ptr<Texture> texture) {
        m_albedoMap = texture;
    }

    std::vector<uint8_t> Material::ReadShaderFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Material failed to open shader file: " + filepath);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<uint8_t> buffer(fileSize);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

    void Material::CreateUniformBuffer() {
        // Create buffer for MaterialUniforms
        m_uniformBuffer = m_device->CreateBuffer(
            sizeof(MaterialUniforms),
            RHIBufferUsage::Uniform,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );

        // Initialize with default data
        MaterialUniforms uniforms{};
        uniforms.baseColor = glm::vec4(m_data.properties.baseColor, m_data.properties.opacity);
        uniforms.metallic = m_data.properties.metallic;
        uniforms.roughness = m_data.properties.roughness;
        uniforms.ao = m_data.properties.ao;
        uniforms.padding = 0.0f;
        
        void* data = m_uniformBuffer->Map();
        memcpy(data, &uniforms, sizeof(MaterialUniforms));
        m_uniformBuffer->Unmap();
    }

    void Material::CreateDescriptorSet() {
        if (!m_descriptorSetLayout) return;

        m_descriptorSet = m_device->AllocateDescriptorSet(m_descriptorSetLayout.get());
        UpdateDescriptorSet();
    }

    void Material::UpdateDescriptorSet() {
        if (!m_descriptorSet) return;

        // Binding 0: Material Uniforms
        if (m_uniformBuffer) {
            m_descriptorSet->UpdateUniformBuffer(0, m_uniformBuffer.get(), 0, sizeof(MaterialUniforms));
        }

        // Binding 1: Albedo Map
        if (m_albedoMap) {
            m_descriptorSet->UpdateCombinedImageSampler(1, m_albedoMap->GetRHITexture(), m_albedoMap->GetRHISampler());
        }
    }

    void Material::CreatePipeline(const std::string& vertPath, const std::string& fragPath, IRHIDescriptorSetLayout* globalLayout) {
        // 1. Create Descriptor Set Layout (Set 1: Material)
        std::vector<RHIDescriptorSetLayoutBinding> bindings;
        
        // Binding 0: Material UBO (Fragment Stage)
        RHIDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(uboBinding);

        // Binding 1: Sampler (Fragment Stage)
        RHIDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 1;
        samplerBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = RHIShaderStage::Fragment;
        bindings.push_back(samplerBinding);
        
        m_descriptorSetLayout = m_device->CreateDescriptorSetLayout(bindings);

        // 2. Load Shaders
        auto vertCode = ReadShaderFile(vertPath);
        auto fragCode = ReadShaderFile(fragPath);
        
        auto vertexShader = m_device->CreateShader(RHIShaderStage::Vertex, std::span<const uint8_t>(vertCode.data(), vertCode.size()));
        auto fragmentShader = m_device->CreateShader(RHIShaderStage::Fragment, std::span<const uint8_t>(fragCode.data(), fragCode.size()));

        // 3. Create Pipeline
        RHIPipelineStateDescriptor pipelineDesc{};
        pipelineDesc.vertexShader = vertexShader.get();
        pipelineDesc.fragmentShader = fragmentShader.get();
        
        // Set 0: Global (Camera/Scene)
        if (globalLayout) {
            pipelineDesc.descriptorSetLayouts.push_back(globalLayout);
        }
        // Set 1: Material
        pipelineDesc.descriptorSetLayouts.push_back(m_descriptorSetLayout.get());

        // Vertex Bindings (Standard Layout - Hardcoded for now)
        RHIVertexInputBinding binding{};
        binding.binding = 0;
        binding.stride = 56; // sizeof(Vertex)
        binding.isInstanced = false;
        pipelineDesc.vertexBindings.push_back(binding);

        // Vertex Attributes
        // Position
        RHIVertexInputAttribute posAttr{};
        posAttr.location = 0; posAttr.binding = 0;
        posAttr.format = RHIFormat::R32G32B32_FLOAT;
        posAttr.offset = 0;
        pipelineDesc.vertexAttributes.push_back(posAttr);

        // Normal
        RHIVertexInputAttribute normAttr{};
        normAttr.location = 1; normAttr.binding = 0;
        normAttr.format = RHIFormat::R32G32B32_FLOAT;
        normAttr.offset = 12;
        pipelineDesc.vertexAttributes.push_back(normAttr);

        // TexCoord
        RHIVertexInputAttribute uvAttr{};
        uvAttr.location = 2; uvAttr.binding = 0;
        uvAttr.format = RHIFormat::R32G32_FLOAT;
        uvAttr.offset = 24;
        pipelineDesc.vertexAttributes.push_back(uvAttr);
        
        // Tangent
        RHIVertexInputAttribute tanAttr{};
        tanAttr.location = 3; tanAttr.binding = 0;
        tanAttr.format = RHIFormat::R32G32B32_FLOAT;
        tanAttr.offset = 32;
        pipelineDesc.vertexAttributes.push_back(tanAttr);

        // Bitangent
        RHIVertexInputAttribute bitanAttr{};
        bitanAttr.location = 4; bitanAttr.binding = 0;
        bitanAttr.format = RHIFormat::R32G32B32_FLOAT;
        bitanAttr.offset = 44;
        pipelineDesc.vertexAttributes.push_back(bitanAttr);

        // Rasterizer
        pipelineDesc.cullMode = RHICullMode::Back;
        pipelineDesc.frontFace = RHIFrontFace::CounterClockwise;

        // Depth Stencil
        pipelineDesc.depthTestEnabled = true;
        pipelineDesc.depthWriteEnabled = true;
        pipelineDesc.depthCompareOp = RHICompareOp::Less;

        m_pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);
    }

}
