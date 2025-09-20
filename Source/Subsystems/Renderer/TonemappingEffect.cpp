#include "TonemappingEffect.h"
#include "VulkanRenderer.h"
#include "Core/VulkanDevice.h"
#include "Buffers/VulkanTexture.h"
#include "../Asset/AssetData.h"
#include <fstream>
#include <filesystem>

namespace AstralEngine {

TonemappingEffect::TonemappingEffect() {
    // Varsayılan tonemapping parametrelerini ayarla
    m_uboData.exposure = 1.0f;
    m_uboData.gamma = 2.2f;
    m_uboData.tonemapper = 1; // ACES
    m_uboData.contrast = 1.0f;
    m_uboData.brightness = 0.0f;
    m_uboData.saturation = 1.0f;
    m_uboData.vignetteIntensity = 0.0f;
    m_uboData.vignetteRadius = 0.5f;
    m_uboData.chromaticAberrationIntensity = 0.0f;
    m_uboData.bloomIntensity = 0.0f;
    m_uboData.lensDirtIntensity = 0.0f;
    m_uboData.useColorGrading = 0;
    m_uboData.useDithering = 0;
    m_uboData.padding = glm::vec2(0.0f, 0.0f);
    
    // Varsayılan push constants
    m_pushConstants.texelSize = glm::vec2(1.0f / 1920.0f, 1.0f / 1080.0f); // Varsayılan çözünürlük
    m_pushConstants.useBloom = 0;
    m_pushConstants.useVignette = 0;
    m_pushConstants.useChromaticAberration = 0;
    
    // Temel sınıf konfigürasyonunu ayarla
    SetName("TonemappingEffect");
}

TonemappingEffect::~TonemappingEffect() {
    Shutdown();
}

bool TonemappingEffect::Initialize(VulkanRenderer* renderer) {
    // Temel sınıfın Initialize metodunu çağır
    return PostProcessingEffectBase::Initialize(renderer);
}

void TonemappingEffect::Shutdown() {
    // Temel sınıfın Shutdown metodunu çağır
    PostProcessingEffectBase::Shutdown();
}

void TonemappingEffect::RecordCommands(VkCommandBuffer commandBuffer,
                                      VulkanTexture* inputTexture,
                                      VulkanFramebuffer* outputFramebuffer,
                                      uint32_t frameIndex) {
    if (!IsInitialized() || !inputTexture || !outputFramebuffer) {
        Logger::Error("TonemappingEffect", "RecordCommands çağrısı için efekt başlatılmamış veya geçersiz parametreler");
        return;
    }

    // Descriptor set'leri güncelle
    UpdateDescriptorSets(inputTexture, frameIndex);

    // Uniform buffer'ı güncelle
    void* data;
    vkMapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetBufferMemory(), 0, sizeof(TonemappingUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(TonemappingUBO));
    vkUnmapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetBufferMemory());

    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = outputFramebuffer->GetRenderPass();
    renderPassInfo.framebuffer = outputFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {outputFramebuffer->GetWidth(), outputFramebuffer->GetHeight()};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    DrawFullScreenQuad(commandBuffer, frameIndex);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

const std::string& TonemappingEffect::GetName() const {
    return m_name;
}

bool TonemappingEffect::IsEnabled() const {
    return m_isEnabled;
}

void TonemappingEffect::SetEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool TonemappingEffect::OnInitialize() {
    Logger::Info("TonemappingEffect", "Tonemapping efektinin özel başlatma işlemleri başlatılıyor...");

    // Shader'ları yükle - TonemappingEffect'e özel shader yollarını kullan
    if (!CreateShaders("Assets/Shaders/PostProcessing/tonemap.vert.spv", "Assets/Shaders/PostProcessing/tonemap.frag.spv")) {
        return false;
    }

    // Tonemapping efektine özel descriptor set layout'ı oluştur
    if (!CreateDescriptorSetLayout()) {
        return false;
    }

    // Pipeline'ı oluştur
    if (!CreatePipeline()) {
        return false;
    }

    // Uniform buffer'ları oluştur (TonemappingUBO boyutunda)
    if (!CreateUniformBuffers(sizeof(TonemappingUBO))) {
        return false;
    }

    // Descriptor set'leri oluştur (Tonemapping için 1 sampler descriptor: input texture)
    if (!CreateDescriptorSets(1)) {
        return false;
    }

    Logger::Info("TonemappingEffect", "Tonemapping efektinin özel başlatma işlemleri tamamlandı");
    return true;
}

void TonemappingEffect::OnShutdown() {
    Logger::Info("TonemappingEffect", "Tonemapping efektinin özel kapatma işlemleri başlatılıyor...");
    
    // Tonemapping efektine özel temizlik işlemleri burada yapılır
    // Şu an için ekstra temizlik gereken kaynak yok
    
    Logger::Info("TonemappingEffect", "Tonemapping efektinin özel kapatma işlemleri tamamlandı");
}

void TonemappingEffect::OnRecordCommands(VkCommandBuffer commandBuffer,
                                       VulkanTexture* inputTexture,
                                       VulkanFramebuffer* outputFramebuffer,
                                       uint32_t frameIndex) {
    // Uniform buffer'ı güncelle
    void* data;
    vkMapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetBufferMemory(), 0, sizeof(TonemappingUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(TonemappingUBO));
    vkUnmapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetBufferMemory());

    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = outputFramebuffer->GetRenderPass();
    renderPassInfo.framebuffer = outputFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {outputFramebuffer->GetWidth(), outputFramebuffer->GetHeight()};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    DrawFullScreenQuad(commandBuffer, frameIndex);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

bool TonemappingEffect::CreateDescriptorSetLayout() {
    // Uniform buffer binding
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    // Sampler binding
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(GetDevice()->GetDevice(), &layoutInfo, nullptr, const_cast<VkDescriptorSetLayout*>(&GetDescriptorSetLayout())) != VK_SUCCESS) {
        SetError("Descriptor set layout oluşturulamadı");
        return false;
    }

    return true;
}

void TonemappingEffect::UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) {
    // Uniform buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = GetUniformBuffers()[frameIndex]->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(TonemappingUBO);

    // Image info
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = inputTexture->GetImageView();
    imageInfo.sampler = inputTexture->GetSampler();

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = GetDescriptorSets()[frameIndex];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = GetDescriptorSets()[frameIndex];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(GetDevice()->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                          descriptorWrites.data(), 0, nullptr);
}

} // namespace AstralEngine