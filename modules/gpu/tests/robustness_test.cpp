// Regression tests for the M1 review findings: misuse must never crash or corrupt memory.
#include "gpu_test_support.hpp"

#include <shaders/fill.comp.hpp>

#include <array>
#include <cstdint>

using namespace astral;

TEST_CASE("buffer destroyed while a recorded but unsubmitted list uses it stays alive") {
    auto device = test::make_device();
    if (!device) return;
    auto a = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto b = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto cmd = device->begin_commands();
    cmd.copy_buffer(a, b, 1024);
    device->destroy(a);                          // still referenced by `cmd`
    { auto other = device->begin_commands(); }   // collects garbage
    device->wait(device->submit(cmd));           // must not use freed memory
}

TEST_CASE("draw and dispatch without a valid pipeline are skipped, not crashed") {
    auto device = test::make_device();
    if (!device) return;
    device.expect_errors(2);
    auto cmd = device->begin_commands();
    cmd.bind_pipeline(gpu::PipelineHandle{}); // logged
    cmd.dispatch(4);                          // logged and skipped
    device->wait(device->submit(cmd));
}

TEST_CASE("out-of-range copies are rejected") {
    auto device = test::make_device();
    if (!device) return;
    device.expect_errors(2);
    auto tex = device->create_texture({.width = 64, .height = 64, .format = gpu::Format::RGBA8Unorm,
                                       .usage = gpu::TextureUsage::TransferSrc | gpu::TextureUsage::Sampled});
    auto small = device->create_buffer({.size = 16, .usage = gpu::BufferUsage::TransferDst});
    auto big = device->create_buffer({.size = 1024, .usage = gpu::BufferUsage::Storage});
    auto cmd = device->begin_commands();
    cmd.transition(tex, gpu::ResourceState::TransferSrc);
    cmd.copy_texture_to_buffer(tex, small); // too small: rejected
    cmd.copy_buffer(big, small, 512);       // too big: rejected
    device->wait(device->submit(cmd));
}

TEST_CASE("a dropped command list does not leave texture states behind") {
    auto device = test::make_device();
    if (!device) return;
    auto tex = device->create_texture({.width = 4, .height = 4, .format = gpu::Format::RGBA8Unorm,
                                       .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::Sampled});
    {
        auto dropped = device->begin_commands();
        dropped.transition(tex, gpu::ResourceState::ColorTarget);
    } // never submitted: the texture is still Undefined on the GPU
    auto cmd = device->begin_commands();
    cmd.transition(tex, gpu::ResourceState::ShaderRead); // must transition from Undefined
    device->wait(device->submit(cmd));
}

TEST_CASE("staged uploads are refused while a command list is recording") {
    auto device = test::make_device();
    if (!device) return;
    device.expect_errors(1);
    auto buffer = device->create_buffer({.size = 64, .usage = gpu::BufferUsage::Storage}); // GPU-only
    auto cmd = device->begin_commands();
    std::array<std::uint8_t, 64> data{};
    CHECK_FALSE(device->write_buffer(buffer, data.data(), data.size()));
    device->wait(device->submit(cmd));
    CHECK(device->write_buffer(buffer, data.data(), data.size())); // fine once nothing records
}

TEST_CASE("push constants larger than the limit are rejected even when the sum wraps") {
    auto device = test::make_device();
    if (!device) return;
    device.expect_errors(1);
    std::array<std::uint8_t, 16> data{};
    auto cmd = device->begin_commands();
    cmd.push_constants(data.data(), 16, 0xFFFFFFF8u); // offset + size wraps around 32 bits
    device->wait(device->submit(cmd));
}

TEST_CASE("using a command list after submit is a logged no-op") {
    auto device = test::make_device();
    if (!device) return;
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::fill_comp}});
    auto cmd = device->begin_commands();
    device->wait(device->submit(cmd));
    cmd.bind_pipeline(pipeline); // list is consumed: ignored
    cmd.dispatch(1);
    CHECK_FALSE(cmd.valid());
}
