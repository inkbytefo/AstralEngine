#pragma once

#include "Subsystems/Asset/AssetData.h"
#include "Subsystems/Renderer/RHI/IRHIDescriptor.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Texture.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace AstralEngine {

struct MaterialUniforms {
  glm::vec4 baseColor;
  float metallic;
  float roughness;
  float ao;
  float emissiveIntensity;
  glm::vec4 emissiveColor;
  int useNormalMap;
  int useMetallicMap;
  int useRoughnessMap;
  int useAOMap;
  int useEmissiveMap;
  float padding[3]; // Align to 16 bytes if necessary, though std140 usually
                    // handles member alignment.
};

class Material {
public:
  Material(IRHIDevice *device, const MaterialData &data,
           IRHIDescriptorSetLayout *globalLayout);
  ~Material();

  void SetAlbedoMap(std::shared_ptr<Texture> texture);
  void SetNormalMap(std::shared_ptr<Texture> texture);
  void SetMetallicMap(std::shared_ptr<Texture> texture);
  void SetRoughnessMap(std::shared_ptr<Texture> texture);
  void SetAOMap(std::shared_ptr<Texture> texture);
  void SetEmissiveMap(std::shared_ptr<Texture> texture);

  // Property Setters
  void SetBaseColor(const glm::vec4 &color);
  void SetMetallic(float value);
  void SetRoughness(float value);
  void SetAO(float value);
  void SetEmissiveColor(const glm::vec4 &color);
  void SetEmissiveIntensity(float value);

  // Property Getters
  glm::vec4 GetBaseColor() const {
    return glm::vec4(m_data.properties.baseColor, m_data.properties.opacity);
  }
  float GetMetallic() const { return m_data.properties.metallic; }
  float GetRoughness() const { return m_data.properties.roughness; }
  float GetAO() const { return m_data.properties.ao; }
  glm::vec4 GetEmissiveColor() const {
    return glm::vec4(m_data.properties.emissiveColor, 1.0f);
  }
  float GetEmissiveIntensity() const {
    return m_data.properties.emissiveIntensity;
  }

  void UpdateDescriptorSet();

  IRHIPipeline *GetPipeline() const { return m_pipeline.get(); }
  IRHIDescriptorSetLayout *GetDescriptorSetLayout() const {
    return m_descriptorSetLayout.get();
  }
  IRHIDescriptorSet *GetDescriptorSet() const { return m_descriptorSet.get(); }

  std::shared_ptr<Texture> GetAlbedoMap() const { return m_albedoMap; }

private:
  void CreatePipeline(const std::string &vertPath, const std::string &fragPath,
                      IRHIDescriptorSetLayout *globalLayout);
  std::vector<uint8_t> ReadShaderFile(const std::string &filepath);
  void CreateUniformBuffer();
  void CreateDescriptorSet();

  IRHIDevice *m_device;
  MaterialData m_data;

  std::shared_ptr<IRHIPipeline> m_pipeline;
  std::shared_ptr<IRHIDescriptorSetLayout> m_descriptorSetLayout;
  std::shared_ptr<IRHIDescriptorSet> m_descriptorSet;
  std::shared_ptr<IRHIBuffer> m_uniformBuffer;

  std::shared_ptr<Texture> m_albedoMap;
  std::shared_ptr<Texture> m_normalMap;
  std::shared_ptr<Texture> m_metallicMap;
  std::shared_ptr<Texture> m_roughnessMap;
  std::shared_ptr<Texture> m_aoMap;
  std::shared_ptr<Texture> m_emissiveMap;

  static std::shared_ptr<Texture> s_defaultWhiteTexture;
  static std::shared_ptr<Texture> s_defaultBlackTexture;
  static std::shared_ptr<Texture> s_defaultNormalTexture;
};

} // namespace AstralEngine
