#pragma once
#include "../Interfaces/IDeveloperTool.h"
#include "../Common/DevToolsTypes.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>

namespace AstralEngine {

class Engine;
class RenderSubsystem;
class ECSSubsystem;
class VulkanRenderer;

class DebugRenderer : public IDeveloperTool {
public:
    struct DebugDrawCall {
        enum class Type { Line, Box, Sphere, Text, Frustum };
        
        Type type;
        glm::vec3 start;
        glm::vec3 end;
        glm::vec3 center;
        glm::vec3 size;
        float radius;
        glm::vec4 color;
        std::string text;
        float duration;
        bool depthTest;
        std::chrono::system_clock::time_point creationTime;
        
        DebugDrawCall()
            : type(Type::Line), start(0.0f), end(0.0f), center(0.0f), size(1.0f),
              radius(1.0f), color(1.0f, 1.0f, 1.0f, 1.0f), duration(0.0f), depthTest(true),
              creationTime(std::chrono::system_clock::now()) {}
    };
    
    DebugRenderer();
    ~DebugRenderer() = default;
    
    // IDeveloperTool implementasyonu
    void OnInitialize() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
    
    const std::string& GetName() const override { return m_name; }
    bool IsEnabled() const override { return m_enabled; }
    void SetEnabled(bool enabled) override { m_enabled = enabled; }
    
    void LoadSettings(const std::string& settings) override;
    std::string SaveSettings() const override;
    
    // Debug çizim komutları
    void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float duration = 0.0f, bool depthTest = true);
    void DrawBox(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, float duration = 0.0f, bool depthTest = true);
    void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color, float duration = 0.0f, bool depthTest = true);
    void DrawText(const glm::vec3& position, const std::string& text, const glm::vec4& color, float duration = 0.0f);
    void DrawFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration = 0.0f, bool depthTest = true);
    
    // Collision görselleştirme
    void DrawBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color, float duration = 0.0f);
    void DrawCapsule(const glm::vec3& center, float radius, float height, const glm::vec4& color, float duration = 0.0f);
    void DrawCylinder(const glm::vec3& center, float radius, float height, const glm::vec4& color, float duration = 0.0f);
    
    // Işık ve kamera görselleştirme
    void DrawLightFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration = 0.0f);
    void DrawCameraFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration = 0.0f);
    void DrawLightPosition(const glm::vec3& position, const glm::vec4& color, float radius = 0.1f, float duration = 0.0f);
    
    // Navigation ve AI görselleştirme
    void DrawNavMesh(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, const glm::vec4& color, float duration = 0.0f);
    void DrawPath(const std::vector<glm::vec3>& points, const glm::vec4& color, float duration = 0.0f);
    void DrawRay(const glm::vec3& start, const glm::vec3& direction, float length, const glm::vec4& color, float duration = 0.0f);
    
    // Veri bağlama
    void BindToRenderSubsystem(RenderSubsystem* renderSubsystem);
    void BindToECS(ECSSubsystem* ecs);
    
    // Vulkan render pass erişimi
    VkRenderPass GetDebugRenderPass() const { return m_debugRenderPass; }
    
private:
    // Render metodları
    void RenderDebugDraws(VkCommandBuffer commandBuffer);
    void UpdateDebugDraws(float deltaTime);
    void CreateDebugPipeline();
    void CreateDebugBuffers();
    void UpdateDebugVertexBuffer();
    
    // Yardımcı metodlar
    void GenerateBoxVertices(const glm::vec3& center, const glm::vec3& size, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
    void GenerateSphereVertices(const glm::vec3& center, float radius, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
    void GenerateFrustumVertices(const glm::mat4& viewProj, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
    void GenerateTextVertices(const glm::vec3& position, const std::string& text, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
    
    // Vulkan objeleri
    void CreateVulkanObjects();
    void DestroyVulkanObjects();
    void CreateDebugRenderPass();
    void CreateDebugPipelineLayout();
    void CreateDebugShaderModules();
    
    std::string m_name = "DebugRenderer";
    bool m_enabled = true;
    
    // Debug çizim verileri
    std::vector<DebugDrawCall> m_drawCalls;
    std::vector<glm::vec3> m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    std::vector<glm::vec4> m_colorBuffer;
    
    // Vulkan objeleri
    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_debugRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_debugPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_debugPipeline = VK_NULL_HANDLE;
    VkShaderModule m_vertexShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragmentShaderModule = VK_NULL_HANDLE;
    
    // Buffer'lar
    VkBuffer m_vertexBufferGPU = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBufferGPU = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_colorBufferGPU = VK_NULL_HANDLE;
    VkDeviceMemory m_colorBufferMemory = VK_NULL_HANDLE;
    
    // Bağlantılar
    RenderSubsystem* m_renderSubsystem = nullptr;
    ECSSubsystem* m_ecs = nullptr;
    VulkanRenderer* m_vulkanRenderer = nullptr;
    
    // Ayarlar
    bool m_depthTestEnabled = true;
    bool m_wireframeMode = false;
    float m_lineWidth = 1.0f;
    glm::vec4 m_defaultColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Performans
    size_t m_maxDrawCalls = 10000;
    size_t m_maxVertices = 100000;
    size_t m_maxIndices = 200000;
};

} // namespace AstralEngine