#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace AstralEngine {

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
