#pragma once
// Optional offscreen 3D scene viewport service on Context: lets an editor or tool draw the 3D world
// into a TextureHandle sampled by UI panels while the swapchain renders UI layers only.

#include <astral/gpu/types.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>

namespace astral {

class World;

class ViewportService {
public:
    virtual ~ViewportService() = default;

    /// Requests rendering the 3D scene into an offscreen target of `width x height` pixels.
    /// Pass 0x0 to disable offscreen viewport mode and render directly to the swapchain.
    virtual void set_size(std::uint32_t width, std::uint32_t height) = 0;

    /// Handle of the offscreen 3D viewport texture (pack with `pack_ui_texture` for ImGui::Image),
    /// or invalid when disabled or before the first render.
    [[nodiscard]] virtual TextureHandle texture() const noexcept = 0;
    [[nodiscard]] virtual gpu::Extent2D size() const noexcept = 0;

    /// Optional alternate ECS World to render into the viewport (e.g. isolated Play or AI preview world).
    /// Pass nullptr to render the main Context::world.
    virtual void set_render_world(World* world) noexcept = 0;
    [[nodiscard]] virtual World* render_world() const noexcept = 0;

    /// Optional explicit camera overriding the active Camera entity in the rendered world.
    virtual void set_camera_override(const CameraData* camera) noexcept = 0;

    /// Background clear color for the swapchain behind UI layers when viewport mode is active.
    virtual void set_clear_color(Vec4 rgba) noexcept = 0;
};

} // namespace astral
