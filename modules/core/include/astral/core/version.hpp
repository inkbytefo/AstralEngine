#pragma once

#include <string_view>

namespace astral {

/// Engine version as "MAJOR.MINOR.PATCH".
std::string_view version() noexcept;

} // namespace astral
