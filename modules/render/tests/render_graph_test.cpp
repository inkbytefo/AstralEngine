#include "gpu_test_support.hpp"

#include <astral/render/render_graph.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace astral;

namespace {
RGTextureDesc color_desc(std::uint32_t size = 16) { return {size, size, gpu::Format::RGBA8Unorm}; }
} // namespace

TEST_CASE("passes run in order and unused passes are culled") {
    auto device = test::make_device();
    if (!device) return;
    RenderGraph graph(*device);
    auto output = device->create_texture({.width = 16, .height = 16, .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled});
    std::vector<std::string> ran;

    graph.begin_frame();
    RGTexture out = graph.import(output, "output");
    RGTexture scratch = graph.create("scratch", color_desc());
    graph.add_pass("unused", [&](PassBuilder& b) { b.color(scratch); }, [&](PassContext&) { ran.push_back("unused"); });
    graph.add_pass("first", [&](PassBuilder& b) { b.color(out); }, [&](PassContext&) { ran.push_back("first"); });
    graph.add_pass("debug", [&](PassBuilder& b) { b.side_effect(); }, [&](PassContext&) { ran.push_back("debug"); });
    auto cmd = device->begin_commands();
    graph.execute(cmd);
    device->wait(device->submit(cmd));

    CHECK(ran == std::vector<std::string>{"first", "debug"});
    CHECK(graph.stats().passes_culled == 1);
}

TEST_CASE("a pass that feeds a needed pass is kept, and data flows through barriers") {
    auto device = test::make_device();
    if (!device) return;
    RenderGraph graph(*device);
    auto output = device->create_texture({.width = 16, .height = 16, .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::TransferDst | gpu::TextureUsage::TransferSrc});
    auto readback = device->create_buffer({.size = 16 * 16 * 4, .usage = gpu::BufferUsage::TransferDst,
                                           .memory = gpu::MemoryUsage::GpuToCpu});
    auto staging = device->create_buffer({.size = 16 * 16 * 4, .usage = gpu::BufferUsage::TransferSrc,
                                          .memory = gpu::MemoryUsage::CpuToGpu});

    graph.begin_frame();
    RGTexture out = graph.import(output, "output", gpu::ResourceState::TransferSrc);
    RGTexture mid = graph.create("mid", color_desc());
    // Pass 1 clears `mid` to green; pass 2 copies `mid` into the output through a buffer.
    graph.add_pass("clear", [&](PassBuilder& b) { b.color(mid, gpu::LoadOp::Clear, {0, 1, 0, 1}); },
                   [](PassContext&) {});
    graph.add_pass("copy", [&](PassBuilder& b) {
        b.copy_src(mid);
        b.copy_dst(out);
    }, [&](PassContext& ctx) {
        ctx.cmd.copy_texture_to_buffer(ctx.texture(mid), staging);
        ctx.cmd.memory_barrier();
        ctx.cmd.copy_buffer_to_texture(staging, ctx.texture(out));
    });
    auto cmd = device->begin_commands();
    graph.execute(cmd);
    cmd.copy_texture_to_buffer(output, readback); // graph left `out` in TransferSrc
    device->wait(device->submit(cmd));

    std::uint32_t pixel = 0;
    REQUIRE(device->read_buffer(readback, &pixel, 4));
    CHECK(pixel == 0xFF00FF00u); // opaque green
    CHECK(graph.stats().passes_executed == 2);
}

TEST_CASE("transient textures are pooled across frames") {
    auto device = test::make_device();
    if (!device) return;
    RenderGraph graph(*device);
    auto output = device->create_texture({.width = 16, .height = 16, .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::ColorTarget});
    gpu::TextureHandle first{};
    for (int frame = 0; frame < 3; ++frame) {
        graph.begin_frame();
        RGTexture out = graph.import(output, "output");
        RGTexture tmp = graph.create("tmp", color_desc());
        graph.add_pass("a", [&](PassBuilder& b) { b.color(tmp); }, [](PassContext&) {});
        graph.add_pass("b", [&](PassBuilder& b) {
            b.read(tmp);
            b.color(out);
        }, [](PassContext&) {});
        auto cmd = device->begin_commands();
        graph.execute(cmd);
        device->wait(device->submit(cmd));
        if (frame == 0) first = graph.texture(tmp);
        else CHECK(graph.texture(tmp) == first);
    }
    CHECK(graph.stats().transient_textures == 1);
}

TEST_CASE("pass context exposes the attachment extent and bindless indices") {
    auto device = test::make_device();
    if (!device) return;
    RenderGraph graph(*device);
    auto output = device->create_texture({.width = 32, .height = 8, .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::ColorTarget});
    graph.begin_frame();
    RGTexture out = graph.import(output, "output");
    RGTexture lookup = graph.create("lookup", color_desc(4));
    gpu::Extent2D extent{};
    std::uint32_t index = UINT32_MAX;
    graph.add_pass("fill", [&](PassBuilder& b) { b.color(lookup); }, [](PassContext&) {});
    graph.add_pass("use", [&](PassBuilder& b) {
        b.read(lookup);
        b.color(out);
    }, [&](PassContext& ctx) {
        extent = ctx.extent;
        index = ctx.bindless(lookup);
    });
    auto cmd = device->begin_commands();
    graph.execute(cmd);
    device->wait(device->submit(cmd));
    CHECK(extent.width == 32);
    CHECK(extent.height == 8);
    CHECK(index != UINT32_MAX);
}
