// Regression tests for the Master Stabilization Roadmap (docs/status/master-stabilization-roadmap.md).
// Each case names the finding it pins down.
#include "gpu_test_support.hpp"

#include <cstdint>
#include <limits>
#include <utility>

using namespace astral;

TEST_CASE("G01: move-assigning a recording list onto another recording list keeps both usable") {
    auto device = test::make_device();
    if (!device) return;
    const gpu::TextureDesc desc{.width = 4, .height = 4, .format = gpu::Format::RGBA8Unorm,
                                .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled};
    auto tex_a = device->create_texture(desc);
    auto tex_b = device->create_texture(desc);
    auto a = device->begin_commands();
    auto b = device->begin_commands();
    a.transition(tex_a, gpu::ResourceState::ColorTarget); // dropped by the assignment below
    b.transition(tex_b, gpu::ResourceState::ShaderRead);
    a = std::move(b); // ends a's recording, then takes b's
    CHECK(a.valid());
    CHECK_FALSE(b.valid());
    device->wait(device->submit(a));
    // tex_a's transition never ran: a fresh list must start it from Undefined again.
    auto c = device->begin_commands();
    c.transition(tex_a, gpu::ResourceState::ShaderRead);
    device->wait(device->submit(c));
}

TEST_CASE("G02: a resource destroyed while two lists record it survives until the later one completes") {
    auto device = test::make_device();
    if (!device) return;
    auto src = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto dst_a = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto dst_b = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto a = device->begin_commands();
    auto b = device->begin_commands();
    a.copy_buffer(src, dst_a, 1024);
    b.copy_buffer(src, dst_b, 1024);
    device->destroy(src);            // both open lists still reference it
    auto c = device->begin_commands(); // opened later; submits first and takes the next ticket
    device->wait(device->submit(c));
    device->wait(device->submit(a)); // garbage collection runs here
    device->wait(device->submit(b)); // must not copy from freed memory (validation error otherwise)
}

TEST_CASE("G05: block-compressed copies check the real block-sized byte count") {
    auto device = test::make_device();
    if (!device || !device->supports(gpu::Feature::TextureCompressionBC)) return;
    device.expect_errors(2);
    auto tex = device->create_texture({.width = 8, .height = 8, .format = gpu::Format::BC7Unorm,
                                       .usage = gpu::TextureUsage::TransferSrc | gpu::TextureUsage::TransferDst |
                                                gpu::TextureUsage::Sampled});
    REQUIRE(tex.valid());
    // 8x8 BC7 is four 16-byte blocks = 64 bytes; 16 bytes is too small for both directions.
    auto small = device->create_buffer({.size = 16, .usage = gpu::BufferUsage::TransferDst | gpu::BufferUsage::TransferSrc});
    auto cmd = device->begin_commands();
    cmd.transition(tex, gpu::ResourceState::TransferDst);
    cmd.copy_buffer_to_texture(small, tex); // rejected
    cmd.transition(tex, gpu::ResourceState::TransferSrc);
    cmd.copy_texture_to_buffer(tex, small); // rejected
    device->wait(device->submit(cmd));
}

TEST_CASE("G06: timestamp query ranges whose end wraps uint32 are rejected") {
    auto device = test::make_device();
    if (!device || !device->supports(gpu::Feature::Timestamps)) return;
    device.expect_errors(1);
    auto pool = device->create_timestamp_pool(4);
    REQUIRE(pool.valid());
    std::uint64_t out[4] = {};
    constexpr std::uint32_t huge = std::numeric_limits<std::uint32_t>::max();
    CHECK_FALSE(device->read_timestamps(pool, 2, huge, out)); // 2 + huge wraps to 1
    auto cmd = device->begin_commands();
    cmd.reset_queries(pool, 2, huge); // logged and skipped, not recorded
    device->wait(device->submit(cmd));
    device->destroy(pool);
}

TEST_CASE("G09: a list from another device is refused and not consumed") {
    auto first = test::make_device();
    auto second = test::make_device();
    if (!first || !second) return;
    second.expect_errors(1);
    auto cmd = first->begin_commands();
    const gpu::SubmitTicket wrong = second->submit(cmd); // logged, nothing submitted
    CHECK(second->is_complete(wrong));
    CHECK(cmd.valid()); // still owned by `first`
    first->wait(first->submit(cmd));
}

TEST_CASE("G03: dropping a list that a later open list built on keeps the later list's layouts valid") {
    auto device = test::make_device();
    if (!device) return;
    auto tex = device->create_texture({.width = 4, .height = 4, .format = gpu::Format::RGBA8Unorm,
                                       .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled});
    auto b = device->begin_commands();
    {
        auto a = device->begin_commands();
        a.transition(tex, gpu::ResourceState::ColorTarget);
        b.transition(tex, gpu::ResourceState::ShaderRead); // barrier from ColorTarget, which only `a` set up
    } // `a` dropped: the GPU never runs its transition
    device->wait(device->submit(b)); // a fix-up barrier runs first: no layout mismatch
    auto c = device->begin_commands();
    c.transition(tex, gpu::ResourceState::ColorTarget); // and the tracked state is right afterwards
    device->wait(device->submit(c));
}

TEST_CASE("G03: dropping both lists in turn leaves a consistent state") {
    auto device = test::make_device();
    if (!device) return;
    auto tex = device->create_texture({.width = 4, .height = 4, .format = gpu::Format::RGBA8Unorm,
                                       .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled});
    {
        auto b = device->begin_commands();
        {
            auto a = device->begin_commands();
            a.transition(tex, gpu::ResourceState::ColorTarget);
            b.transition(tex, gpu::ResourceState::ShaderRead);
        }
    } // both dropped
    auto c = device->begin_commands();
    c.transition(tex, gpu::ResourceState::ShaderRead);
    device->wait(device->submit(c));
}
