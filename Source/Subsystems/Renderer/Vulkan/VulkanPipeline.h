#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AstralEngine {

class GraphicsDevice;

class VulkanPipeline {
public:
    VulkanPipeline(GraphicsDevice* device, const std::string& vertShaderPath, const std::string& fragShaderPath);
    ~VulkanPipeline();

    void Initialize(VkRenderPass renderPass);
    void Shutdown();

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }

private:
    static std::vector<char> ReadFile(const std::string& filepath);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);

    GraphicsDevice* m_device; // Non-owning
    std::string m_vertShaderPath;
    std::string m_fragShaderPath;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

}
