#include <astral/assets/material_export.hpp>

#include <astral/assets/model.hpp>
#include <astral/core/json.hpp>

#include "file_util.hpp"
#include "imported_material.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace astral {
namespace {

bool relative_asset(const std::filesystem::path& path) {
    if (path.empty() || path.has_root_path()) return false;
    for (const auto& part : path) {
        if (part == ".." || part == ".") return false;
    }
    return true;
}

std::vector<std::uint8_t> tga(const ImageData& image) {
    if (image.width == 0 || image.height == 0 || image.width > 65535 || image.height > 65535 ||
        image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4) return {};
    std::vector<std::uint8_t> output(18 + image.rgba.size(), 0);
    output[2] = 2; // Uncompressed true-color
    output[12] = static_cast<std::uint8_t>(image.width);
    output[13] = static_cast<std::uint8_t>(image.width >> 8);
    output[14] = static_cast<std::uint8_t>(image.height);
    output[15] = static_cast<std::uint8_t>(image.height >> 8);
    output[16] = 32;
    output[17] = 0x28; // Top-left origin and 8 alpha bits
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        output[18 + i] = image.rgba[i + 2];
        output[19 + i] = image.rgba[i + 1];
        output[20 + i] = image.rgba[i];
        output[21 + i] = image.rgba[i + 3];
    }
    return output;
}

} // namespace

std::optional<std::string> export_model_material(const std::filesystem::path& asset_root,
                                                 const std::filesystem::path& model_path,
                                                 std::size_t material_index,
                                                 const std::filesystem::path& output_folder,
                                                 std::string* error) {
    const auto fail = [&](std::string message) -> std::optional<std::string> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    if (!relative_asset(model_path) || !relative_asset(output_folder)) return fail("expected relative asset paths");
    std::string load_error;
    const auto model = load_gltf(asset_root / model_path, &load_error);
    if (!model) return fail(load_error);
    if (material_index >= model->materials.size()) return fail("material slot index out of range");

    const auto output = asset_root / output_folder;
    std::error_code ec;
    std::filesystem::create_directories(output.parent_path(), ec);
    if (ec || !std::filesystem::create_directory(output, ec)) return fail("cannot create new folder: " + output.string());
    const auto rollback = [&](std::string message) {
        std::filesystem::remove_all(output, ec);
        return fail(std::move(message));
    };

    const ModelMaterial& material = model->materials[material_index];
    Json data = Json::object();
    data["format"] = "astral.material";
    data["version"] = 1;
    Json values = Json::object();
    if (detail::imported_surface_key(material)) {
        const auto shader_file = output_folder / "surface.ashader";
        const auto shader = detail::imported_surface_source(material);
        const auto shader_bytes = std::span(reinterpret_cast<const std::uint8_t*>(shader.data()), shader.size());
        if (!detail::write_file(asset_root / shader_file, shader_bytes, &load_error)) return rollback(load_error);
        data["shader"] = shader_file.generic_string();
        for (const auto& [name, value] : detail::imported_surface_values(material).values) values[name] = value;
    } else {
        data["type"] = "pbr";
        const auto c = material.base_color;
        const auto e = material.emissive;
        values["base_color"] = Json::array({c.x, c.y, c.z, c.w});
        values["emissive"] = Json::array({e.x, e.y, e.z});
        values["roughness"] = material.roughness;
        values["metallic"] = material.metallic;
        values["normal_scale"] = material.normal_scale;
    }
    const auto images = detail::imported_surface_images(material);
    for (const auto& [name, index] : images) {
        if (index < 0) continue;
        const auto pixels = tga(model->images[static_cast<std::size_t>(index)].image);
        const std::string filename = std::string(name) + ".tga";
        if (pixels.empty() || !detail::write_file(output / filename, pixels, &load_error)) {
            return rollback(load_error.empty() ? "cannot export " + filename : load_error);
        }
        values[name] = (output_folder / filename).generic_string();
    }
    data["values"] = std::move(values);
    const std::string text = data.dump(2);
    const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    if (!detail::write_file(output / "surface.material", bytes, &load_error)) return rollback(load_error);
    return (output_folder / "surface.material").generic_string();
}

} // namespace astral
