#pragma once
// Where things are on disk.

#include <filesystem>

namespace astral {

/// Folder of the running executable (shipped games keep their .apak next to it). Empty if unknown.
std::filesystem::path executable_directory();

} // namespace astral
