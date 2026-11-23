#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace astral {

/// Extracts one glTF material and its source images into a new asset-relative folder.
/// Returns the asset-relative .material path. Existing folders are never overwritten.
std::optional<std::string> export_model_material(const std::filesystem::path& asset_root,
                                                 const std::filesystem::path& model_path,
                                                 std::size_t material_index,
                                                 const std::filesystem::path& output_folder,
                                                 std::string* error = nullptr);

} // namespace astral
