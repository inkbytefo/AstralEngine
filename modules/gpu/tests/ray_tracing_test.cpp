#include "gpu_test_support.hpp"

#include <shaders/ray_probe.comp.hpp>

#include <array>
#include <cstdint>
#include <vector>

using namespace astral;

namespace {

struct Vtx {
    float x, y, z, pad[5]; // 32-byte stride like the renderer's vertices
};

// A unit quad in the z = 0 plane: two triangles.
const std::array<Vtx, 4> quad_vertices{{{0, 0, 0, {}}, {1, 0, 0, {}}, {1, 1, 0, {}}, {0, 1, 0, {}}}};
const std::array<std::uint32_t, 6> quad_indices{0, 1, 2, 0, 2, 3};

gpu::TlasInstance instance_at(gpu::AccelerationStructureHandle blas, float x, std::uint32_t custom) {
    gpu::TlasInstance inst;
    inst.blas = blas;
    inst.custom_index = custom;
    inst.transform = {1, 0, 0, x, 0, 1, 0, 0, 0, 0, 1, 0};
    return inst;
}

} // namespace

TEST_CASE("ray queries hit triangles through a TLAS built in a command list") {
    auto device = test::make_device();
    if (!device) return;
    if (!device->supports(gpu::Feature::RayTracing)) {
        MESSAGE("no ray tracing support — skipping");
        return;
    }
    auto vertices = device->create_buffer({.size = sizeof(quad_vertices)});
    auto indices = device->create_buffer({.size = sizeof(quad_indices), .usage = gpu::BufferUsage::Index});
    REQUIRE(device->write_buffer(vertices, quad_vertices.data(), sizeof(quad_vertices)));
    REQUIRE(device->write_buffer(indices, quad_indices.data(), sizeof(quad_indices)));

    auto blas = device->create_blas({.vertices = vertices, .vertex_count = 4, .vertex_stride = sizeof(Vtx),
                                     .indices = indices, .index_count = 6});
    REQUIRE(blas.valid());
    auto tlas = device->create_tlas({.max_instances = 4});
    REQUIRE(tlas.valid());
    CHECK(device->acceleration_structure_address(tlas) != 0);

    auto results = device->create_buffer({.size = 4 * 16, .memory = gpu::MemoryUsage::GpuToCpu});
    auto pipeline = device->create_compute_pipeline({.shader = {shaders::ray_probe_comp}});
    REQUIRE(pipeline.valid());

    const std::array<gpu::TlasInstance, 2> instances{instance_at(blas, 0, 7), instance_at(blas, 10, 42)};
    struct Push {
        std::uint64_t tlas, results;
    } push{device->acceleration_structure_address(tlas), device->buffer_address(results)};

    auto cmd = device->begin_commands();
    cmd.build_tlas(tlas, instances);
    cmd.bind_pipeline(pipeline);
    cmd.push_constants(&push, sizeof(push));
    cmd.dispatch(1);
    device->wait(device->submit(cmd));

    std::array<float, 16> r{};
    REQUIRE(device->read_buffer(results, r.data(), sizeof(r)));
    CHECK(r[0] == doctest::Approx(5.0f));  // hit distance
    CHECK(r[1] == 7.0f);                   // custom index of instance 0
    CHECK(r[2] == 1.0f);                   // (0.25, 0.6) lies in the second triangle (y > x)
    CHECK(r[4] == -1.0f);                  // miss
    CHECK(r[8] == doctest::Approx(5.0f));  // second instance, moved by its transform
    CHECK(r[9] == 42.0f);
    CHECK(r[12] == -1.0f);                 // tmax too short

    // Rebuilding with fewer instances in a later frame works and drops the second instance.
    auto cmd2 = device->begin_commands();
    cmd2.build_tlas(tlas, std::span(instances).first(1));
    cmd2.bind_pipeline(pipeline);
    cmd2.push_constants(&push, sizeof(push));
    cmd2.dispatch(1);
    device->wait(device->submit(cmd2));
    REQUIRE(device->read_buffer(results, r.data(), sizeof(r)));
    CHECK(r[0] == doctest::Approx(5.0f));
    CHECK(r[8] == -1.0f);

    device->destroy(tlas);
    device->destroy(blas);
}

TEST_CASE("acceleration structure misuse is refused, not crashed") {
    auto device = test::make_device();
    if (!device) return;
    if (!device->supports(gpu::Feature::RayTracing)) return;
    device.expect_errors(3);
    CHECK_FALSE(device->create_blas({}).valid());                 // no geometry
    CHECK_FALSE(device->create_tlas({.max_instances = 0}).valid());
    auto tlas = device->create_tlas({.max_instances = 1});
    const std::array<gpu::TlasInstance, 2> two{};
    auto cmd = device->begin_commands();
    cmd.build_tlas(tlas, two); // more than max_instances
    device->wait(device->submit(cmd));
    CHECK(device->acceleration_structure_address({}) == 0);
}
