#include "physics_internal.hpp"

#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <cstring>

namespace astral {

namespace {

constexpr std::array<std::uint8_t, 4> kCookedMagic = {'A', 'S', 'S', 'H'};
constexpr std::uint32_t kCookedFormatVersion = 1;
constexpr std::uint32_t kFlagStaticOnly = 1u << 0;

struct CookedHeader {
    std::array<std::uint8_t, 4> magic = kCookedMagic;
    std::uint32_t version = kCookedFormatVersion;
    std::uint32_t backend_hash = 0;
    std::uint32_t shape_type = 0;
    std::uint32_t flags = 0;
    std::uint32_t payload_bytes = 0;
};

static_assert(sizeof(CookedHeader) == 24);

class MemoryStreamOut final : public JPH::StreamOut {
public:
    void WriteBytes(const void* data, std::size_t n) override {
        const auto* bytes = static_cast<const std::byte*>(data);
        buffer.insert(buffer.end(), bytes, bytes + n);
    }
    bool IsFailed() const override { return false; }

    std::vector<std::byte> buffer;
};

class MemoryStreamIn final : public JPH::StreamIn {
public:
    explicit MemoryStreamIn(std::span<const std::byte> data) : data_(data) {}
    void ReadBytes(void* out, std::size_t n) override {
        if (offset_ + n > data_.size()) {
            eof_ = true;
            failed_ = true;
            return;
        }
        std::memcpy(out, data_.data() + offset_, n);
        offset_ += n;
    }
    bool IsEOF() const override { return eof_; }
    bool IsFailed() const override { return failed_; }

private:
    std::span<const std::byte> data_;
    std::size_t offset_ = 0;
    bool eof_ = false;
    bool failed_ = false;
};

/// A hull needs volume: not all points on one plane. Order-independent (duplicates and collinear
/// runs anywhere in the list are fine): b is the point farthest from a, c the point farthest from
/// line ab, and some point must sit off the plane abc.
bool hull_has_volume(const std::vector<Vec3>& pts) {
    const Vec3 a = pts[0];
    float extent = 0.0f;
    Vec3 b = a;
    for (const Vec3& p : pts) {
        const float d = length(p - a);
        if (d > extent) {
            extent = d;
            b = p;
        }
    }
    const float eps = 1e-4f * std::max(extent, 1.0f);
    if (extent <= eps) return false; // all points coincide
    const Vec3 ab = normalize(b - a);
    Vec3 normal{};
    float best = 0.0f;
    for (const Vec3& p : pts) {
        const Vec3 n = cross(ab, p - a); // |n| = distance from line ab
        if (length(n) > best) {
            best = length(n);
            normal = n;
        }
    }
    if (best <= eps) return false; // all points on one line
    normal = normalize(normal);
    for (const Vec3& p : pts) {
        if (std::abs(dot(p - a, normal)) > eps) return true;
    }
    return false;
}

bool valid_shape(const ShapeDesc& d) {
    switch (d.type) {
    case ShapeType::Box:
        return positive_scalar(d.half_extents.x) && positive_scalar(d.half_extents.y) && positive_scalar(d.half_extents.z);
    case ShapeType::Sphere: return positive_scalar(d.radius);
    case ShapeType::Capsule:
    case ShapeType::Cylinder: return positive_scalar(d.radius) && positive_scalar(d.half_height);
    case ShapeType::ConvexHull:
        return d.points.size() >= 4 && std::all_of(d.points.begin(), d.points.end(), finite_vec) && hull_has_volume(d.points);
    case ShapeType::Mesh:
        return d.vertices.size() >= 3 && !d.indices.empty() && d.indices.size() % 3 == 0 &&
               std::all_of(d.vertices.begin(), d.vertices.end(), finite_vec) &&
               std::all_of(d.indices.begin(), d.indices.end(), [&](std::uint32_t i) { return i < d.vertices.size(); });
    case ShapeType::HeightField:
        return d.samples >= 4 && positive_scalar(d.cell_size) && d.heights.size() == std::size_t{d.samples} * d.samples &&
               std::all_of(d.heights.begin(), d.heights.end(), [](float h) { return std::isfinite(h); });
    case ShapeType::Compound:
        return !d.children.empty() && std::all_of(d.children.begin(), d.children.end(), [](const ShapeChild& c) {
            return finite_vec(c.position) && valid_shape(c.shape);
        });
    }
    return false;
}

std::vector<std::byte> serialize_jolt_shape(const JPH::Shape& shape, ShapeType type, bool static_only) {
    MemoryStreamOut stream;
    JPH::Shape::ShapeToIDMap shape_map;
    JPH::Shape::MaterialToIDMap material_map;
    shape.SaveWithChildren(stream, shape_map, material_map);

    CookedHeader header{};
    header.backend_hash = jolt_backend_abi_hash();
    header.shape_type = static_cast<std::uint32_t>(type);
    header.flags = static_only ? kFlagStaticOnly : 0u;
    header.payload_bytes = static_cast<std::uint32_t>(stream.buffer.size());

    std::vector<std::byte> out(sizeof(CookedHeader) + stream.buffer.size());
    std::memcpy(out.data(), &header, sizeof(CookedHeader));
    if (!stream.buffer.empty()) std::memcpy(out.data() + sizeof(CookedHeader), stream.buffer.data(), stream.buffer.size());
    return out;
}

} // namespace

std::uint32_t jolt_backend_abi_hash() noexcept {
    // Deterministic FNV-1a fingerprint of the Jolt binary serialization ABI version & precision flags.
    constexpr std::string_view tag = "Jolt-5.7.0-f32-ASSH-v1";
    std::uint32_t h = 2166136261u;
    for (char c : tag) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

/// Meshes and height fields are static geometry: they cannot be a moving body or be swept in a query.
bool shape_needs_static(const ShapeDesc& d) {
    if (d.type == ShapeType::Mesh || d.type == ShapeType::HeightField) return true;
    for (const ShapeChild& c : d.children) {
        if (shape_needs_static(c.shape)) return true;
    }
    return false;
}

JPH::Ref<JPH::Shape> make_jolt_shape(const ShapeDesc& d) {
    if (!valid_shape(d)) return nullptr;
    JPH::ShapeSettings::ShapeResult result;
    switch (d.type) {
    case ShapeType::Box: {
        const float convex = std::min({0.05f, d.half_extents.x * 0.5f, d.half_extents.y * 0.5f, d.half_extents.z * 0.5f});
        result = JPH::BoxShapeSettings(to_jph(d.half_extents), convex).Create();
        break;
    }
    case ShapeType::Sphere: result = JPH::SphereShapeSettings(d.radius).Create(); break;
    case ShapeType::Capsule: result = JPH::CapsuleShapeSettings(d.half_height, d.radius).Create(); break;
    case ShapeType::Cylinder: {
        const float convex = std::min({0.05f, d.radius * 0.5f, d.half_height * 0.5f});
        result = JPH::CylinderShapeSettings(d.half_height, d.radius, convex).Create();
        break;
    }
    case ShapeType::ConvexHull: {
        JPH::Array<JPH::Vec3> points;
        points.reserve(d.points.size());
        for (const Vec3& p : d.points) points.push_back(to_jph(p));
        result = JPH::ConvexHullShapeSettings(points).Create();
        break;
    }
    case ShapeType::Mesh: {
        JPH::VertexList vertices;
        vertices.reserve(d.vertices.size());
        for (const Vec3& v : d.vertices) vertices.push_back(JPH::Float3(v.x, v.y, v.z));
        JPH::IndexedTriangleList triangles;
        for (std::size_t i = 0; i + 2 < d.indices.size(); i += 3) {
            triangles.push_back(JPH::IndexedTriangle(d.indices[i], d.indices[i + 1], d.indices[i + 2]));
        }
        result = JPH::MeshShapeSettings(vertices, triangles).Create();
        break;
    }
    case ShapeType::HeightField: {
        const float half = 0.5f * static_cast<float>(d.samples - 1) * d.cell_size;
        result = JPH::HeightFieldShapeSettings(d.heights.data(), JPH::Vec3(-half, 0, -half),
                                               JPH::Vec3(d.cell_size, 1, d.cell_size), d.samples)
                     .Create();
        break;
    }
    case ShapeType::Compound: {
        JPH::StaticCompoundShapeSettings compound;
        for (const ShapeChild& c : d.children) {
            JPH::Ref<JPH::Shape> child = make_jolt_shape(c.shape);
            if (!child) return nullptr;
            compound.AddShape(to_jph(c.position), to_jph(c.rotation), child);
        }
        result = compound.Create();
        break;
    }
    }
    return result.IsValid() ? result.Get() : nullptr;
}

std::vector<std::byte> cook_physics_shape(const ShapeDesc& desc) {
    acquire_jolt_registry();
    std::vector<std::byte> bytes;
    if (JPH::Ref<JPH::Shape> shape = make_jolt_shape(desc)) {
        bytes = serialize_jolt_shape(*shape, desc.type, shape_needs_static(desc));
    }
    release_jolt_registry();
    return bytes;
}

std::optional<CookedShapeInfo> inspect_cooked_shape(std::span<const std::byte> data) {
    if (data.size() < sizeof(CookedHeader)) return std::nullopt;
    CookedHeader header{};
    std::memcpy(&header, data.data(), sizeof(CookedHeader));
    if (header.magic != kCookedMagic) return std::nullopt;
    if (header.shape_type > static_cast<std::uint32_t>(ShapeType::Compound)) return std::nullopt;
    if (sizeof(CookedHeader) + header.payload_bytes != data.size()) return std::nullopt;
    CookedShapeInfo info{};
    info.format_version = header.version;
    info.backend_hash = header.backend_hash;
    info.shape_type = static_cast<ShapeType>(header.shape_type);
    info.static_only = (header.flags & kFlagStaticOnly) != 0;
    info.payload_bytes = header.payload_bytes;
    info.compatible = (header.version == kCookedFormatVersion && header.backend_hash == jolt_backend_abi_hash());
    return info;
}

ShapeHandle PhysicsWorld::create_shape(const ShapeDesc& desc) {
    auto& m = *impl_;
    JPH::Ref<JPH::Shape> shape = make_jolt_shape(desc);
    if (!shape) {
        m.error("create_shape: invalid shape description");
        return {};
    }
    return m.shapes.create({shape, shape_needs_static(desc)});
}

std::vector<std::byte> PhysicsWorld::cook_shape(const ShapeDesc& desc) const {
    const JPH::Ref<JPH::Shape> shape = make_jolt_shape(desc);
    if (!shape) {
        impl_->error("cook_shape: invalid shape description");
        return {};
    }
    return serialize_jolt_shape(*shape, desc.type, shape_needs_static(desc));
}

ShapeHandle PhysicsWorld::create_shape_from_cooked(std::span<const std::byte> data) {
    auto& m = *impl_;
    const auto info = inspect_cooked_shape(data);
    if (!info) {
        m.error("create_shape_from_cooked: corrupt or unrecognized ASSH binary");
        return {};
    }
    if (!info->compatible) {
        m.error("create_shape_from_cooked: incompatible cooked shape version or backend ABI hash");
        return {};
    }
    MemoryStreamIn stream(data.subspan(sizeof(CookedHeader)));
    JPH::Shape::IDToShapeMap shape_map;
    JPH::Shape::IDToMaterialMap material_map;
    const JPH::Shape::ShapeResult restored = JPH::Shape::sRestoreWithChildren(stream, shape_map, material_map);
    if (restored.HasError() || !restored.IsValid()) {
        m.error("create_shape_from_cooked: Jolt failed to restore cooked shape");
        return {};
    }
    return m.shapes.create({restored.Get(), info->static_only});
}

void PhysicsWorld::destroy(ShapeHandle shape) { impl_->shapes.destroy(shape); }
bool PhysicsWorld::valid(ShapeHandle shape) const noexcept { return impl_->shapes.get(shape) != nullptr; }

} // namespace astral
