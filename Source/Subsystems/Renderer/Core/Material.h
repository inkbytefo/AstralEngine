#pragma once

#include "Subsystems/Renderer/RHI/IRHIPipeline.h"
#include "Subsystems/Renderer/RHI/IRHIDescriptor.h"
#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Asset/AssetData.h"
#include "Texture.h"
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace AstralEngine {

    struct MaterialUniforms {
        glm::vec4 baseColor;
        float metallic;
        float roughness;
        float ao;
        float padding;
    };

    class Material {
    public:
        Material(IRHIDevice* device, const MaterialData& data, IRHIDescriptorSetLayout* globalLayout);
        ~Material();

        void SetAlbedoMap(std::shared_ptr<Texture> texture);
        void UpdateDescriptorSet();
        
        IRHIPipeline* GetPipeline() const { return m_pipeline.get(); }
        IRHIDescriptorSetLayout* GetDescriptorSetLayout() const { return m_descriptorSetLayout.get(); }
        IRHIDescriptorSet* GetDescriptorSet() const { return m_descriptorSet.get(); }
        std::shared_ptr<Texture> GetAlbedoMap() const { return m_albedoMap; }

    private:
        void CreatePipeline(const std::string& vertPath, const std::string& fragPath, IRHIDescriptorSetLayout* globalLayout);
        std::vector<uint8_t> ReadShaderFile(const std::string& filepath);
        void CreateUniformBuffer();
        void CreateDescriptorSet();

        IRHIDevice* m_device;
        MaterialData m_data;

        std::shared_ptr<IRHIPipeline> m_pipeline;
        std::shared_ptr<IRHIDescriptorSetLayout> m_descriptorSetLayout;
        std::shared_ptr<IRHIDescriptorSet> m_descriptorSet;
        std::shared_ptr<IRHIBuffer> m_uniformBuffer;
        
        std::shared_ptr<Texture> m_albedoMap;
    };

}
