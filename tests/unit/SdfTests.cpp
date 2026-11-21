#include <doctest/doctest.h>

#include "Astral/Engine/Physics/SdfMeshing.hpp"
#include "Astral/Render/SdfMath.hpp"

#include <vector>

using namespace Astral;
using namespace Astral::Render;

namespace {
SdfShapeDesc Sphere(glm::vec3 p, float r, SdfOperation op = SdfOperation::Union) {
    SdfShapeDesc s;
    s.primitive = SdfPrimitive::Sphere;
    s.position = p;
    s.size = glm::vec4(r);
    s.operation = op;
    return s;
}
} // namespace

TEST_CASE("Primitive distances") {
    CHECK(Sdf::Sphere({2, 0, 0}, 1.0f) == doctest::Approx(1.0f));
    CHECK(Sdf::Box({0, 2, 0}, glm::vec3(1), 0.0f) == doctest::Approx(1.0f));
    CHECK(Sdf::Box({0, 0, 0}, glm::vec3(1), 0.0f) == doctest::Approx(-1.0f));
    CHECK(Sdf::Torus({2, 0, 0}, 2.0f, 0.5f) == doctest::Approx(-0.5f));
    CHECK(Sdf::Capsule({0, 3, 0}, 0.5f, 1.0f) == doctest::Approx(1.5f));
    CHECK(Sdf::Cylinder({0, 2, 0}, 1.0f, 1.0f, 0.0f) == doctest::Approx(1.0f));
}

TEST_CASE("Rotation is applied in the shape's local space") {
    SdfShapeDesc box;
    box.primitive = SdfPrimitive::Box;
    box.size = {2.0f, 0.5f, 0.5f, 0.0f};
    // Unrotated: the box reaches x = 2.
    CHECK(Sdf::Primitive(box, {1.9f, 0, 0}) < 0.0f);
    // Rotated 90 degrees about Z: the long axis now points along Y.
    box.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
    CHECK(Sdf::Primitive(box, {1.9f, 0, 0}) > 0.0f);
    CHECK(Sdf::Primitive(box, {0, 1.9f, 0}) < 0.0f);
}

TEST_CASE("CSG operations combine in order") {
    std::vector<SdfShapeDesc> shapes = {Sphere({0, 0, 0}, 1.0f), Sphere({0, 0, 0}, 0.5f, SdfOperation::Subtract)};
    CHECK(Sdf::Evaluate(shapes, {0, 0, 0}) > 0.0f);        // hollowed out
    CHECK(Sdf::Evaluate(shapes, {0.75f, 0, 0}) < 0.0f);    // inside the shell

    std::vector<SdfShapeDesc> blend = {Sphere({-0.6f, 0, 0}, 0.5f), Sphere({0.6f, 0, 0}, 0.5f, SdfOperation::SmoothUnion)};
    blend[1].blend = 0.5f;
    CHECK(Sdf::Evaluate(blend, {0, 0, 0}) < 0.0f); // the smooth union bridges the gap
    blend[1].operation = SdfOperation::Union;
    CHECK(Sdf::Evaluate(blend, {0, 0, 0}) > 0.0f);
}

TEST_CASE("Picking finds the dominant shape") {
    std::vector<SdfShapeDesc> shapes = {Sphere({-2, 0, 0}, 1.0f), Sphere({2, 0, 0}, 1.0f)};
    shapes[0].objectId = 1;
    shapes[1].objectId = 2;
    CHECK(Sdf::DominantShape(shapes, {2.5f, 0, 0}) == 1);
    CHECK(Sdf::DominantShape(shapes, {-2.5f, 0, 0}) == 0);
}

TEST_CASE("Surface nets mesh lies on the surface with outward-facing triangles") {
    std::vector<SdfShapeDesc> shapes = {Sphere({0, 0, 0}, 1.0f)};
    TriangleMesh mesh = MeshSdfRegion(shapes, glm::vec3(-1.5f), glm::vec3(1.5f), 0.1f);
    REQUIRE(mesh.indices.size() > 300);
    REQUIRE(mesh.indices.size() % 3 == 0);

    for (const glm::vec3& v : mesh.vertices) CHECK(std::abs(glm::length(v) - 1.0f) < 0.05f);

    int outward = 0, total = 0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        glm::vec3 a = mesh.vertices[mesh.indices[i]], b = mesh.vertices[mesh.indices[i + 1]], c = mesh.vertices[mesh.indices[i + 2]];
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::dot(n, (a + b + c) / 3.0f) > 0.0f) ++outward;
        ++total;
    }
    CHECK(outward == total);
}
