#pragma once

#include <astral/gpu/device.hpp>

#include <cstdint>
#include <memory>

namespace astral::gpu {

struct SwapchainDesc {
    bool vsync = true;
    /// Preferred color format; falls back to whatever the surface offers.
    Format format = Format::BGRA8Srgb;
    std::uint32_t frames_in_flight = 2;
};

/// Presents rendered images to the window the Device was created with.
///
///     TextureHandle image = swapchain.acquire();
///     if (!image.valid()) continue;           // minimized / being resized
///     auto cmd = device.begin_commands();
///     cmd.transition(image, ResourceState::ColorTarget);
///     ... render ...
///     cmd.transition(image, ResourceState::Present);
///     swapchain.present(cmd);                 // submits (waiting for the image) and presents
///
/// Out-of-date or resized surfaces are recreated automatically. The Swapchain must be
/// destroyed before its Device.
class Swapchain {
public:
    /// Requires a Device created with DeviceDesc::surface. Returns nullptr on failure.
    static std::unique_ptr<Swapchain> create(Device& device, const SurfaceSource& surface,
                                             const SwapchainDesc& desc = {});
    ~Swapchain();
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    /// Next image to render into, or an invalid handle when nothing can be drawn this frame.
    /// Also throttles the CPU so at most `frames_in_flight` frames are queued. Calling it again
    /// before present() returns the same image.
    TextureHandle acquire();
    /// Submits `commands` (which must leave the image in Present state) so they wait for the
    /// acquired image, then presents it. Without an acquired image it is a plain submit.
    SubmitTicket present(CommandList& commands);
    /// Forces recreation at the next acquire (e.g. after a window resize event).
    void invalidate() noexcept;

    [[nodiscard]] Format format() const noexcept;
    [[nodiscard]] Extent2D extent() const noexcept;

    struct Impl;

private:
    explicit Swapchain(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral::gpu
