#include <astral/render/text.hpp>

#include <cstddef>

namespace astral::embedded {
extern const std::uint8_t roboto_medium_ttf[];
extern const std::size_t roboto_medium_ttf_size;
} // namespace astral::embedded

namespace astral {

std::span<const std::uint8_t> default_font_ttf() noexcept {
    return {embedded::roboto_medium_ttf, embedded::roboto_medium_ttf_size};
}

} // namespace astral
