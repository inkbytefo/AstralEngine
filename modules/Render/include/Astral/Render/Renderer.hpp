#pragma once

#include "Astral/Render/RenderTypes.hpp"
#include "Astral/Render/World.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Astral::Render {

// Region of space baked into the sparse brick volume used to render SDF shapes.
struct SdfVolumeSettings {
    glm::vec3 origin{-44.8f, -8.0f, -44.8f};
    float voxelSize = 0.08f;               // a brick cell spans 7 voxels
    glm::uvec3 cells{160, 96, 160};        // 89.6 x 53.8 x 89.6 m by default
    glm::uvec3 atlasBricks{32, 32, 48};    // 49,152 bricks, 48 MB
};

struct RendererDesc {
    std::string applicationName = "Astral";
    bool enableValidation = false;               // also enabled by ASTRAL_VALIDATION=1
    std::vector<std::string> instanceExtensions; // e.g. what the window system needs for surfaces
    SdfVolumeSettings sdfVolume;
};

// The Astral renderer. Owns the GPU device, resources (meshes, textures, materials),
// the World and any number of views (render targets with their own history).
//
// Typical frame:
//     if (renderer.BeginFrame()) {
//         renderer.RenderView(view, camera);
//         renderer.PresentView(view);   // only when a surface is attached
//         renderer.EndFrame();
//     }
//
// Vulkan-level access (surfaces, overlay callbacks, native image views) lives in
// Astral/Render/VulkanInterop.hpp so that this header stays API-neutral.
class Renderer {
public:
    explicit Renderer(const RendererDesc& desc = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // --- Resources ---------------------------------------------------------
    MeshHandle CreateMesh(const MeshDesc& desc);
    void DestroyMesh(MeshHandle mesh);

    TextureHandle CreateTexture(const TextureDesc& desc);
    void DestroyTexture(TextureHandle texture);

    MaterialHandle CreateMaterial(const MaterialDesc& desc);
    void UpdateMaterial(MaterialHandle material, const MaterialDesc& desc);
    void DestroyMaterial(MaterialHandle material);
    MaterialHandle GetDefaultMaterial() const;

    World& GetWorld();
    const World& GetWorld() const;

    // --- Views ---------------------------------------------------------------
    ViewHandle CreateView(uint32_t width, uint32_t height);
    void ResizeView(ViewHandle view, uint32_t width, uint32_t height);
    void DestroyView(ViewHandle view);
    glm::uvec2 GetViewSize(ViewHandle view) const;

    // --- Frame -----------------------------------------------------------------
    // Waits for a free frame slot and, if a surface is attached, acquires the next
    // swapchain image. Returns false when the frame should be skipped (e.g. minimised).
    bool BeginFrame();
    void RenderView(ViewHandle view, const Camera& camera, const ViewSettings& settings = {});
    void PresentView(ViewHandle view);            // scales the view onto the surface
    void EndFrame();                              // submits (and presents)
    void ResizeSurface(uint32_t width, uint32_t height);
    bool HasSurface() const;

    // --- Readback ------------------------------------------------------------
    // Picking is asynchronous: the result arrives a few frames later.
    void RequestPick(ViewHandle view, uint32_t x, uint32_t y);
    std::optional<PickResult> TakePickResult();
    // Blocking copy of a view's final image (RGBA8). For tests and screenshots.
    std::vector<uint8_t> ReadViewPixels(ViewHandle view);

    RenderStats GetStats() const;
    void WaitIdle();

    struct Impl;
    Impl& GetImpl() { return *m_Impl; }

private:
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Astral::Render
