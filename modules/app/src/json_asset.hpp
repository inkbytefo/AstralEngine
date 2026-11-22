#pragma once
// Private: a JSON asset as the cook ships it (binary) or as written (text).
#include <astral/core/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

inline std::optional<Json> parse_json_asset(const std::vector<std::uint8_t>& bytes, std::string* error) {
    if (Json::is_binary(bytes)) return Json::from_binary(bytes, error);
    return Json::parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), error);
}

} // namespace astral
