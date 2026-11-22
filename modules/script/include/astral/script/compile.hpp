#pragma once
// Luau source -> bytecode, for the cook (shipped games load bytecode: no source in the pack, syntax errors found at
// build time). A compiled script starts with the magic "ALBC"; ScriptHost::load accepts source and compiled alike.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// True if `data` is compiled by compile_script.
[[nodiscard]] bool is_compiled_script(std::string_view data) noexcept;

/// Compiles `source`. False with "name:line: message" in *error if it has a syntax error. `out` = "ALBC" + bytecode.
bool compile_script(std::string_view name, std::string_view source, std::vector<std::uint8_t>& out, std::string* error);

} // namespace astral
