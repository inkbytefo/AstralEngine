// Regression check: <astral/assets/model.hpp> is self-contained and exposes complete model/image
// data without pulling in <astral/render/renderer.hpp> (Renderer remains incomplete).
#include <astral/assets/model.hpp>

#include <cstdint>
#include <type_traits>

namespace astral {
class Renderer;
}

namespace {

template <typename T, typename = void>
constexpr bool is_complete_v = false;

template <typename T>
constexpr bool is_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;

static_assert(is_complete_v<astral::ImageFormat>, "model.hpp must provide complete ImageFormat");
static_assert(is_complete_v<astral::ImageData>, "model.hpp must provide complete ImageData");
static_assert(is_complete_v<astral::MeshData>, "model.hpp must provide complete MeshData");
static_assert(is_complete_v<astral::ModelImage>, "model.hpp must provide complete ModelImage");
static_assert(is_complete_v<astral::ModelData>, "model.hpp must provide complete ModelData");
static_assert(!is_complete_v<astral::Renderer>, "model.hpp must not pull in <astral/render/renderer.hpp>");
static_assert(std::is_same_v<std::underlying_type_t<astral::ImageFormat>, std::uint8_t>);

} // namespace

#include <doctest/doctest.h>

TEST_CASE("model.hpp exposes complete model and image data without Renderer") {
    astral::ModelData model{};
    astral::ModelImage entry{};
    entry.image.width = 4;
    entry.image.height = 4;
    entry.image.format = astral::ImageFormat::RGBA8;
    entry.source = "test.png";
    model.images.push_back(entry);

    CHECK(model.images.size() == 1);
    CHECK(model.images[0].image.width == 4);
    CHECK(model.images[0].image.height == 4);
    CHECK(model.images[0].image.srgb);
    CHECK(model.images[0].image.format == astral::ImageFormat::RGBA8);
}
