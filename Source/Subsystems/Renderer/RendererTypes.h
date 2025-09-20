#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace AstralEngine {

// Forward declarations
class GraphicsDevice;
class Camera;
class VulkanTexture;
class VulkanBuffer;
class AssetHandle;

/**
 * @brief Renderer API türleri
 */
enum class RendererAPI {
    None = 0,
    Vulkan = 1,
    OpenGL = 2,
    DirectX = 3
};

/**
 * @brief Render komut türleri
 */
enum class RenderCommandType {
    Draw,
    DrawIndexed,
    DrawInstanced,
    DrawIndexedInstanced,
    Dispatch,
    Clear,
    SetViewport,
    SetScissor,
    BindPipeline,
    BindVertexBuffer,
    BindIndexBuffer,
    BindDescriptorSet,
    PushConstants,
    CopyBuffer,
    CopyTexture,
    Blit
};

/**
 * @brief Render komutu yapısı
 */
struct RenderCommand {
    RenderCommandType type;
    uint32_t vertexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstVertex = 0;
    uint32_t firstInstance = 0;
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    
    // Pipeline state
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    
    // Buffers
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    
    // Descriptors
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    uint32_t descriptorSetCount = 0;
    
    // Viewport and scissor
    VkViewport viewport{};
    VkRect2D scissor{};
    
    // Clear values
    std::array<VkClearValue, 4> clearValues{};
    uint32_t clearValueCount = 0;
    
    // Push constants
    std::vector<uint8_t> pushConstants;
    VkShaderStageFlags pushConstantStages = 0;
    
    // Copy operations
    VkBuffer srcBuffer = VK_NULL_HANDLE;
    VkBuffer dstBuffer = VK_NULL_HANDLE;
    VkImage srcImage = VK_NULL_HANDLE;
    VkImage dstImage = VK_NULL_HANDLE;
    VkImageBlit blitRegion{};
    VkBufferCopy bufferCopy{};
    VkImageCopy imageCopy{};
    
    // Constructor
    RenderCommand() = default;
    explicit RenderCommand(RenderCommandType cmdType) : type(cmdType) {}
};



/**
 * @brief GPU kaynaklarının yükleme durumlarını belirten enum
 */
enum class GpuResourceState {
    Unloaded,   ///< Henüz yüklenmeye başlanmadı
    Uploading,  ///< GPU'ya upload ediliyor
    Ready,      ///< GPU'da kullanıma hazır
    Failed      ///< Yükleme başarısız oldu
};

/**
 * @brief Render pipeline türleri
 */
enum class RenderPipelineType {
    Graphics,
    Compute,
    RayTracing
};

/**
 * @brief Vertex attribute türleri
 */
enum class VertexAttributeType {
    Position,
    Normal,
    TexCoord,
    Color,
    Tangent,
    Bitangent
};

/**
 * @brief Texture format bilgileri
 */
struct TextureFormatInfo {
    VkFormat vkFormat;
    uint32_t bytesPerPixel;
    bool isCompressed;
    bool hasAlpha;
};

/**
 * @brief Mesh material anahtarı için yapı
 */
struct MeshMaterialKey {
    uint32_t meshId;
    uint32_t materialId;
    
    bool operator<(const MeshMaterialKey& other) const {
        return (meshId < other.meshId) || 
               (meshId == other.meshId && materialId < other.materialId);
    }
    
    bool operator==(const MeshMaterialKey& other) const {
        return meshId == other.meshId && materialId == other.materialId;
    }
};

} // namespace AstralEngine

// std::hash specialization for MeshMaterialKey
namespace std {
    template<>
    struct hash<AstralEngine::MeshMaterialKey> {
        size_t operator()(const AstralEngine::MeshMaterialKey& key) const {
            return hash<uint32_t>()(key.meshId) ^ (hash<uint32_t>()(key.materialId) << 1);
        }
    };
}