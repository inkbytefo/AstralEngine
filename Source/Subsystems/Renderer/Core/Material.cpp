#include "Material.h"
#include <fstream>
#include <iostream>

namespace AstralEngine {

std::shared_ptr<Texture> Material::s_defaultWhiteTexture = nullptr;
std::shared_ptr<Texture> Material::s_defaultBlackTexture = nullptr;
std::shared_ptr<Texture> Material::s_defaultNormalTexture = nullptr;

Material::Material(IRHIDevice *device, const MaterialData &data,
                   IRHIDescriptorSetLayout *globalLayout)
    : m_device(device), m_data(data) {

  if (!s_defaultWhiteTexture) {
    s_defaultWhiteTexture = Texture::CreateFlatTexture(m_device, 1, 1, glm::vec4(1.0f));
    s_defaultBlackTexture = Texture::CreateFlatTexture(m_device, 1, 1, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    s_defaultNormalTexture = Texture::CreateFlatTexture(m_device, 1, 1, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
  }

  CreateUniformBuffer();
  CreatePipeline(data.vertexShaderPath, data.fragmentShaderPath, globalLayout);
  CreateDescriptorSet();
}

Material::~Material() {}

void Material::SetAlbedoMap(std::shared_ptr<Texture> texture) {
  m_albedoMap = texture;
}
void Material::SetNormalMap(std::shared_ptr<Texture> texture) {
  m_normalMap = texture;
}
void Material::SetMetallicMap(std::shared_ptr<Texture> texture) {
  m_metallicMap = texture;
}
void Material::SetRoughnessMap(std::shared_ptr<Texture> texture) {
  m_roughnessMap = texture;
}
void Material::SetAOMap(std::shared_ptr<Texture> texture) { m_aoMap = texture; }
void Material::SetEmissiveMap(std::shared_ptr<Texture> texture) {
  m_emissiveMap = texture;
}

void Material::SetBaseColor(const glm::vec4 &color) {
  m_data.properties.baseColor = glm::vec3(color);
  m_data.properties.opacity = color.a;
  UpdateDescriptorSet();
}

void Material::SetMetallic(float value) {
  m_data.properties.metallic = value;
  UpdateDescriptorSet();
}

void Material::SetRoughness(float value) {
  m_data.properties.roughness = value;
  UpdateDescriptorSet();
}

void Material::SetAO(float value) {
  m_data.properties.ao = value;
  UpdateDescriptorSet();
}

void Material::SetEmissiveColor(const glm::vec4 &color) {
  m_data.properties.emissiveColor = glm::vec3(color);
  UpdateDescriptorSet();
}

void Material::SetEmissiveIntensity(float value) {
  m_data.properties.emissiveIntensity = value;
  UpdateDescriptorSet();
}

std::vector<uint8_t> Material::ReadShaderFile(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Material failed to open shader file: " +
                             filepath);
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<uint8_t> buffer(fileSize);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
  file.close();

  return buffer;
}

void Material::CreateUniformBuffer() {
  // Create buffer for MaterialUniforms
  m_uniformBuffer = m_device->CreateBuffer(
      sizeof(MaterialUniforms), RHIBufferUsage::Uniform,
      RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);

  // Initialize with default data
  MaterialUniforms uniforms{};
  uniforms.baseColor =
      glm::vec4(m_data.properties.baseColor, m_data.properties.opacity);
  uniforms.metallic = m_data.properties.metallic;
  uniforms.roughness = m_data.properties.roughness;
  uniforms.ao = m_data.properties.ao;
  uniforms.emissiveIntensity = m_data.properties.emissiveIntensity;
  uniforms.emissiveColor = glm::vec4(m_data.properties.emissiveColor, 1.0f);

  uniforms.useNormalMap = 0;
  uniforms.useMetallicMap = 0;
  uniforms.useRoughnessMap = 0;
  uniforms.useAOMap = 0;
  uniforms.useEmissiveMap = 0;

  void *data = m_uniformBuffer->Map();
  memcpy(data, &uniforms, sizeof(MaterialUniforms));
  m_uniformBuffer->Unmap();
}

void Material::CreateDescriptorSet() {
  if (!m_descriptorSetLayout)
    return;

  m_descriptorSet =
      m_device->AllocateDescriptorSet(m_descriptorSetLayout.get());
  UpdateDescriptorSet();
}

void Material::UpdateDescriptorSet() {
  if (!m_descriptorSet)
    return;

  // Update Uniforms state based on texture presence
  MaterialUniforms uniforms{};
  uniforms.baseColor =
      glm::vec4(m_data.properties.baseColor, m_data.properties.opacity);
  uniforms.metallic = m_data.properties.metallic;
  uniforms.roughness = m_data.properties.roughness;
  uniforms.ao = m_data.properties.ao;
  uniforms.emissiveIntensity = m_data.properties.emissiveIntensity;
  uniforms.emissiveColor = glm::vec4(m_data.properties.emissiveColor, 1.0f);

  uniforms.useNormalMap = m_normalMap ? 1 : 0;
  uniforms.useMetallicMap = m_metallicMap ? 1 : 0;
  uniforms.useRoughnessMap = m_roughnessMap ? 1 : 0;
  uniforms.useAOMap = m_aoMap ? 1 : 0;
  uniforms.useEmissiveMap = m_emissiveMap ? 1 : 0;

  // Upload uniforms
  void *data = m_uniformBuffer->Map();
  memcpy(data, &uniforms, sizeof(MaterialUniforms));
  m_uniformBuffer->Unmap();

  // Binding 0: Material Uniforms
  if (m_uniformBuffer) {
    m_descriptorSet->UpdateUniformBuffer(0, m_uniformBuffer.get(), 0,
                                         sizeof(MaterialUniforms));
  }

  // Binding 1: Albedo Map
  auto albedo = m_albedoMap ? m_albedoMap : s_defaultWhiteTexture;
  m_descriptorSet->UpdateCombinedImageSampler(1, albedo->GetRHITexture(),
                                              albedo->GetRHISampler());

  // Binding 2: Normal Map
  auto normal = m_normalMap ? m_normalMap : s_defaultNormalTexture;
  m_descriptorSet->UpdateCombinedImageSampler(2, normal->GetRHITexture(),
                                              normal->GetRHISampler());

  // Binding 3: Metallic Map
  auto metallic = m_metallicMap ? m_metallicMap : s_defaultBlackTexture;
  m_descriptorSet->UpdateCombinedImageSampler(
      3, metallic->GetRHITexture(), metallic->GetRHISampler());

  // Binding 4: Roughness Map
  auto roughness = m_roughnessMap ? m_roughnessMap : s_defaultWhiteTexture;
  m_descriptorSet->UpdateCombinedImageSampler(
      4, roughness->GetRHITexture(), roughness->GetRHISampler());

  // Binding 5: AO Map
  auto ao = m_aoMap ? m_aoMap : s_defaultWhiteTexture;
  m_descriptorSet->UpdateCombinedImageSampler(5, ao->GetRHITexture(),
                                              ao->GetRHISampler());

  // Binding 6: Emissive Map
  auto emissive = m_emissiveMap ? m_emissiveMap : s_defaultBlackTexture;
  m_descriptorSet->UpdateCombinedImageSampler(
      6, emissive->GetRHITexture(), emissive->GetRHISampler());
}

void Material::CreatePipeline(const std::string &vertPath,
                              const std::string &fragPath,
                              IRHIDescriptorSetLayout *globalLayout) {
  // 1. Create Descriptor Set Layout (Set 1: Material)
  std::vector<RHIDescriptorSetLayoutBinding> bindings;

  // Binding 0: Material UBO (Fragment Stage)
  RHIDescriptorSetLayoutBinding uboBinding{};
  uboBinding.binding = 0;
  uboBinding.descriptorType = RHIDescriptorType::UniformBuffer;
  uboBinding.descriptorCount = 1;
  uboBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(uboBinding);

  // Binding 1: Albedo Sampler
  RHIDescriptorSetLayoutBinding albedoBinding{};
  albedoBinding.binding = 1;
  albedoBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  albedoBinding.descriptorCount = 1;
  albedoBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(albedoBinding);

  // Binding 2: Normal Sampler
  RHIDescriptorSetLayoutBinding normalBinding{};
  normalBinding.binding = 2;
  normalBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  normalBinding.descriptorCount = 1;
  normalBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(normalBinding);

  // Binding 3: Metallic Sampler
  RHIDescriptorSetLayoutBinding metallicBinding{};
  metallicBinding.binding = 3;
  metallicBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  metallicBinding.descriptorCount = 1;
  metallicBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(metallicBinding);

  // Binding 4: Roughness Sampler
  RHIDescriptorSetLayoutBinding roughnessBinding{};
  roughnessBinding.binding = 4;
  roughnessBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  roughnessBinding.descriptorCount = 1;
  roughnessBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(roughnessBinding);

  // Binding 5: AO Sampler
  RHIDescriptorSetLayoutBinding aoBinding{};
  aoBinding.binding = 5;
  aoBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  aoBinding.descriptorCount = 1;
  aoBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(aoBinding);

  // Binding 6: Emissive Sampler
  RHIDescriptorSetLayoutBinding emissiveBinding{};
  emissiveBinding.binding = 6;
  emissiveBinding.descriptorType = RHIDescriptorType::CombinedImageSampler;
  emissiveBinding.descriptorCount = 1;
  emissiveBinding.stageFlags = RHIShaderStage::Fragment;
  bindings.push_back(emissiveBinding);

  m_descriptorSetLayout = m_device->CreateDescriptorSetLayout(bindings);

  // 2. Load Shaders
  auto vertCode = ReadShaderFile(vertPath);
  auto fragCode = ReadShaderFile(fragPath);

  auto vertexShader = m_device->CreateShader(
      RHIShaderStage::Vertex,
      std::span<const uint8_t>(vertCode.data(), vertCode.size()));
  auto fragmentShader = m_device->CreateShader(
      RHIShaderStage::Fragment,
      std::span<const uint8_t>(fragCode.data(), fragCode.size()));

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
  posAttr.location = 0;
  posAttr.binding = 0;
  posAttr.format = RHIFormat::R32G32B32_FLOAT;
  posAttr.offset = 0;
  pipelineDesc.vertexAttributes.push_back(posAttr);

  // Normal
  RHIVertexInputAttribute normAttr{};
  normAttr.location = 1;
  normAttr.binding = 0;
  normAttr.format = RHIFormat::R32G32B32_FLOAT;
  normAttr.offset = 12;
  pipelineDesc.vertexAttributes.push_back(normAttr);

  // TexCoord
  RHIVertexInputAttribute uvAttr{};
  uvAttr.location = 2;
  uvAttr.binding = 0;
  uvAttr.format = RHIFormat::R32G32_FLOAT;
  uvAttr.offset = 24;
  pipelineDesc.vertexAttributes.push_back(uvAttr);

  // Tangent
  RHIVertexInputAttribute tanAttr{};
  tanAttr.location = 3;
  tanAttr.binding = 0;
  tanAttr.format = RHIFormat::R32G32B32_FLOAT;
  tanAttr.offset = 32;
  pipelineDesc.vertexAttributes.push_back(tanAttr);

  // Bitangent
  RHIVertexInputAttribute bitanAttr{};
  bitanAttr.location = 4;
  bitanAttr.binding = 0;
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

  // Dynamic Rendering Formats
  pipelineDesc.colorFormats = {RHIFormat::B8G8R8A8_SRGB};
  pipelineDesc.depthFormat = RHIFormat::D32_FLOAT;

  // Push Constants for model matrix
  RHIPushConstantRange pushConstant{};
  pushConstant.stageFlags = RHIShaderStage::Vertex;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(glm::mat4);
  pipelineDesc.pushConstants.push_back(pushConstant);

  m_pipeline = m_device->CreateGraphicsPipeline(pipelineDesc);
}

} // namespace AstralEngine
