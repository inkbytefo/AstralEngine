#pragma once

#include "Astral/Render/Renderer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace GpuTest {

extern int g_VulkanErrors;

struct Rgb { int r, g, b; };

inline Rgb PixelAt(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t x, uint32_t y) {
    size_t i = (size_t(y) * width + x) * 4;
    return {rgba[i], rgba[i + 1], rgba[i + 2]};
}

// Renders `frames` frames of `view` and then picks pixel (x, y).
inline std::optional<Astral::Render::PickResult> RenderAndPick(Astral::Render::Renderer& r, Astral::Render::ViewHandle view,
                                                               const Astral::Render::Camera& camera, uint32_t x, uint32_t y,
                                                               const Astral::Render::ViewSettings& settings = {}) {
    r.RequestPick(view, x, y);
    for (int i = 0; i < 4; ++i) { // the result arrives after the frame's fence
        if (r.BeginFrame()) {
            r.RenderView(view, camera, settings);
            r.EndFrame();
        }
    }
    r.WaitIdle();
    if (r.BeginFrame()) r.EndFrame(); // collects completed readbacks
    std::optional<Astral::Render::PickResult> last;
    while (auto p = r.TakePickResult()) last = p;
    return last;
}

} // namespace GpuTest
