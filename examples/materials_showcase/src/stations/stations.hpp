#pragma once
// The three stations of materials_showcase. Only one is loaded at a time because sun, environment, sky, post and the
// GI volume are global, so each station owns the whole picture while active.

#include <astral/astral.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace materials_showcase {

enum class StationId : std::uint8_t { Studio, Workflow, Gallery };
enum class UvInspectMode : std::uint8_t { Off, Checker, Density };

struct StationInfo {
    const char* name;  ///< Command line: --view <name>.
    const char* title; ///< HUD title.
    const char* hint;  ///< HUD one-line hint.
};

[[nodiscard]] const StationInfo& info(StationId id);
/// Unknown names fall back to studio.
[[nodiscard]] StationId station_from_name(std::string_view name);
/// Renderer settings that suit the station (GI volume, bloom, bounces).
[[nodiscard]] astral::RenderSettings station_settings(StationId id);
/// Asset path of the UV diagnostic material for `mode` (empty for Off).
[[nodiscard]] std::string_view uv_inspect_material(UvInspectMode mode) noexcept;

inline constexpr std::size_t studio_pbr_sphere_count = 7;
inline constexpr std::size_t studio_total_sphere_count = 8; // 7 M13 PBR spheres + car_paint.ashader exhibit
inline constexpr std::string_view studio_car_paint_material = "studio/materials/car_paint.material";
inline constexpr std::string_view gallery_sky_material = "gallery/materials/dusk_sky.material";
inline constexpr std::string_view gallery_post_material = "gallery/materials/vignette.material";

struct WorkflowPrimitiveRecord {
    astral::Entity entity{};
    std::string material_file;
};

struct WorkflowModelRecord {
    astral::Entity entity{};
    std::vector<astral::ModelMaterialOverride> overrides;
};

/// Saved workflow material bindings so pressing U can toggle UV inspection and restore cleanly.
struct WorkflowState {
    std::vector<WorkflowPrimitiveRecord> primitives;
    std::vector<WorkflowModelRecord> models;

    void clear() noexcept {
        primitives.clear();
        models.clear();
    }
};

/// Render resources created once and reused across station switches (MaterialService has no destroy).
class StationResources {
public:
    astral::MaterialHandle material(astral::Context& ctx, StationId station, const astral::MaterialDesc& desc);
    astral::MeshHandle smooth_sphere(astral::Context& ctx);
    astral::TextureHandle checker_texture(astral::Context& ctx);
    /// Call before building a station so materials are reused in order.
    void rewind() noexcept { next_ = 0; }

private:
    std::vector<astral::MaterialHandle> materials_[3];
    std::size_t next_ = 0;
    astral::MeshHandle sphere_{};
    astral::TextureHandle checker_{};
};

/// Builder context for one station. Every entity created goes into `owned` so switching removes all of it.
struct StationBuild {
    astral::Context& ctx;
    StationResources& resources;
    std::vector<astral::Entity>& owned;
    WorkflowState& workflow;
    bool raster = false;

    template <class... Cs> astral::Entity create(Cs&&... components) {
        const astral::Entity e = ctx.world.create(std::forward<Cs>(components)...);
        owned.push_back(e);
        return e;
    }
    astral::MaterialHandle material(StationId station, const astral::MaterialDesc& desc) {
        return resources.material(ctx, station, desc);
    }
    /// Spawns a fly camera at `eye` looking at `target`.
    astral::Entity camera(astral::Vec3 eye, astral::Vec3 target, float fov_degrees);
};

void build_studio(StationBuild& build);
void build_workflow(StationBuild& build);
void build_gallery(StationBuild& build);
void apply_workflow_uv_mode(astral::Context& ctx, const WorkflowState& state, UvInspectMode mode);

} // namespace materials_showcase
