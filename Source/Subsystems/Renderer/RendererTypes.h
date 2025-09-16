#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace AstralEngine {

/**
 * @struct UniformBufferObject
 * @brief Shader'daki uniform buffer yapısı - Model-View-Projection matrislerini tutar
 * 
 * Bu yapı, her frame güncellenen dönüşüm matrislerini CPU'dan GPU'ya taşımak için kullanılır.
 * Shader'daki layout(binding = 0) uniform UniformBufferObject ile eşleşir.
 */
struct UniformBufferObject {
    glm::mat4 model; ///< Model matrisi - nesnenin kendi dönüşümü
    glm::mat4 view;  ///< View matrisi - kamera dönüşümü
    glm::mat4 proj;  ///< Projection matrisi - perspektif projeksiyon
};

/**
 * @struct Vertex
 * @brief Vertex veri yapısı - shader'daki input değişkenleriyle eşleşir
 * 
 * Bu yapı, üçgenin her bir vertex'i için konum ve renk bilgisi tutar.
 * Shader'daki layout(location = 0) in vec2 inPosition ve
 * layout(location = 1) in vec3 inColor ile eşleşir.
 */
struct Vertex {
    glm::vec2 pos;     ///< 2D konum (x, y)
    glm::vec3 color;   ///< 3D renk (r, g, b)

    /**
     * @brief Vertex binding description'ını döndürür
     * 
     * Bu fonksiyon, vertex verilerinin bellekte nasıl düzenlendiğini
     * ve GPU'ya nasıl yükleneceğini tanımlar.
     * 
     * @return VkVertexInputBindingDescription vertex binding description
     */
    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;                                    // Binding index
        bindingDescription.stride = sizeof(Vertex);                       // Her vertex'in boyutu
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;      // Vertex başına veri
        
        return bindingDescription;
    }

    /**
     * @brief Vertex attribute description'larını döndürür
     * 
     * Bu fonksiyon, vertex struct'ındaki her bir üyenin (pos, color)
     * shader'daki location'larla nasıl eşleşeceğini tanımlar.
     * 
     * @return std::array<VkVertexInputAttributeDescription, 2> attribute descriptions
     */
    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position attribute (location = 0)
        attributeDescriptions[0].binding = 0;                           // Aynı binding index
        attributeDescriptions[0].location = 0;                          // Shader'daki location
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;      // 2 float (vec2)
        attributeDescriptions[0].offset = offsetof(Vertex, pos);        // Struct içindeki offset

        // Color attribute (location = 1)
        attributeDescriptions[1].binding = 0;                           // Aynı binding index
        attributeDescriptions[1].location = 1;                          // Shader'daki location
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;   // 3 float (vec3)
        attributeDescriptions[1].offset = offsetof(Vertex, color);      // Struct içindeki offset

        return attributeDescriptions;
    }
};

/**
 * @enum RendererAPI
 * @brief Desteklenen rendering API'leri
 */
enum class RendererAPI {
    Vulkan = 0,
    DirectX11,
    DirectX12,
    OpenGL,
    Metal
};

/**
 * @enum RenderCommandType
 * @brief Render komut tipleri
 */
enum class RenderCommandType {
    Draw,
    DrawIndexed,
    SetViewport,
    SetScissor,
    BindPipeline,
    BindVertexBuffer,
    BindIndexBuffer,
    BindDescriptorSets,
    PushConstants,
    CopyBuffer,
    CopyImage,
    BlitImage,
    Barrier
};

/**
 * @struct RenderCommand
 * @brief Render komut yapısı
 */
struct RenderCommand {
    RenderCommandType type;
    
    union {
        struct {
            uint32_t vertexCount;
            uint32_t instanceCount;
            uint32_t firstVertex;
            uint32_t firstInstance;
        } draw;
        
        struct {
            uint32_t indexCount;
            uint32_t instanceCount;
            uint32_t firstIndex;
            int32_t vertexOffset;
            uint32_t firstInstance;
        } drawIndexed;
        
        struct {
            float x, y, width, height;
            float minDepth, maxDepth;
        } setViewport;
        
        struct {
            int32_t x, y;
            uint32_t width, height;
        } setScissor;
        
        struct {
            void* pipeline;
        } bindPipeline;
        
        struct {
            void* buffer;
            uint64_t offset;
        } bindVertexBuffer;
        
        struct {
            void* buffer;
            uint64_t offset;
        } bindIndexBuffer;
        
        struct {
            void* pipelineLayout;
            uint32_t firstSet;
            uint32_t descriptorSetCount;
            void** descriptorSets;
            uint32_t dynamicOffsetCount;
            const uint32_t* dynamicOffsets;
        } bindDescriptorSets;
        
        struct {
            void* pipelineLayout;
            uint32_t offset;
            uint32_t size;
            const void* data;
        } pushConstants;
    };
};

} // namespace AstralEngine
