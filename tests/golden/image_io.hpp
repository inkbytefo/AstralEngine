#pragma once

// RGB8 image load/save helpers for golden image tools and tests.
// Test infrastructure only (not part of the engine's public API).

#include "image_compare.hpp"

#include <optional>

namespace astral::golden {

[[nodiscard]] std::optional<Image> load_image(const char* path);
[[nodiscard]] bool save_png(const char* path, const Image& img);

} // namespace astral::golden
