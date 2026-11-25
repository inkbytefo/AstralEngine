#pragma once
// Private helpers shared between bindings.cpp and bindings_services.cpp.

#include "host_impl.hpp"

#include <string>
#include <string_view>

namespace astral::bindings_detail {

using Impl = ScriptHost::Impl;

inline const TypeRegistry::Entry& entry_of(Impl& impl, std::uint32_t index) { return impl.env.types.entries()[index]; }

std::uint32_t require_type(lua_State* L, Impl& impl, std::string_view name);
Entity check_entity(lua_State* L, int index);
Quat check_quat(lua_State* L, int index);
void push_quat(lua_State* L, Quat q);
void set_function(lua_State* L, const char* name, lua_CFunction fn);
/// Raises a Lua error when called from a module's top-level code (ADR-0044): `what` changes the world.
void forbid_while_loading(lua_State* L, const char* what);

/// Registers save/load/has_save on the astral table at top of stack, plus astral.physics,
/// astral.fields, astral.quat, and astral.input sub-tables.
void register_service_bindings(lua_State* L);

} // namespace astral::bindings_detail
