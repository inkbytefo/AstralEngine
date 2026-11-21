#include "gpu_test_support.hpp"

#include <array>
#include <cstdint>

using namespace astral;

TEST_CASE("gpu-only buffer round-trips data through staging") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 256, .usage = gpu::BufferUsage::Storage,
                                         .memory = gpu::MemoryUsage::GpuOnly});
    REQUIRE(buffer.valid());

    std::array<std::uint32_t, 64> in{};
    for (std::uint32_t i = 0; i < in.size(); ++i) in[i] = i * 7u;
    REQUIRE(device->write_buffer(buffer, in.data(), sizeof(in)));

    std::array<std::uint32_t, 64> out{};
    REQUIRE(device->read_buffer(buffer, out.data(), sizeof(out)));
    CHECK(out == in);
    device->destroy(buffer);
}

TEST_CASE("host-visible buffer round-trips with an offset") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 64, .usage = gpu::BufferUsage::Storage,
                                         .memory = gpu::MemoryUsage::CpuToGpu});
    const std::uint32_t value = 0xDEADBEEF;
    REQUIRE(device->write_buffer(buffer, &value, sizeof(value), 16));
    std::uint32_t read = 0;
    REQUIRE(device->read_buffer(buffer, &read, sizeof(read), 16));
    CHECK(read == value);
}

TEST_CASE("writes outside the buffer are rejected") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 16, .usage = gpu::BufferUsage::Storage});
    std::array<std::uint8_t, 32> data{};
    CHECK_FALSE(device->write_buffer(buffer, data.data(), data.size()));
    CHECK_FALSE(device->write_buffer(buffer, data.data(), 8, 12));
}

TEST_CASE("destroyed buffer handle is rejected") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 16, .usage = gpu::BufferUsage::Storage});
    device->destroy(buffer);
    std::uint32_t v = 1;
    CHECK_FALSE(device->write_buffer(buffer, &v, sizeof(v)));
    CHECK(device->buffer_address(buffer) == 0);
}

TEST_CASE("buffers expose a device address") {
    auto device = test::make_device();
    if (!device) return;
    auto buffer = device->create_buffer({.size = 64, .usage = gpu::BufferUsage::Storage});
    CHECK(device->buffer_address(buffer) != 0);
}

TEST_CASE("sampled textures get distinct bindless indices") {
    auto device = test::make_device();
    if (!device) return;
    gpu::TextureDesc desc{.width = 4, .height = 4, .format = gpu::Format::RGBA8Unorm,
                          .usage = gpu::TextureUsage::Sampled};
    auto a = device->create_texture(desc);
    auto b = device->create_texture(desc);
    REQUIRE(a.valid());
    REQUIRE(b.valid());
    CHECK(device->bindless_index(a) != device->bindless_index(b));
    CHECK(device->texture_desc(a).width == 4);
}

TEST_CASE("invalid descriptions are refused") {
    auto device = test::make_device();
    if (!device) return;
    device.expect_errors(2); // both refusals are logged
    CHECK_FALSE(device->create_buffer({.size = 0}).valid());
    CHECK_FALSE(device->create_texture({.width = 0, .height = 4}).valid());
}
