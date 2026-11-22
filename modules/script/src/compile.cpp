#include <astral/script/compile.hpp>

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstdlib>

namespace astral {

namespace {
constexpr std::string_view magic = "ALBC";
}

bool is_compiled_script(std::string_view data) noexcept { return data.size() >= magic.size() && data.substr(0, magic.size()) == magic; }

bool compile_script(std::string_view name, std::string_view source, std::vector<std::uint8_t>& out, std::string* error) {
    std::size_t size = 0;
    char* bytecode = luau_compile(source.data(), source.size(), nullptr, &size);
    // A failed compile returns bytecode that holds the error: loading it reports "name:line: message".
    lua_State* L = luaL_newstate();
    const std::string chunk = "=" + std::string(name);
    const int rc = luau_load(L, chunk.c_str(), bytecode, size, 0);
    if (rc != 0) {
        if (error) *error = lua_tostring(L, -1) ? lua_tostring(L, -1) : std::string(name) + ": cannot compile";
    } else {
        out.assign(magic.begin(), magic.end());
        out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(bytecode), reinterpret_cast<const std::uint8_t*>(bytecode) + size);
    }
    lua_close(L);
    std::free(bytecode);
    return rc == 0;
}

} // namespace astral
