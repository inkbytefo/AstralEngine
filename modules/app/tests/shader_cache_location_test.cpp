#include "shader_cache_location.hpp"

#include <doctest/doctest.h>

#include <map>

using namespace astral;
namespace fs = std::filesystem;

namespace {
EnvLookup env_of(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](std::string_view name) {
        const auto it = vars.find(std::string(name));
        return it == vars.end() ? std::string{} : it->second;
    };
}
} // namespace

TEST_CASE("the shader cache folder follows the environment override") {
    CHECK(shader_cache_directory("Game", env_of({{"ASTRAL_SHADER_CACHE", "off"}})).empty());
    CHECK(shader_cache_directory("Game", env_of({{"ASTRAL_SHADER_CACHE", "0"}})).empty());
    CHECK(shader_cache_directory("Game", env_of({{"ASTRAL_SHADER_CACHE", "D:/shared/spv"}})) == fs::path("D:/shared/spv"));
}

TEST_CASE("the default shader cache folder is per user and per game") {
    const fs::path windows = shader_cache_directory("Sword Arena", env_of({{"LOCALAPPDATA", "C:/Users/me/AppData/Local"}, {"XDG_CACHE_HOME", "/x"}}));
    const fs::path xdg = shader_cache_directory("Sword Arena", env_of({{"XDG_CACHE_HOME", "/home/me/.cache"}}));
    const fs::path home = shader_cache_directory("Sword Arena", env_of({{"HOME", "/home/other"}}));
    CHECK(windows == fs::path("C:/Users/me/AppData/Local") / "AstralEngine" / "shader-cache" / "Sword_Arena");
    CHECK(xdg.string().find("Sword_Arena") != std::string::npos);
    CHECK(home.string().find("Sword_Arena") != std::string::npos);
    CHECK(xdg != home);
    CHECK(shader_cache_directory("A", env_of({{"HOME", "/h"}})) != shader_cache_directory("B", env_of({{"HOME", "/h"}})));
}

TEST_CASE("the game title cannot escape the cache folder") {
    const fs::path base = shader_cache_directory("x", env_of({{"XDG_CACHE_HOME", "/c"}})).parent_path();
    for (const char* title : {"../../etc", "a/b\\c:d", "", "..", "  "}) {
        CAPTURE(title);
        const fs::path dir = shader_cache_directory(title, env_of({{"XDG_CACHE_HOME", "/c"}}));
        CHECK(dir.parent_path() == base);
        CHECK_FALSE(dir.filename().empty());
        CHECK(dir.filename() != "..");
        CHECK(dir.filename() != ".");
    }
}

TEST_CASE("without any user folder the cache falls back to the temp folder") {
    const fs::path dir = shader_cache_directory("Game", env_of({}));
    CHECK_FALSE(dir.empty());
    CHECK(dir.filename() == "Game");
}
