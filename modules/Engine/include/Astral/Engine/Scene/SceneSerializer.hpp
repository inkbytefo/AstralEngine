#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace Astral {

class Scene;

// Scene files are JSON (".ascene"): human-readable, diff-friendly and easy to
// generate from tools. See docs/engine.md for the format.
std::string SerializeScene(const Scene& scene);
bool DeserializeScene(std::string_view json, Scene& scene, std::string& error);

bool SaveScene(const Scene& scene, const std::filesystem::path& path, std::string& error);
std::unique_ptr<Scene> LoadScene(const std::filesystem::path& path, std::string& error);

} // namespace Astral
