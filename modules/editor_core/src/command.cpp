#include <astral/editor/command.hpp>

#include <algorithm>

namespace astral::editor {

namespace {

bool validate_field_value(const TypeRegistry::Entry& entry, std::string_view field_name, const Json& value, std::string& error) {
    const FieldInfo* f = find_field(entry.fields, field_name);
    if (!f) {
        error = std::string(entry.name) + " has no reflected field \"" + std::string(field_name) + "\"";
        return false;
    }
    switch (f->kind) {
    case FieldKind::Bool:
        if (!value.is_bool()) return error = "field \"" + f->name + "\" expects bool", false;
        return true;
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float:
        if (!value.is_number()) return error = "field \"" + f->name + "\" expects number", false;
        return true;
    case FieldKind::String:
        if (!value.is_string()) return error = "field \"" + f->name + "\" expects string", false;
        return true;
    case FieldKind::Vec2:
        if (!value.is_array() || value.size() != 2) return error = "field \"" + f->name + "\" expects [x, y]", false;
        return true;
    case FieldKind::Vec3:
        if (!value.is_array() || value.size() != 3) return error = "field \"" + f->name + "\" expects [x, y, z]", false;
        return true;
    case FieldKind::Vec4:
    case FieldKind::Quat:
        if (!value.is_array() || value.size() != 4) return error = "field \"" + f->name + "\" expects 4 numbers", false;
        return true;
    case FieldKind::Enum:
        if (value.is_string()) {
            const bool ok = std::any_of(f->enumerators.begin(), f->enumerators.end(),
                                        [&](const auto& kv) { return kv.first == value.as_string(); });
            if (!ok) return error = "unknown enumerator \"" + value.as_string() + "\" for " + f->name, false;
            return true;
        }
        if (!value.is_number()) return error = "field \"" + f->name + "\" expects enum string or number", false;
        return true;
    case FieldKind::EntityRef:
    case FieldKind::Other:
        return true;
    }
    return true;
}

Json& ensure_entities_array(EditorDocument& doc) {
    const Json* list = doc.root.find("entities");
    if (!list || !list->is_array()) doc.root["entities"] = Json::array();
    return doc.root["entities"];
}

Json& ensure_object_member(Json& parent, std::string_view key) {
    const Json* child = parent.find(key);
    if (!child || !child->is_object()) parent[key] = Json::object();
    return parent[key];
}

/// The record id in a record's astral::Parent, or 0.
std::uint32_t parent_record(const Json& record) {
    const Json* comps = record.find("components");
    const Json* parent = comps && comps->is_object() ? comps->find("astral::Parent") : nullptr;
    return parent && parent->is_object() ? static_cast<std::uint32_t>((*parent)["entity"].as_number(0.0)) : 0u;
}

/// True when `ancestor` is `id` or one of its parents. Bounded by the record count, so a cycle already in a
/// hand-edited file cannot loop forever.
bool is_ancestor_or_self(const EditorDocument& doc, std::uint32_t ancestor, std::uint32_t id) {
    const Json* list = doc.root.find("entities");
    const std::size_t limit = list && list->is_array() ? list->size() + 1 : 1;
    for (std::size_t step = 0; step < limit && id != 0; ++step) {
        if (id == ancestor) return true;
        const Json* record = doc.find_record(id);
        if (!record) return false;
        id = parent_record(*record);
    }
    return id == ancestor && id != 0;
}

bool is_safe_relative_asset_path(std::string_view raw) {
    if (raw.empty() || raw.front() == '/' || raw.front() == '\\') return false;
    if (raw.find(':') != std::string_view::npos || raw.find('\0') != std::string_view::npos) return false;
    const std::filesystem::path p(raw);
    if (p.is_absolute()) return false;
    for (const auto& part : p) {
        if (part == "..") return false;
    }
    return true;
}

std::string apply_set_component_field(EditorDocument& doc, const EditorCommand& cmd) {
    const Json* existing = doc.find_record(cmd.target.record_id);
    if (!existing) return "target entity " + cmd.target.to_string() + " not found";
    Json rec = *existing;
    if (cmd.target.part_path.empty()) {
        Json& comps = ensure_object_member(rec, "components");
        Json& comp = ensure_object_member(comps, cmd.component);
        comp[cmd.field] = cmd.value;
    } else {
        Json& parts = ensure_object_member(rec, "parts");
        Json& part = ensure_object_member(parts, cmd.target.part_path);
        Json& comps = ensure_object_member(part, "components");
        Json& comp = ensure_object_member(comps, cmd.component);
        comp[cmd.field] = cmd.value;
    }
    doc.replace_record(cmd.target.record_id, std::move(rec));
    return {};
}

} // namespace

std::string_view command_kind_name(CommandKind kind) noexcept {
    switch (kind) {
    case CommandKind::CreateEntity: return "create_entity";
    case CommandKind::DeleteEntity: return "delete_entity";
    case CommandKind::AddComponent: return "add_component";
    case CommandKind::RemoveComponent: return "remove_component";
    case CommandKind::SetComponentField: return "set_component_field";
    case CommandKind::ReparentEntity: return "reparent_entity";
    case CommandKind::InstantiatePrefab: return "instantiate_prefab";
    case CommandKind::SetPrefabOverride: return "set_prefab_override";
    case CommandKind::SavePrefab: return "save_prefab";
    }
    return "unknown";
}

std::optional<CommandKind> command_kind_from_name(std::string_view name) noexcept {
    if (name == "create_entity") return CommandKind::CreateEntity;
    if (name == "delete_entity") return CommandKind::DeleteEntity;
    if (name == "add_component") return CommandKind::AddComponent;
    if (name == "remove_component") return CommandKind::RemoveComponent;
    if (name == "set_component_field") return CommandKind::SetComponentField;
    if (name == "reparent_entity") return CommandKind::ReparentEntity;
    if (name == "instantiate_prefab") return CommandKind::InstantiatePrefab;
    if (name == "set_prefab_override") return CommandKind::SetPrefabOverride;
    if (name == "save_prefab") return CommandKind::SavePrefab;
    return std::nullopt;
}

Json command_to_json(const EditorCommand& cmd) {
    Json out = Json::object();
    out["schema_version"] = static_cast<double>(cmd.schema_version);
    out["kind"] = command_kind_name(cmd.kind);
    if (cmd.target.valid()) out["target"] = cmd.target.to_string();
    if (cmd.parent.valid()) out["parent"] = cmd.parent.to_string();
    if (!cmd.component.empty()) out["component"] = cmd.component;
    if (!cmd.field.empty()) out["field"] = cmd.field;
    if (!cmd.asset_path.empty()) out["asset_path"] = cmd.asset_path;
    if (!cmd.value.is_null()) out["value"] = cmd.value;
    return out;
}

std::optional<EditorCommand> command_from_json(const Json& j, std::string* error) {
    if (!j.is_object()) {
        if (error) *error = "command must be a JSON object";
        return std::nullopt;
    }
    const auto kind = command_kind_from_name(j["kind"].as_string());
    if (!kind) {
        if (error) *error = "unknown command kind \"" + j["kind"].as_string() + "\"";
        return std::nullopt;
    }
    EditorCommand cmd;
    cmd.schema_version = static_cast<std::uint32_t>(j["schema_version"].as_number(static_cast<double>(kEditorCommandSchemaVersion)));
    cmd.kind = *kind;
    if (const Json* t = j.find("target"); t && t->is_string()) {
        const auto parsed = DocumentEntityId::parse(t->as_string());
        if (!parsed) {
            if (error) *error = "invalid target id \"" + t->as_string() + "\"";
            return std::nullopt;
        }
        cmd.target = *parsed;
    }
    if (const Json* p = j.find("parent"); p && p->is_string()) {
        const auto parsed = DocumentEntityId::parse(p->as_string());
        if (!parsed) {
            if (error) *error = "invalid parent id \"" + p->as_string() + "\"";
            return std::nullopt;
        }
        cmd.parent = *parsed;
    }
    cmd.component = j["component"].as_string();
    cmd.field = j["field"].as_string();
    cmd.asset_path = j["asset_path"].as_string();
    if (const Json* v = j.find("value")) cmd.value = *v;
    return cmd;
}

Json build_command_schema(const TypeRegistry& types) {
    Json doc = Json::object();
    doc["format"] = "astral.editor.command_schema";
    doc["schema_version"] = static_cast<double>(kEditorCommandSchemaVersion);
    Json kinds = Json::array();
    for (CommandKind k : {CommandKind::CreateEntity, CommandKind::DeleteEntity, CommandKind::AddComponent,
                          CommandKind::RemoveComponent, CommandKind::SetComponentField, CommandKind::ReparentEntity,
                          CommandKind::InstantiatePrefab, CommandKind::SetPrefabOverride, CommandKind::SavePrefab}) {
        kinds.push_back(command_kind_name(k));
    }
    doc["commands"] = std::move(kinds);
    Json comps = Json::object();
    for (const TypeRegistry::Entry& entry : types.entries()) {
        Json c = Json::object();
        c["schema_version"] = static_cast<double>(entry.schema_version);
        Json fields = Json::object();
        for (const FieldInfo& f : entry.fields) {
            Json fd = Json::object();
            fd["kind"] = field_kind_name(f.kind);
            if (!f.enumerators.empty()) {
                Json enums = Json::array();
                for (const auto& [name, _] : f.enumerators) enums.push_back(name);
                fd["enum"] = std::move(enums);
            }
            fields[f.name] = std::move(fd);
        }
        c["fields"] = std::move(fields);
        comps[entry.name] = std::move(c);
    }
    doc["components"] = std::move(comps);
    return doc;
}

std::string validate_command(const EditorDocument& doc, const TypeRegistry& types, const EditorCommand& cmd) {
    if (cmd.schema_version != kEditorCommandSchemaVersion) {
        return "unsupported command schema_version " + std::to_string(cmd.schema_version);
    }
    switch (cmd.kind) {
    case CommandKind::CreateEntity:
        if (cmd.target.valid() && doc.find_record(cmd.target.record_id)) return "record id already exists";
        if (!cmd.value.is_null() && !cmd.value.is_object()) return "create_entity value must be a components object";
        return {};
    case CommandKind::DeleteEntity:
        if (!cmd.target.valid() || !doc.find_record(cmd.target.record_id)) return "delete_entity target does not exist";
        return {};
    case CommandKind::AddComponent:
    case CommandKind::RemoveComponent: {
        if (!cmd.target.valid() || !doc.find_record(cmd.target.record_id)) return "target entity does not exist";
        const TypeRegistry::Entry* entry = types.find(cmd.component);
        if (!entry) return "unknown component \"" + cmd.component + "\"";
        if (cmd.kind == CommandKind::AddComponent && !cmd.value.is_null() && !cmd.value.is_object()) {
            return "add_component value must be a JSON object";
        }
        return {};
    }
    case CommandKind::SetComponentField:
    case CommandKind::SetPrefabOverride: {
        if (!cmd.target.valid() || !doc.find_record(cmd.target.record_id)) return "target entity does not exist";
        const TypeRegistry::Entry* entry = types.find(cmd.component);
        if (!entry) return "unknown component \"" + cmd.component + "\"";
        std::string err;
        if (!validate_field_value(*entry, cmd.field, cmd.value, err)) return err;
        return {};
    }
    case CommandKind::ReparentEntity:
        if (!cmd.target.valid() || !doc.find_record(cmd.target.record_id)) return "reparent target does not exist";
        if (cmd.parent.valid() && !doc.find_record(cmd.parent.record_id)) return "reparent parent does not exist";
        if (cmd.parent.valid() && cmd.parent == cmd.target) return "cannot reparent entity to itself";
        if (cmd.parent.valid() && is_ancestor_or_self(doc, cmd.target.record_id, cmd.parent.record_id)) {
            return "cannot reparent an entity under its own descendant (cycle)";
        }
        return {};
    case CommandKind::InstantiatePrefab:
        if (!is_safe_relative_asset_path(cmd.asset_path)) return "instantiate_prefab requires a safe relative asset_path";
        if (cmd.target.valid() && doc.find_record(cmd.target.record_id)) return "record id already exists";
        return {};
    case CommandKind::SavePrefab:
        if (!is_safe_relative_asset_path(cmd.asset_path)) return "save_prefab requires a safe relative asset_path";
        if (!cmd.value.is_object() || cmd.value["format"].as_string() != "astral.scene") {
            return "save_prefab value must be an astral.scene prefab document";
        }
        return {};
    }
    return "unknown command";
}

std::string apply_command(EditorDocument& doc, const TypeRegistry& types, const EditorCommand& cmd,
                          IEditorFileSystem* fs, const std::filesystem::path& asset_root) {
    if (std::string err = validate_command(doc, types, cmd); !err.empty()) return err;
    switch (cmd.kind) {
    case CommandKind::CreateEntity: {
        Json& list = ensure_entities_array(doc);
        Json rec = Json::object();
        rec["id"] = static_cast<double>(cmd.target.valid() ? cmd.target.record_id : doc.next_record_id());
        rec["components"] = cmd.value.is_object() ? cmd.value : Json::object();
        list.push_back(std::move(rec));
        return {};
    }
    case CommandKind::DeleteEntity: {
        Json& list = ensure_entities_array(doc);
        Json next = Json::array();
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (static_cast<std::uint32_t>(list[i]["id"].as_number(0.0)) == cmd.target.record_id) continue;
            Json record = list[i];
            // Its children become roots instead of pointing at a record that is gone (ADR-0053).
            if (parent_record(record) == cmd.target.record_id) record["components"].erase("astral::Parent");
            next.push_back(std::move(record));
        }
        list = std::move(next);
        return {};
    }
    case CommandKind::AddComponent: {
        Json rec = *doc.find_record(cmd.target.record_id);
        Json& comps = ensure_object_member(rec, "components");
        comps[cmd.component] = cmd.value.is_object() ? cmd.value : Json::object();
        doc.replace_record(cmd.target.record_id, std::move(rec));
        return {};
    }
    case CommandKind::RemoveComponent: {
        Json rec = *doc.find_record(cmd.target.record_id);
        if (const Json* comps = rec.find("components"); comps && comps->is_object()) {
            rec["components"].erase(cmd.component);
            doc.replace_record(cmd.target.record_id, std::move(rec));
        }
        return {};
    }
    case CommandKind::SetComponentField:
    case CommandKind::SetPrefabOverride:
        return apply_set_component_field(doc, cmd);
    case CommandKind::ReparentEntity: {
        Json rec = *doc.find_record(cmd.target.record_id);
        Json& comps = ensure_object_member(rec, "components");
        if (cmd.parent.valid()) {
            Json p = Json::object();
            p["entity"] = static_cast<double>(cmd.parent.record_id);
            comps["astral::Parent"] = std::move(p);
        } else {
            comps.erase("astral::Parent");
        }
        doc.replace_record(cmd.target.record_id, std::move(rec));
        return {};
    }
    case CommandKind::InstantiatePrefab: {
        Json& list = ensure_entities_array(doc);
        Json rec = Json::object();
        rec["id"] = static_cast<double>(cmd.target.valid() ? cmd.target.record_id : doc.next_record_id());
        rec["prefab"] = cmd.asset_path;
        if (cmd.value.is_object() && cmd.value.size() > 0) rec["components"] = cmd.value;
        list.push_back(std::move(rec));
        return {};
    }
    case CommandKind::SavePrefab: {
        if (!fs) return "save_prefab requires an IEditorFileSystem";
        std::string io_err;
        const std::filesystem::path target = asset_root.empty() ? std::filesystem::path(cmd.asset_path) : asset_root / cmd.asset_path;
        if (!fs->write_atomic(target, serialize_document(cmd.value), &io_err)) return io_err;
        return {};
    }
    }
    return "unknown command";
}

} // namespace astral::editor
