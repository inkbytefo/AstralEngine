#pragma once
// Text for the 2D overlay.

#include <cstdint>
#include <span>

namespace astral {

/// The engine's built-in font (Roboto Medium, Apache License 2.0), as TrueType bytes. Always available:
/// the debug UI and HUD text use it when a game brings no font of its own.
std::span<const std::uint8_t> default_font_ttf() noexcept;

} // namespace astral
