#pragma once

#include "Vk/Resources.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Astral::Render {

// How a pass uses a resource. The graph turns consecutive uses into the right
// layout transitions and memory/execution dependencies.
enum class Use {
    ColorAttachment,   // written as a render target
    DepthAttachment,   // depth test + write
    Sampled,           // read through a sampler (fragment or compute)
    StorageRead,       // imageLoad / SSBO read (compute)
    StorageWrite,      // imageStore / SSBO write (compute)
    StorageReadWrite,
    VertexIndexRead,   // buffers only
    IndirectRead,      // buffers only
    TransferSrc,
    TransferDst,
    HostRead,          // buffers only: CPU reads after the frame's fence
};

using BufferState = Vk::SyncState;

// Minimal frame graph. Passes are executed in the order they are added; the graph's
// job is synchronisation: each pass declares its resource uses and the graph emits
// the barriers in front of it. Images keep their state between frames, so the same
// mechanism works for history buffers and swapchain images.
class RenderGraph {
public:
    class PassBuilder {
    public:
        PassBuilder& Image(Vk::Image& image, Use use, bool discardContents = false);
        PassBuilder& RawImage(vk::Image image, Vk::ImageState& state, vk::ImageSubresourceRange range, Use use,
                              bool discardContents = false);
        PassBuilder& Buffer(BufferState& state, Use use);

    private:
        friend class RenderGraph;
        struct ImageUse { vk::Image image; Vk::ImageState* state; vk::ImageSubresourceRange range; Use use; bool discard; bool depth; };
        struct BufferUse { BufferState* state; Use use; };
        // Several uses of one resource in a pass are merged into a single requirement.
        std::vector<ImageUse> m_Images;
        std::vector<BufferUse> m_Buffers;
    };

    explicit RenderGraph(bool debugLabels) : m_DebugLabels(debugLabels) {}

    // Records barriers + the pass immediately into `cmd`.
    void Pass(vk::CommandBuffer cmd, const char* name, const std::function<void(PassBuilder&)>& declare,
              const std::function<void(vk::CommandBuffer)>& execute);

private:
    bool m_DebugLabels;
};

} // namespace Astral::Render
