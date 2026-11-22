#include "image_io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <stb_image.h>
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace astral::golden {

std::optional<Image> load_image(const char* path) {
    int w = 0;
    int h = 0;
    int n = 0;
    unsigned char* data = stbi_load(path, &w, &h, &n, 3);
    if (!data) return std::nullopt;
    Image img{static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), {}};
    img.rgb.assign(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
    stbi_image_free(data);
    return img;
}

bool save_png(const char* path, const Image& img) {
    return stbi_write_png(path, static_cast<int>(img.width), static_cast<int>(img.height), 3, img.rgb.data(),
                          static_cast<int>(img.width * 3)) != 0;
}

} // namespace astral::golden
