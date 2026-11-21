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

TEST_CASE("textures take uploads per mip level") {
    auto device = astral::test::make_device();
    if (!device) return;
    auto t = device->create_texture({.width = 8, .height = 4, .format = astral::gpu::Format::RGBA8Unorm,
                                     .usage = astral::gpu::TextureUsage::Sampled, .mip_levels = 4});
    std::vector<std::uint32_t> mip0(8 * 4, 0xFF0000FF), mip1(4 * 2), mip2(2 * 1), mip3(1);
    CHECK(device->write_texture(t, mip0.data(), mip0.size() * 4, 0));
    CHECK(device->write_texture(t, mip1.data(), mip1.size() * 4, 1));
    CHECK(device->write_texture(t, mip2.data(), mip2.size() * 4, 2));
    CHECK(device->write_texture(t, mip3.data(), mip3.size() * 4, 3));
    CHECK_FALSE(device->write_texture(t, mip3.data(), mip3.size() * 4, 4));  // no such mip
    CHECK_FALSE(device->write_texture(t, mip0.data(), mip0.size() * 4, 1));  // wrong size for mip 1
}

TEST_CASE("block-compressed textures upload whole 4x4 blocks per mip") {
    using astral::gpu::Format;
    CHECK(astral::gpu::texture_data_size(Format::BC7Srgb, 10, 6) == 3 * 2 * 16); // partial blocks round up
    CHECK(astral::gpu::texture_data_size(Format::BC5Unorm, 1, 1) == 16);
    CHECK(astral::gpu::texture_data_size(Format::RGBA8Unorm, 10, 6) == 240);
    auto device = astral::test::make_device();
    if (!device || !device->supports(astral::gpu::Feature::TextureCompressionBC)) return;
    auto t = device->create_texture({.width = 8, .height = 8, .format = Format::BC7Srgb,
                                     .usage = astral::gpu::TextureUsage::Sampled, .mip_levels = 4});
    REQUIRE(t.valid());
    std::vector<std::uint8_t> mip0(2 * 2 * 16), small(16);
    CHECK(device->write_texture(t, mip0.data(), mip0.size(), 0));
    CHECK(device->write_texture(t, small.data(), small.size(), 1)); // 4x4
    CHECK(device->write_texture(t, small.data(), small.size(), 2)); // 2x2: still one block
    CHECK(device->write_texture(t, small.data(), small.size(), 3)); // 1x1
    CHECK_FALSE(device->write_texture(t, small.data(), small.size(), 0));
    // Compressed formats cannot be render targets.
    device.expect_errors(1);
    CHECK_FALSE(device->create_texture({.width = 8, .height = 8, .format = Format::BC7Unorm,
                                        .usage = astral::gpu::TextureUsage::ColorTarget}).valid());
}
