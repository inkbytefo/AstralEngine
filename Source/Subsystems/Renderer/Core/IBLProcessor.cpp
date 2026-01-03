#include "IBLProcessor.h"
#include "Core/Logger.h"
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

namespace AstralEngine {

IBLProcessor::IBLProcessor(IRHIDevice* device) : m_device(device) {
    m_cubeMesh = Mesh::CreateCube(device);
    m_quadMesh = Mesh::CreateQuad(device);
    Logger::Info("IBLProcessor", "IBLProcessor initialized with helper meshes.");
}

std::shared_ptr<Texture> IBLProcessor::ConvertEquirectangularToCubemap(const std::shared_ptr<Texture>& equirectTexture, uint32_t size) {
    Logger::Info("IBLProcessor", "Converting equirectangular texture to cubemap ({}x{})...", size, size);

    auto cubemap = Texture::CreateCubemap(m_device, size, size, RHIFormat::R32G32B32A32_FLOAT);
    
    // Load shaders
    auto vertCode = LoadShaderCode("Assets/Shaders/Bin/IBL/EquirectangularToCubemap.slang.vert.spv");
    auto fragCode = LoadShaderCode("Assets/Shaders/Bin/IBL/EquirectangularToCubemap.slang.frag.spv");

    if (vertCode.empty() || fragCode.empty()) {
        Logger::Error("IBLProcessor", "Failed to load shaders for equirectangular conversion!");
        return nullptr;
    }

    auto vertShader = m_device->CreateShader(RHIShaderStage::Vertex, vertCode);
    auto fragShader = m_device->CreateShader(RHIShaderStage::Fragment, fragCode);

    // Create Descriptor Set Layout
    std::vector<RHIDescriptorSetLayoutBinding> bindings = {
        {0, RHIDescriptorType::CombinedImageSampler, 1, RHIShaderStage::Fragment}
    };
    auto layout = m_device->CreateDescriptorSetLayout(bindings);
    auto descriptorSet = m_device->AllocateDescriptorSet(layout.get());

    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = RHISamplerAddressMode::ClampToEdge;
    
    auto sampler = m_device->CreateSampler(samplerDesc);
    descriptorSet->UpdateCombinedImageSampler(0, equirectTexture->GetRHITexture(), sampler.get());

    // Create Pipeline
    RHIPipelineStateDescriptor pipelineDesc{};
    pipelineDesc.vertexShader = vertShader.get();
    pipelineDesc.fragmentShader = fragShader.get();
    pipelineDesc.descriptorSetLayouts = { layout.get() };
    pipelineDesc.colorFormats = { RHIFormat::R32G32B32A32_FLOAT };
    pipelineDesc.depthTestEnabled = false;
    pipelineDesc.pushConstants = { { RHIShaderStage::Vertex, 0, sizeof(glm::mat4) * 2 } };

    // Vertex Input
    pipelineDesc.vertexBindings = { { 0, sizeof(Vertex), false } };
    pipelineDesc.vertexAttributes = { { 0, 0, RHIFormat::R32G32B32_FLOAT, offsetof(Vertex, position) } };

    auto pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);

    // Render 6 faces
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    auto cmdList = m_device->CreateCommandList();
    cmdList->Begin();

    for (uint32_t i = 0; i < 6; ++i) {
        RHIRenderingAttachment colorAttachment{};
        colorAttachment.texture = cubemap->GetRHITexture();
        colorAttachment.arrayLayer = i;
        colorAttachment.mipLevel = 0;
        colorAttachment.clear = true;

        RHIRect2D renderArea{};
        renderArea.extent = { size, size };

        cmdList->BeginRendering({ colorAttachment }, nullptr, renderArea);
        
        cmdList->BindPipeline(pipeline.get());
        cmdList->BindDescriptorSet(pipeline.get(), descriptorSet.get(), 0);
        
        struct {
            glm::mat4 projection;
            glm::mat4 view;
        } pushConstants;
        pushConstants.projection = captureProjection;
        pushConstants.view = captureViews[i];
        
        cmdList->PushConstants(pipeline.get(), RHIShaderStage::Vertex, 0, sizeof(pushConstants), &pushConstants);
        
        m_cubeMesh->Draw(cmdList.get());
        
        cmdList->EndRendering();
    }

    // Final transition for the whole cubemap to SHADER_READ_ONLY_OPTIMAL
    cmdList->TransitionImageLayout(cubemap->GetRHITexture(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    cmdList->End();
    m_device->SubmitCommandList(cmdList.get());
    m_device->WaitIdle(); // Ensure conversion is finished
    
    Logger::Info("IBLProcessor", "Cubemap conversion completed.");
    return cubemap;
}

std::shared_ptr<Texture> IBLProcessor::CreateIrradianceMap(const std::shared_ptr<Texture>& cubemap, uint32_t size) {
    Logger::Info("IBLProcessor", "Generating Irradiance Map ({}x{})...", size, size);
    auto irradianceMap = Texture::CreateCubemap(m_device, size, size, RHIFormat::R32G32B32A32_FLOAT);
    
    auto vertCode = LoadShaderCode("Assets/Shaders/Bin/IBL/IrradianceConvolution.slang.vert.spv");
    auto fragCode = LoadShaderCode("Assets/Shaders/Bin/IBL/IrradianceConvolution.slang.frag.spv");

    if (vertCode.empty() || fragCode.empty()) {
        Logger::Error("IBLProcessor", "Failed to load shaders for irradiance convolution!");
        return nullptr;
    }

    auto vertShader = m_device->CreateShader(RHIShaderStage::Vertex, vertCode);
    auto fragShader = m_device->CreateShader(RHIShaderStage::Fragment, fragCode);

    std::vector<RHIDescriptorSetLayoutBinding> bindings = {
        {0, RHIDescriptorType::CombinedImageSampler, 1, RHIShaderStage::Fragment}
    };
    auto layout = m_device->CreateDescriptorSetLayout(bindings);
    auto descriptorSet = m_device->AllocateDescriptorSet(layout.get());

    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = RHISamplerAddressMode::ClampToEdge;
    auto sampler = m_device->CreateSampler(samplerDesc);

    descriptorSet->UpdateCombinedImageSampler(0, cubemap->GetRHITexture(), sampler.get());

    RHIPipelineStateDescriptor pipelineDesc{};
    pipelineDesc.vertexShader = vertShader.get();
    pipelineDesc.fragmentShader = fragShader.get();
    pipelineDesc.descriptorSetLayouts = { layout.get() };
    pipelineDesc.colorFormats = { RHIFormat::R32G32B32A32_FLOAT };
    pipelineDesc.depthTestEnabled = false;
    pipelineDesc.pushConstants = { { RHIShaderStage::Vertex, 0, sizeof(glm::mat4) * 2 } };

    // Vertex Input
    pipelineDesc.vertexBindings = { { 0, sizeof(Vertex), false } };
    pipelineDesc.vertexAttributes = { { 0, 0, RHIFormat::R32G32B32_FLOAT, offsetof(Vertex, position) } };

    auto pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    auto cmdList = m_device->CreateCommandList();
    cmdList->Begin();

    for (uint32_t i = 0; i < 6; ++i) {
        RHIRenderingAttachment colorAttachment{};
        colorAttachment.texture = irradianceMap->GetRHITexture();
        colorAttachment.arrayLayer = i;
        colorAttachment.mipLevel = 0;
        colorAttachment.clear = true;

        RHIRect2D renderArea{};
        renderArea.extent = { size, size };

        cmdList->BeginRendering({ colorAttachment }, nullptr, renderArea);
        cmdList->BindPipeline(pipeline.get());
        cmdList->BindDescriptorSet(pipeline.get(), descriptorSet.get(), 0);

        struct {
            glm::mat4 projection;
            glm::mat4 view;
        } pushConstants;
        pushConstants.projection = captureProjection;
        pushConstants.view = captureViews[i];
        cmdList->PushConstants(pipeline.get(), RHIShaderStage::Vertex, 0, sizeof(pushConstants), &pushConstants);

        m_cubeMesh->Draw(cmdList.get());
        cmdList->EndRendering();
    }

    // Final transition for the whole irradiance map
    cmdList->TransitionImageLayout(irradianceMap->GetRHITexture(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    cmdList->End();
    m_device->SubmitCommandList(cmdList.get());
    m_device->WaitIdle();

    Logger::Info("IBLProcessor", "Irradiance Map generation completed.");
    return irradianceMap;
}

std::shared_ptr<Texture> IBLProcessor::CreatePrefilteredMap(const std::shared_ptr<Texture>& cubemap, uint32_t size) {
    Logger::Info("IBLProcessor", "Generating Prefiltered Map ({}x{})...", size, size);
    uint32_t mipLevels = 5; 
    auto prefilteredMap = Texture::CreateCubemap(m_device, size, size, RHIFormat::R32G32B32A32_FLOAT, mipLevels);
    
    auto vertCode = LoadShaderCode("Assets/Shaders/Bin/IBL/Prefilter.slang.vert.spv");
    auto fragCode = LoadShaderCode("Assets/Shaders/Bin/IBL/Prefilter.slang.frag.spv");

    if (vertCode.empty() || fragCode.empty()) {
        Logger::Error("IBLProcessor", "Failed to load shaders for prefiltered map!");
        return nullptr;
    }

    auto vertShader = m_device->CreateShader(RHIShaderStage::Vertex, vertCode);
    auto fragShader = m_device->CreateShader(RHIShaderStage::Fragment, fragCode);

    std::vector<RHIDescriptorSetLayoutBinding> bindings = {
        {0, RHIDescriptorType::CombinedImageSampler, 1, RHIShaderStage::Fragment}
    };
    auto layout = m_device->CreateDescriptorSetLayout(bindings);
    auto descriptorSet = m_device->AllocateDescriptorSet(layout.get());

    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = RHISamplerAddressMode::ClampToEdge;
    auto sampler = m_device->CreateSampler(samplerDesc);

    descriptorSet->UpdateCombinedImageSampler(0, cubemap->GetRHITexture(), sampler.get());

    RHIPipelineStateDescriptor pipelineDesc{};
    pipelineDesc.vertexShader = vertShader.get();
    pipelineDesc.fragmentShader = fragShader.get();
    pipelineDesc.descriptorSetLayouts = { layout.get() };
    pipelineDesc.colorFormats = { RHIFormat::R32G32B32A32_FLOAT };
    pipelineDesc.depthTestEnabled = false;
    // Push constants: mat4 proj, mat4 view, float roughness
    pipelineDesc.pushConstants = { { RHIShaderStage::Vertex | RHIShaderStage::Fragment, 0, sizeof(glm::mat4) * 2 + sizeof(float) * 4 } };

    // Vertex Input
    pipelineDesc.vertexBindings = { { 0, sizeof(Vertex), false } };
    pipelineDesc.vertexAttributes = { { 0, 0, RHIFormat::R32G32B32_FLOAT, offsetof(Vertex, position) } };

    auto pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    auto cmdList = m_device->CreateCommandList();
    cmdList->Begin();

    for (uint32_t mip = 0; mip < mipLevels; ++mip) {
        uint32_t mipWidth = (uint32_t)(size * std::pow(0.5, mip));
        uint32_t mipHeight = (uint32_t)(size * std::pow(0.5, mip));
        float roughness = (float)mip / (float)(mipLevels - 1);

        for (uint32_t i = 0; i < 6; ++i) {
            RHIRenderingAttachment colorAttachment{};
            colorAttachment.texture = prefilteredMap->GetRHITexture();
            colorAttachment.arrayLayer = i;
            colorAttachment.mipLevel = mip;
            colorAttachment.clear = true;

            RHIRect2D renderArea{};
            renderArea.extent = { mipWidth, mipHeight };

            cmdList->BeginRendering({ colorAttachment }, nullptr, renderArea);
            cmdList->BindPipeline(pipeline.get());
            cmdList->BindDescriptorSet(pipeline.get(), descriptorSet.get(), 0);

            struct {
                glm::mat4 projection;
                glm::mat4 view;
                float roughness;
            } pushConstants;
            pushConstants.projection = captureProjection;
            pushConstants.view = captureViews[i];
            pushConstants.roughness = roughness;
            
            cmdList->PushConstants(pipeline.get(), RHIShaderStage::Vertex | RHIShaderStage::Fragment, 0, sizeof(pushConstants), &pushConstants);

            m_cubeMesh->Draw(cmdList.get());
            cmdList->EndRendering();
        }
    }

    // Final transition for the whole prefiltered map
    cmdList->TransitionImageLayout(prefilteredMap->GetRHITexture(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    cmdList->End();
    m_device->SubmitCommandList(cmdList.get());
    m_device->WaitIdle();

    Logger::Info("IBLProcessor", "Prefiltered Map generation completed.");
    return prefilteredMap;
}

std::shared_ptr<Texture> IBLProcessor::CreateBRDFLookUpTable(uint32_t size) {
    Logger::Info("IBLProcessor", "Generating BRDF LUT ({}x{})...", size, size);
    
    // Create RHI Texture directly
    auto rhiTexture = m_device->CreateTexture2D(size, size, RHIFormat::R16G16_FLOAT, RHITextureUsage::Sampled | RHITextureUsage::ColorAttachment);
    auto brdfLut = std::make_shared<Texture>(m_device, rhiTexture);
    
    auto vertCode = LoadShaderCode("Assets/Shaders/Bin/IBL/BRDFLUT.slang.vert.spv");
    auto fragCode = LoadShaderCode("Assets/Shaders/Bin/IBL/BRDFLUT.slang.frag.spv");

    if (vertCode.empty() || fragCode.empty()) {
        Logger::Error("IBLProcessor", "Failed to load shaders for BRDF LUT!");
        return nullptr;
    }

    auto vertShader = m_device->CreateShader(RHIShaderStage::Vertex, vertCode);
    auto fragShader = m_device->CreateShader(RHIShaderStage::Fragment, fragCode);

    RHIPipelineStateDescriptor pipelineDesc{};
    pipelineDesc.vertexShader = vertShader.get();
    pipelineDesc.fragmentShader = fragShader.get();
    pipelineDesc.colorFormats = { RHIFormat::R16G16_FLOAT };
    pipelineDesc.depthTestEnabled = false;

    auto pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);

    auto cmdList = m_device->CreateCommandList();
    cmdList->Begin();

    RHIRenderingAttachment colorAttachment{};
    colorAttachment.texture = brdfLut->GetRHITexture();
    colorAttachment.clear = true;

    RHIRect2D renderArea{};
    renderArea.extent = { size, size };

    cmdList->BeginRendering({ colorAttachment }, nullptr, renderArea);
    cmdList->BindPipeline(pipeline.get());
    
    cmdList->BindVertexBuffer(0, m_quadMesh->GetVertexBuffer(), 0);
    cmdList->BindIndexBuffer(m_quadMesh->GetIndexBuffer(), 0, false);
    cmdList->DrawIndexed(m_quadMesh->GetIndexCount(), 1, 0, 0, 0);
    
    cmdList->EndRendering();

    // Final transition for BRDF LUT
    cmdList->TransitionImageLayout(brdfLut->GetRHITexture(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    cmdList->End();
    m_device->SubmitCommandList(cmdList.get());
    m_device->WaitIdle();

    Logger::Info("IBLProcessor", "BRDF LUT generation completed.");
    return brdfLut;
}

std::vector<uint8_t> IBLProcessor::LoadShaderCode(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("IBLProcessor", "Failed to open shader file: {}", path);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint8_t> buffer(fileSize);
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();
    return buffer;
}

} // namespace AstralEngine
