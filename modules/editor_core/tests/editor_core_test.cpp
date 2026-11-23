// GPU-free tests for Astral::EditorCore: command schema, transactions, stale revisions,
// partial-write rollback, byte-for-byte undo/redo, audit log, isolated ECS preview, and AI approval (ADR-0035).
#include <astral/editor/agent.hpp>
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
            return false;
        }
        ++writes;
        files[key_of(path)] = std::string(text);
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
