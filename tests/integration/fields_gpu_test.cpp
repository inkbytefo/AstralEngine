// The GPU twin of the fields (fields.glsl) must give the CPU's numbers (M12.4).
#include <astral/gpu/device.hpp>
#include <astral/physics/fields.hpp>

#include <doctest/doctest.h>

#include <shaders/fields_probe.comp.hpp>

#include <cmath>
#include <vector>

using namespace astral;

TEST_CASE("wind, noise, volume weight and water surface: GPU = CPU") {
    auto device = gpu::Device::create({.app_name = "astral_fields_test"});
    if (!device) {
        MESSAGE("no Vulkan 1.3 device available — skipping");
        return;
    }
    constexpr std::uint32_t count = 512;
    const WindDesc wind{.velocity = {8, 1, -3}, .gust_strength = 3.0f, .gust_frequency = 0.7f, .turbulence = 1.5f, .turbulence_scale = 6.0f, .seed = 42};
    const WaterVolume water{.center = {1, -2, 3}, .half_extents = {30, 2, 30}, .wave_height = 0.4f, .wave_length = 9.0f, .wave_speed = 1.3f};

    std::vector<float> points;
    std::vector<Vec3> positions;
    std::vector<float> times;
    for (std::uint32_t i = 0; i < count; ++i) {
        const float f = static_cast<float>(i);
        const Vec3 p{std::sin(f * 0.37f) * 20.0f + f * 0.11f, std::cos(f * 0.21f) * 6.0f, std::sin(f * 0.13f + 1.0f) * 25.0f - f * 0.05f};
        const float t = f * 0.173f;
        positions.push_back(p);
        times.push_back(t);
        points.insert(points.end(), {p.x, p.y, p.z, t});
    }
    struct WindGpu {
        float vx, vy, vz, gust_strength, gust_frequency, turbulence, turbulence_scale;
        std::uint32_t seed;
    } wind_gpu{wind.velocity.x, wind.velocity.y, wind.velocity.z, wind.gust_strength, wind.gust_frequency, wind.turbulence, wind.turbulence_scale, wind.seed};
    struct WaterGpu {
        float cx, cy, cz, wave_height, hx, hy, hz, wave_length, wave_speed;
    } water_gpu{water.center.x, water.center.y, water.center.z, water.wave_height, water.half_extents.x, water.half_extents.y, water.half_extents.z, water.wave_length, water.wave_speed};

    auto in_points = device->create_buffer({.size = points.size() * 4, .usage = gpu::BufferUsage::Storage | gpu::BufferUsage::TransferDst, .memory = gpu::MemoryUsage::CpuToGpu});
    auto in_wind = device->create_buffer({.size = sizeof(WindGpu), .usage = gpu::BufferUsage::Storage, .memory = gpu::MemoryUsage::CpuToGpu});
    auto in_water = device->create_buffer({.size = sizeof(WaterGpu), .usage = gpu::BufferUsage::Storage, .memory = gpu::MemoryUsage::CpuToGpu});
    auto out_wind = device->create_buffer({.size = count * 16, .usage = gpu::BufferUsage::Storage});
    auto out_misc = device->create_buffer({.size = count * 16, .usage = gpu::BufferUsage::Storage});
    REQUIRE(device->write_buffer(in_points, points.data(), points.size() * 4));
    REQUIRE(device->write_buffer(in_wind, &wind_gpu, sizeof wind_gpu));
    REQUIRE(device->write_buffer(in_water, &water_gpu, sizeof water_gpu));
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::fields_probe_comp}});
    REQUIRE(pipeline.valid());
    struct Push {
        std::uint64_t points, wind, water, out_wind, out_misc;
        std::uint32_t count;
    } push{device->buffer_address(in_points), device->buffer_address(in_wind), device->buffer_address(in_water),
           device->buffer_address(out_wind), device->buffer_address(out_misc), count};
    auto cmd = device->begin_commands();
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&push, sizeof push);
    cmd.dispatch((count + 63) / 64);
    device->wait(device->submit(cmd));
    std::vector<float> got_wind(count * 4), got_misc(count * 4);
    REQUIRE(device->read_buffer(out_wind, got_wind.data(), got_wind.size() * 4));
    REQUIRE(device->read_buffer(out_misc, got_misc.data(), got_misc.size() * 4));

    float worst = 0.0f;
    for (std::uint32_t i = 0; i < count; ++i) {
        const Vec3 p = positions[i];
        const float t = times[i];
        const Vec3 w = wind_of(wind, p, t);
        const float noise = field_noise(p, 7);
        const float weight = volume_weight(p, {0, 0, 0}, {5, 5, 5}, 3.0f);
        const float surface = water_surface_height(water, p.x, p.z, t);
        worst = std::max({worst, std::abs(w.x - got_wind[i * 4]), std::abs(w.y - got_wind[i * 4 + 1]), std::abs(w.z - got_wind[i * 4 + 2]),
                          std::abs(noise - got_wind[i * 4 + 3]), std::abs(weight - got_misc[i * 4]), std::abs(surface - got_misc[i * 4 + 1])});
    }
    CHECK(worst < 2e-4f);
}
