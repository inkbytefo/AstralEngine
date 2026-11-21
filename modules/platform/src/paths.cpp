#include <astral/platform/paths.hpp>

#include <SDL3/SDL_filesystem.h>

namespace astral {

std::filesystem::path executable_directory() {
    const char* base = SDL_GetBasePath(); // owned and cached by SDL; works before SDL_Init
    return base ? std::filesystem::path(base) : std::filesystem::path();
}

} // namespace astral
