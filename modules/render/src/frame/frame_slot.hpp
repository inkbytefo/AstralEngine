#pragma once

#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace astral {

inline constexpr std::uint32_t ring_size = 3;

/// A host-visible buffer that grows on demand (old buffers are destroyed deferred).
struct GrowBuffer {
    gpu::BufferHandle buffer;
    std::uint64_t capacity = 0;

    void write(gpu::Device& device, const void* data, std::uint64_t bytes) {
        const std::uint64_t needed = std::max<std::uint64_t>(bytes, 64);
        if (capacity < needed) {
            device.destroy(buffer);
            capacity = std::max(needed, capacity * 2);
            buffer = device.create_buffer({.size = capacity, .usage = gpu::BufferUsage::Storage,
                                           .memory = gpu::MemoryUsage::CpuToGpu});
        }
        if (bytes) device.write_buffer(buffer, data, bytes);
    }

    void destroy(gpu::Device& device) {
        device.destroy(buffer);
        buffer = {};
        capacity = 0;
    }
};

struct FrameSlot {
    GrowBuffer instances, lights, shapes, frame, rects, materials, params, debug_lines, ui_vertices, ui_indices;
    std::shared_ptr<std::uint64_t> last_use = std::make_shared<std::uint64_t>(0);

    void destroy(gpu::Device& device) {
        for (GrowBuffer* b : {&instances, &lights, &shapes, &frame, &rects, &materials, &params, &debug_lines,
                              &ui_vertices, &ui_indices}) {
            b->destroy(device);
        }
    }
};

struct FrameRing {
    std::array<FrameSlot, ring_size> slots{};
    std::uint32_t index = 0;

    /// Returns the next ring slot, waiting only if the GPU still reads it.
    FrameSlot& acquire(gpu::Device& device, gpu::CommandList& cmd) {
        FrameSlot& slot = slots[index];
        index = (index + 1) % ring_size;
        device.wait({*slot.last_use});
        cmd.on_submitted([use = slot.last_use](gpu::SubmitTicket t) { *use = t.value; });
        return slot;
    }

    void destroy(gpu::Device& device) {
        for (FrameSlot& slot : slots) slot.destroy(device);
    }
};

} // namespace astral
