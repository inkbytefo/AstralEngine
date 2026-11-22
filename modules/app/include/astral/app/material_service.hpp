#pragma once

#include <astral/render/mesh.hpp>
#include <astral/render/renderer.hpp>

#include <string_view>

namespace astral {

/// Render meshes, materials, and .material/.ashader loading bound to an App's Context.
class MaterialService {
public:
    virtual ~MaterialService() = default;
    MaterialService(const MaterialService&) = delete;
    MaterialService& operator=(const MaterialService&) = delete;

    /// Render resources. Safe in headless mode: they return an invalid handle (nothing is drawn).
    virtual MeshHandle create_mesh(const MeshData& data) = 0;
    virtual MaterialHandle create_material(const MaterialDesc& desc) = 0;
    /// False in headless mode or for a dead handle.
    virtual bool update_material(MaterialHandle material, const MaterialDesc& desc) = 0;
    /// A material from a .material file (ADR-0022): its shader (.ashader) and values, with the parent chain
    /// applied. Loaded on first use and cached; the handle stays valid while the file is edited (hot reload
    /// updates it in place; a version that does not compile is logged and the last good one stays). Invalid
    /// (logged) if it cannot be built, and in headless mode. Draw with MeshRenderer, or give a
    /// PrimitiveMesh a material_file.
    virtual MaterialHandle load_material(std::string_view path) = 0;
    /// Why the custom shaders could not be compiled into the ray-traced shaders ("" = fine).
    [[nodiscard]] virtual std::string_view shader_error() const noexcept = 0;

protected:
    MaterialService() = default;
};

} // namespace astral
