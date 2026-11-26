// GPU-free tests for Astral::EditorCore: command schema, transactions, stale revisions,
// partial-write rollback, byte-for-byte undo/redo, audit log, isolated ECS preview, and AI approval (ADR-0035).
#include <astral/editor/agent.hpp>
#include <astral/editor/agent_task.hpp>
#include <astral/editor/command.hpp>
#include <astral/editor/document.hpp>
#include <astral/editor/transaction.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/serialize.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace astral;
using namespace astral::editor;

namespace {

/// In-memory filesystem with fault injection for deterministic partial-write / I/O failure tests.
class FaultyMemoryFileSystem final : public IEditorFileSystem {
public:
    std::map<std::string, std::string> files;
    int fail_after_writes = -1; // >= 0: fail on that write count
    bool fail_once = false;     // true: only that one write fails (a transient error), later writes succeed
    int writes = 0;

    static std::string key_of(const std::filesystem::path& p) { return p.generic_string(); }

    std::optional<std::string> read_text(const std::filesystem::path& path, std::string* error = nullptr) const override {
        auto it = files.find(key_of(path));
        if (it == files.end()) {
            if (error) *error = "not found: " + path.string();
            return std::nullopt;
        }
        return it->second;
    }

    bool write_atomic(const std::filesystem::path& path, std::string_view text, std::string* error = nullptr) override {
        if (fail_after_writes >= 0 && writes >= fail_after_writes) {
            if (error) *error = "simulated disk full / partial write failure";
            if (fail_once) fail_after_writes = -1;
            return false;
        }
        ++writes;
        files[key_of(path)] = std::string(text);
        return true;
    }

    bool remove(const std::filesystem::path& path, std::string* = nullptr) override {
        files.erase(key_of(path));
        return true;
    }

    std::vector<std::filesystem::path> list_files(const std::filesystem::path&,
                                                  const std::vector<std::string>&) const override {
        std::vector<std::filesystem::path> out;
        for (const auto& [k, _] : files) out.emplace_back(k);
        return out;
    }
};

struct Lamp {
    float intensity = 10.0f;
    bool enabled = true;
};

constexpr std::string_view kInitialScene = R"({
  "format": "astral.scene",
  "version": 1,
  "entities": [
    {
      "id": 1,
      "components": {
        "astral::Name": {
          "value": "Floor"
        },
        "astral::Transform": {
          "position": [0, 0, 0]
        }
      }
    }
  ]
}
)";

TypeRegistry make_types() {
    TypeRegistry types;
    register_world_types(types);
    types.add<Lamp>();
    return types;
}

} // namespace

ASTRAL_REFLECT(Lamp, intensity, enabled)

TEST_CASE("command schema is discoverable and validates field types and JSON round-trip") {
    const TypeRegistry types = make_types();
    const Json schema = build_command_schema(types);
    CHECK(schema["format"].as_string() == "astral.editor.command_schema");
    CHECK(schema["schema_version"].as_number() == 1.0);
    CHECK(schema["commands"].size() == 9);
    CHECK(schema["components"].find("Lamp") != nullptr);
    CHECK(schema["components"]["Lamp"]["fields"]["intensity"]["kind"].as_string() == "f32");

    EditorCommand cmd;
    cmd.kind = CommandKind::SetComponentField;
    cmd.target = {1, ""};
    cmd.component = "Lamp";
    cmd.field = "intensity";
    cmd.value = 42.5;
    const Json serialized = command_to_json(cmd);
    std::string err;
    const auto parsed = command_from_json(serialized, &err);
    REQUIRE(parsed.has_value());
    CHECK(parsed->kind == cmd.kind);
    CHECK(parsed->target == cmd.target);
    CHECK(parsed->component == "Lamp");
    CHECK(parsed->field == "intensity");
    CHECK(parsed->value.as_number() == doctest::Approx(42.5));
}

TEST_CASE("stale revision is rejected and partial disk write failure rolls back cleanly") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/level.scene"] = std::string(kInitialScene);
    const TypeRegistry types = make_types();

    EditorSession session(fs, types, "");
    REQUIRE(session.open("scenes/level.scene"));
    const DocumentRevision rev1 = session.document().revision;
    const std::string original_disk = fs.files["scenes/level.scene"];

    // Apply a valid command -> moves to rev2.
    Transaction tx1;
    tx1.summary = "Add lamp";
    tx1.expected_revision = rev1;
    tx1.commands.push_back({.kind = CommandKind::AddComponent, .target = {1, ""}, .component = "Lamp", .value = Json::object()});
    const TransactionResult r1 = session.apply(tx1);
    REQUIRE(r1.ok);
    CHECK(session.document().revision.number == 2);

    // Stale revision (still using rev1) must be rejected without modifying document or disk.
    Transaction stale;
    stale.summary = "Stale edit";
    stale.expected_revision = rev1;
    stale.commands.push_back({.kind = CommandKind::SetComponentField, .target = {1, ""}, .component = "Lamp", .field = "intensity", .value = 99.0});
    const TransactionResult stale_res = session.apply(stale);
    CHECK_FALSE(stale_res.ok);
    CHECK(stale_res.stale_revision);
    CHECK(session.document().revision.number == 2);

    // Simulate I/O failure on next write -> must roll back both memory and disk.
    const std::string disk_at_rev2 = fs.files["scenes/level.scene"];
    fs.fail_after_writes = fs.writes;
    Transaction failing;
    failing.summary = "Will fail on write";
    failing.expected_revision = session.document().revision;
    failing.commands.push_back({.kind = CommandKind::SetComponentField, .target = {1, ""}, .component = "Lamp", .field = "intensity", .value = 500.0});
    const TransactionResult fail_res = session.apply(failing);
    CHECK_FALSE(fail_res.ok);
    CHECK(fail_res.rolled_back);
    CHECK(session.document().revision.number == 2);
    CHECK(fs.files["scenes/level.scene"] == disk_at_rev2);
    CHECK_FALSE(session.audit_log().to_jsonl().empty());
    (void)original_disk;
}

TEST_CASE("AI prefab proposal previews in isolated ECS world, approves atomically, reloads, and undo/redo is byte-identical") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/workshop.scene"] = std::string(kInitialScene);
    const TypeRegistry types = make_types();

    EditorSession session(fs, types, "");
    REQUIRE(session.open("scenes/workshop.scene"));
    const std::string initial_canonical = serialize_document(session.document().root);

    PrefabAssistantAgent agent;
    ProposalReview review(session);

    // 1. Request proposal and inspect diff + isolated ECS preview.
    std::string err;
    REQUIRE(review.request(agent, {}, "Add a reusable AI crate prefab", &err));
    REQUIRE(review.has_active());
    REQUIRE(review.preview() != nullptr);
    CHECK(review.preview()->valid);
    CHECK_FALSE(review.preview()->diff.empty());
    CHECK(review.preview()->entity_count == 2); // Floor + AI Crate instance in temporary ECS world

    // Main document and disk are completely untouched before approval!
    CHECK(session.document().root["entities"].size() == 1);
    CHECK(fs.files.find("prefabs/ai_crate.prefab") == fs.files.end());

    // 2. Reject first -> verify zero mutation, then request again and approve.
    review.reject("trying reject path");
    CHECK_FALSE(review.has_active());
    CHECK(session.document().root["entities"].size() == 1);

    REQUIRE(review.request(agent, {}, "Add a reusable AI crate prefab", &err));
    const TransactionResult approved = review.approve();
    REQUIRE(approved.ok);
    CHECK_FALSE(review.has_active());
    CHECK(fs.files.find("prefabs/ai_crate.prefab") != fs.files.end());
    CHECK(session.document().root["entities"].size() == 2);
    const std::string after_approve_bytes = fs.files["scenes/workshop.scene"];

    // 3. Undo and Redo produce byte-for-byte identical file content.
    const TransactionResult undone = session.undo();
    REQUIRE(undone.ok);
    CHECK(fs.files["scenes/workshop.scene"] == initial_canonical);

    const TransactionResult redone = session.redo();
    REQUIRE(redone.ok);
    CHECK(fs.files["scenes/workshop.scene"] == after_approve_bytes);

    // 4. Reload from disk matches byte-for-byte and audit log recorded every step.
    REQUIRE(session.reload(&err));
    CHECK(serialize_document(session.document().root) == after_approve_bytes);
    CHECK(session.audit_log().entries().size() >= 4); // reject, apply, undo, redo
}

TEST_CASE("ManualSave supports dirty state, drag coalescing, external conflict detection, autosave, and path safety") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/level.scene"] = std::string(kInitialScene);
    const TypeRegistry types = make_types();

    EditorSession session(fs, types, "");
    session.set_persist_policy(PersistPolicy::ManualSave);
    REQUIRE(session.open("scenes/level.scene"));
    CHECK_FALSE(session.is_dirty());
    const std::string original_disk = fs.files["scenes/level.scene"];

    // Path traversal must be rejected.
    EditorCommand bad_path;
    bad_path.kind = CommandKind::SavePrefab;
    bad_path.asset_path = "../outside.prefab";
    bad_path.value = Json::parse(kInitialScene).value();
    CHECK_FALSE(validate_command(session.document(), types, bad_path).empty());

    // 3 consecutive coalesced drag edits produce ONE undo step and do not touch disk yet.
    for (int i = 1; i <= 3; ++i) {
        Transaction drag;
        drag.summary = "Move Floor";
        drag.expected_revision = session.document().revision;
        drag.coalesce_key = "drag:1:pos";
        Json pos = Json::array();
        pos.push_back(static_cast<double>(i));
        pos.push_back(0.0);
        pos.push_back(0.0);
        drag.commands.push_back({.kind = CommandKind::SetComponentField, .target = {1, ""},
                                 .component = "astral::Transform", .field = "position", .value = std::move(pos)});
        REQUIRE(session.apply(drag).ok);
    }
    CHECK(session.is_dirty());
    CHECK(fs.files["scenes/level.scene"] == original_disk); // untouched until Ctrl+S

    // Autosave sidecar + recovery.
    std::string err;
    REQUIRE(session.write_autosave(&err));
    CHECK(session.has_autosave());

    // Single undo reverts the entire coalesced drag back to clean state.
    REQUIRE(session.undo().ok);
    CHECK_FALSE(session.can_undo());
    CHECK_FALSE(session.is_dirty());
    REQUIRE(session.redo().ok);
    CHECK(session.is_dirty());

    // Simulate external disk edit -> save() refuses unless force_overwrite=true.
    fs.files["scenes/level.scene"] += " ";
    CHECK(session.has_external_conflict());
    CHECK_FALSE(session.save(&err, false));
    REQUIRE(session.save(&err, true));
    CHECK_FALSE(session.is_dirty());
    CHECK_FALSE(session.has_external_conflict());
}

TEST_CASE("AgentTaskManager enforces policy gate, async review, selective approval, revision, and Codex JSON parsing") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/workshop.scene"] = std::string(kInitialScene);
    const TypeRegistry types = make_types();

    EditorSession session(fs, types, "");
    session.set_persist_policy(PersistPolicy::ManualSave);
    REQUIRE(session.open("scenes/workshop.scene"));

    AgentTaskManager mgr(session, {.allow_read_and_preview = true, .require_approval_for_edits = true,
                                   .allow_external_commands = false, .allow_network = false});
    CodexCliProvider codex("nonexistent_codex_binary_for_test");
    std::string err;
    // External CLI must be blocked by policy before spawning anything.
    CHECK_FALSE(mgr.submit(codex, {}, "Add a lamp", false, &err));
    CHECK(mgr.state() == AgentTaskState::Blocked);

    // Offline built-in provider async -> poll -> Review -> selective approval (approve only SavePrefab or all).
    PrefabAssistantAgent builtin;
    REQUIRE(mgr.submit(builtin, {}, "Create crate prefab", true, &err));
    while (mgr.busy()) mgr.poll();
    REQUIRE(mgr.state() == AgentTaskState::Review);
    REQUIRE(mgr.current() != nullptr);
    REQUIRE(mgr.preview() != nullptr);
    CHECK(mgr.preview()->valid);
    CHECK(mgr.current()->command_selection.size() == 2);

    // Revision flow keeps task history.
    REQUIRE(mgr.request_revision(builtin, "Place it higher", false, &err));
    CHECK(mgr.state() == AgentTaskState::Review);
    CHECK(mgr.history().size() >= 2);

    const TransactionResult res = mgr.approve_selected();
    REQUIRE(res.ok);
    CHECK(mgr.state() == AgentTaskState::Complete);
    CHECK(session.is_dirty());

    // Codex response parser handles markdown-fenced JSON and rejects malformed output.
    AgentContext ctx;
    ctx.base_revision = session.document().revision;
    const std::string fenced = "Here is the proposal:\n```json\n{\"summary\":\"Rename floor\",\"commands\":[{\"kind\":\"set_component_field\",\"target\":\"1\",\"component\":\"astral::Name\",\"field\":\"value\",\"value\":\"Stone Floor\"}]}\n```";
    const auto parsed = CodexCliProvider::parse_response(fenced, ctx, "codex_cli", &err);
    REQUIRE(parsed.has_value());
    CHECK(parsed->summary == "Rename floor");
    CHECK(parsed->commands.size() == 1);
    CHECK_FALSE(CodexCliProvider::parse_response("not json at all", ctx, "codex_cli", &err).has_value());
}

TEST_CASE("a transaction that fails to save removes the prefab files it created") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/level.scene"] = std::string(kInitialScene);
    fs.files["prefabs/old.prefab"] = "old";
    const TypeRegistry types = make_types();
    EditorSession session(fs, types, "");
    REQUIRE(session.open("scenes/level.scene"));
    const std::string disk_before = fs.files["scenes/level.scene"];

    Json prefab = Json::object();
    prefab["format"] = "astral.scene";
    prefab["version"] = 1.0;
    prefab["entities"] = Json::array();
    Transaction tx;
    tx.summary = "Two prefabs, then the scene write fails";
    tx.expected_revision = session.document().revision;
    tx.commands.push_back({.kind = CommandKind::SavePrefab, .asset_path = "prefabs/new.prefab", .value = prefab});
    tx.commands.push_back({.kind = CommandKind::SavePrefab, .asset_path = "prefabs/old.prefab", .value = prefab});
    tx.commands.push_back({.kind = CommandKind::AddComponent, .target = {1, ""}, .component = "Lamp", .value = Json::object()});
    fs.fail_after_writes = fs.writes + 2; // both prefabs are written, the scene is not
    fs.fail_once = true;                  // the rollback's own writes succeed
    const TransactionResult res = session.apply(tx);
    CHECK_FALSE(res.ok);
    CHECK(res.rolled_back);
    CHECK(fs.files.count("prefabs/new.prefab") == 0); // created by the transaction: gone
    CHECK(fs.files["prefabs/old.prefab"] == "old");   // existed before: restored
    CHECK(fs.files["scenes/level.scene"] == disk_before);
}

namespace {
Transaction one(EditorSession& session, EditorCommand cmd) {
    Transaction tx;
    tx.summary = "test";
    tx.expected_revision = session.document().revision;
    tx.commands.push_back(std::move(cmd));
    return tx;
}
Json named(const char* name) {
    Json c = Json::object();
    Json n = Json::object();
    n["value"] = name;
    c["astral::Name"] = std::move(n);
    return c;
}
std::optional<double> parent_of(const EditorSession& session, std::uint32_t id) {
    const Json* rec = session.document().find_record(id);
    if (!rec) return std::nullopt;
    const Json* comps = rec->find("components");
    const Json* parent = comps ? comps->find("astral::Parent") : nullptr;
    if (!parent) return std::nullopt;
    return (*parent)["entity"].as_number();
}
} // namespace

TEST_CASE("reparenting refuses indirect cycles; deleting a parent moves its children to the root") {
    FaultyMemoryFileSystem fs;
    fs.files["scenes/level.scene"] = std::string(kInitialScene);
    const TypeRegistry types = make_types();
    EditorSession session(fs, types, "");
    REQUIRE(session.open("scenes/level.scene"));
    // 1 Floor <- 2 Table <- 3 Cup
    REQUIRE(session.apply(one(session, {.kind = CommandKind::CreateEntity, .target = {2, ""}, .value = named("Table")})).ok);
    REQUIRE(session.apply(one(session, {.kind = CommandKind::CreateEntity, .target = {3, ""}, .value = named("Cup")})).ok);
    REQUIRE(session.apply(one(session, {.kind = CommandKind::ReparentEntity, .target = {2, ""}, .parent = {1, ""}})).ok);
    REQUIRE(session.apply(one(session, {.kind = CommandKind::ReparentEntity, .target = {3, ""}, .parent = {2, ""}})).ok);

    const TransactionResult cycle = session.apply(one(session, {.kind = CommandKind::ReparentEntity, .target = {1, ""}, .parent = {3, ""}}));
    CHECK_FALSE(cycle.ok);
    CHECK(cycle.error.find("cycle") != std::string::npos);
    CHECK_FALSE(parent_of(session, 1).has_value());

    REQUIRE(session.apply(one(session, {.kind = CommandKind::DeleteEntity, .target = {2, ""}})).ok);
    CHECK(session.document().find_record(2) == nullptr);
    REQUIRE(session.document().find_record(3) != nullptr);
    CHECK_FALSE(parent_of(session, 3).has_value()); // no dangling parent: Cup is a root now
}
