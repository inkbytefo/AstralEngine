#include "shader_cache_location.hpp"

#include <cstdlib>
#include <system_error>

namespace astral {

std::string process_env(std::string_view name) {
    const std::string key(name);
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    std::string out;
    if (_dupenv_s(&value, &length, key.c_str()) == 0 && value) out = value;
    std::free(value);
    return out;
#else
    const char* value = std::getenv(key.c_str());
    return value ? value : "";
#endif
}

namespace {

/// A folder name made of letters, digits, '-', '_' and '.', never "." or "..".
std::string folder_name(std::string_view title) {
    std::string out;
    for (const char c : title) {
        const bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
        out.push_back(plain ? c : '_');
    }
    const bool blank = out.find_first_not_of('_') == std::string::npos;
    return blank ? "default" : out;
}

std::filesystem::path user_cache_folder(const EnvLookup& env) {
    if (const std::string local = env("LOCALAPPDATA"); !local.empty()) return local;
    if (const std::string xdg = env("XDG_CACHE_HOME"); !xdg.empty()) return xdg;
    if (const std::string home = env("HOME"); !home.empty()) return std::filesystem::path(home) / ".cache";
    std::error_code ec;
    const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
    return ec ? std::filesystem::path(".") : temp;
}

} // namespace

std::filesystem::path shader_cache_directory(std::string_view title, const EnvLookup& env) {
    if (const std::string override_dir = env("ASTRAL_SHADER_CACHE"); !override_dir.empty()) {
        if (override_dir == "off" || override_dir == "0") return {};
        return override_dir;
    }
    return user_cache_folder(env) / "AstralEngine" / "shader-cache" / folder_name(title);
}

} // namespace astral
