#include "gpu_test_support.hpp"

#include <shaders/fill.comp.hpp>
#include <shaders/solid.frag.hpp>
#include <shaders/triangle.vert.hpp>

#include <array>
#include <cstdint>
#include <vector>

using namespace astral;

TEST_CASE("compute shader fills a buffer through its device address") {
    auto device = test::make_device();
    if (!device) return;
    constexpr std::uint32_t count = 1000;
    auto buffer = device->create_buffer({.size = count * 4, .usage = gpu::BufferUsage::Storage});
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::fill_comp}});
    REQUIRE(pipeline.valid());

    struct Push {
        std::uint64_t address;
        std::uint32_t count;
    } push{device->buffer_address(buffer), count};

    auto cmd = device->begin_commands();
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&push, sizeof(push));
    cmd.dispatch((count + 63) / 64);
    device->wait(device->submit(cmd));

    std::vector<std::uint32_t> out(count);
    REQUIRE(device->read_buffer(buffer, out.data(), count * 4));
    CHECK(out[0] == 0);
    CHECK(out[1] == 2);
    CHECK(out[999] == 1998);
}

TEST_CASE("graphics pipeline draws a triangle into an offscreen target") {
    auto device = test::make_device();
    if (!device) return;
    constexpr std::uint32_t size = 64;
    auto target = device->create_texture({.width = size, .height = size,
                                          .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::ColorTarget |
                                                   gpu::TextureUsage::TransferSrc});
    auto readback = device->create_buffer({.size = size * size * 4,
                                           .usage = gpu::BufferUsage::TransferDst,
                                           .memory = gpu::MemoryUsage::GpuToCpu});
    auto pipeline = device->create_graphics_pipeline({
        .vertex = {shaders::triangle_vert},
        .fragment = {shaders::solid_frag},
        .color_formats = {gpu::Format::RGBA8Unorm},
    });
    REQUIRE(pipeline.valid());

    const std::array<float, 4> red{1, 0, 0, 1};
    auto cmd = device->begin_commands();
    cmd.transition(target, gpu::ResourceState::ColorTarget);
    cmd.begin_rendering({.color = {{.texture = target, .load = gpu::LoadOp::Clear,
                                    .clear = {0, 0, 1, 1}}}});
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(red.data(), sizeof(red));
    cmd.draw(3);
    cmd.end_rendering();
    cmd.transition(target, gpu::ResourceState::TransferSrc);
    cmd.copy_texture_to_buffer(target, readback);
    device->wait(device->submit(cmd));

    std::vector<std::uint32_t> pixels(size * size);
    REQUIRE(device->read_buffer(readback, pixels.data(), pixels.size() * 4));
    const std::uint32_t center = pixels[(size / 2) * size + size / 2];
    const std::uint32_t corner = pixels[0];
    CHECK(center == 0xFF0000FFu); // RGBA8 little-endian: R in the low byte
    CHECK(corner == 0xFFFF0000u); // blue clear
}

TEST_CASE("destroying a resource while the GPU uses it is deferred safely") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 4096, .usage = gpu::BufferUsage::Storage});
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::fill_comp}});
    struct Push {
        std::uint64_t address;
        std::uint32_t count;
    } push{device->buffer_address(buffer), 1024};
    auto cmd = device->begin_commands();
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&push, sizeof(push));
    cmd.dispatch(16);
    auto ticket = device->submit(cmd);
    device->destroy(buffer); // GPU may still be running: must not free memory yet
    device->wait(ticket);
    device->wait_idle();
    CHECK(true);
}

TEST_CASE("command lists can be recorded and submitted repeatedly") {
    auto device = test::make_device();
    if (!device) return;
    for (int i = 0; i < 20; ++i) {
        auto cmd = device->begin_commands();
        device->wait(device->submit(cmd));
    }
    CHECK(true);
}

TEST_CASE("on_submitted reports the ticket of the submission, never for dropped lists") {
    auto device = test::make_device();
    if (!device) return;
    std::uint64_t seen = 0;
    int dropped_calls = 0;
    {
        auto dropped = device->begin_commands();
        dropped.on_submitted([&](gpu::SubmitTicket) { ++dropped_calls; });
    }
    auto cmd = device->begin_commands();
    cmd.on_submitted([&](gpu::SubmitTicket t) { seen = t.value; });
    const gpu::SubmitTicket ticket = device->submit(cmd);
    CHECK(seen == ticket.value);
    CHECK(dropped_calls == 0);
    device->wait(ticket);
}

#include <shaders/read_depth.comp.hpp>
#include <shaders/write_image.comp.hpp>

TEST_CASE("compute writes an HDR storage image without a declared format") {
    auto device = test::make_device();
    if (!device) return;
    auto image = device->create_texture({.width = 8, .height = 8, .format = gpu::Format::RGBA16Float,
                                         .usage = gpu::TextureUsage::Storage | gpu::TextureUsage::TransferSrc});
    auto readback = device->create_buffer({.size = 8 * 8 * 8, .usage = gpu::BufferUsage::TransferDst,
                                           .memory = gpu::MemoryUsage::GpuToCpu});
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::write_image_comp}});
    REQUIRE(pipeline.valid());
    const std::uint32_t index = device->bindless_index(image);
    auto cmd = device->begin_commands();
    cmd.transition(image, gpu::ResourceState::ShaderWrite);
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&index, sizeof(index));
    cmd.dispatch(1, 1);
    cmd.transition(image, gpu::ResourceState::TransferSrc);
    cmd.copy_texture_to_buffer(image, readback);
    device->wait(device->submit(cmd));
    std::uint16_t half[4];
    REQUIRE(device->read_buffer(readback, half, sizeof(half)));
    CHECK(half[0] == 0x3400); // 0.25 as IEEE half
    CHECK(half[2] == 0x3A00); // 0.75
}

TEST_CASE("a depth target can be sampled by a shader") {
    auto device = test::make_device();
    if (!device) return;
    auto depth = device->create_texture({.width = 4, .height = 4, .format = gpu::Format::D32Float,
                                         .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled});
    auto out = device->create_buffer({.size = 4, .usage = gpu::BufferUsage::Storage, .memory = gpu::MemoryUsage::GpuToCpu});
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::read_depth_comp}});
    struct Push {
        std::uint64_t address;
        std::uint32_t depth;
    } push{device->buffer_address(out), device->bindless_index(depth)};
    auto cmd = device->begin_commands();
    cmd.transition(depth, gpu::ResourceState::DepthTarget);
    cmd.begin_rendering({.depth = {.texture = depth, .load = gpu::LoadOp::Clear, .clear = 0.5f}});
    cmd.end_rendering();
    cmd.transition(depth, gpu::ResourceState::ShaderRead);
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&push, sizeof(push));
    cmd.dispatch(1);
    device->wait(device->submit(cmd));
    float value = 0;
    REQUIRE(device->read_buffer(out, &value, sizeof(value)));
    CHECK(value == doctest::Approx(0.5f));
}
