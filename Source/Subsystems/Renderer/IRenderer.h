#pragma once

#include "Core/ISubsystem.h"
#include <memory>
#include <vector>

namespace AstralEngine {

/**
 * @enum RendererAPI
 * @brief Desteklenen render API'leri
 */
enum class RendererAPI {
    Vulkan,
    DirectX12,
    Metal,
    OpenGL
};

/**
 * @struct RenderCommand
 * @brief Render komutları için temel yapı
 */
struct RenderCommand {
    enum class Type {
        DrawMesh,
        SetViewport,
        SetScissor,
        BindPipeline,
        ClearColor
    };

    Type type;
    std::vector<uint8_t> data; // Komuta özel veriler
};

/**
 * @class IRenderer
 * @brief Tüm renderer implementasyonları için arayüz
 * 
 * Bu arayüz, farklı render API'leri (Vulkan, DirectX12, Metal vb.)
 * için ortak bir arayüz sağlar. Motorun geri kalanı bu arayüz üzerinden
 * renderer ile iletişim kurar. ISubsystem'den türetilerek tutarlı
 * yaşam döngüsü yönetimi sağlanır.
 */
class IRenderer : public ISubsystem {
public:
    virtual ~IRenderer() = default;

    // Temel yaşam döngüsü
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // Frame yönetimi
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    
    // Komut gönderimi
    virtual void Submit(const RenderCommand& command) = 0;
    virtual void SubmitCommands(const std::vector<RenderCommand>& commands) = 0;
    
    // Durum sorgulama
    virtual bool IsInitialized() const = 0;
    virtual RendererAPI GetAPI() const = 0;
    
    // Yapılandırma
    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
};

} // namespace AstralEngine
