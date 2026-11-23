#include <astral/editor/agent.hpp>

#include <algorithm>
#include <cctype>

namespace astral::editor {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

Json make_prop_prefab(std::string_view name, Vec3 color, bool include_primitive_mesh) {
    Json root = Json::object();
    root["format"] = "astral.scene";
    root["version"] = 1.0;

    Json ent = Json::object();
    ent["id"] = 1.0;
    Json comps = Json::object();
    Json n = Json::object();
    n["value"] = std::string(name);
    comps["astral::Name"] = std::move(n);

    Json tf = Json::object();
    Json pos = Json::array();
    pos.push_back(0.0);
    pos.push_back(0.5);
    pos.push_back(0.0);
    tf["position"] = std::move(pos);
    comps["astral::Transform"] = std::move(tf);

    if (include_primitive_mesh) {
        Json mesh = Json::object();
        mesh["shape"] = "Cube";
        Json mat = Json::object();
        Json col = Json::array();
        col.push_back(static_cast<double>(color.x));
        col.push_back(static_cast<double>(color.y));
        col.push_back(static_cast<double>(color.z));
        col.push_back(1.0);
        mat["base_color"] = std::move(col);
        mesh["material"] = std::move(mat);
        comps["astral::PrimitiveMesh"] = std::move(mesh);
    }

    ent["components"] = std::move(comps);
    Json ents = Json::array();
    ents.push_back(std::move(ent));
    root["entities"] = std::move(ents);
    return root;
}

} // namespace

std::optional<AgentProposal> PrefabAssistantAgent::propose(const AgentContext& ctx, std::string* error) {
    if (!ctx.base_revision.valid()) {
        if (error) *error = "agent requires a valid open document revision";
        return std::nullopt;
    }
    AgentProposal proposal;
    proposal.proposal_id = "prop-" + std::to_string(next_id_++);
    proposal.agent_id = std::string(id());
    proposal.base_revision = ctx.base_revision;

    const std::string lower = to_lower(ctx.prompt);
    if (ctx.selected_entity.valid() && (lower.find("move") != std::string::npos || lower.find("lift") != std::string::npos ||
                                        lower.find("raise") != std::string::npos)) {
        proposal.summary = "Raise selected entity " + ctx.selected_entity.to_string() + " by +1m";
        EditorCommand cmd;
        cmd.kind = CommandKind::SetComponentField;
        cmd.target = ctx.selected_entity;
        cmd.component = "astral::Transform";
        cmd.field = "position";
        Json pos = Json::array();
        pos.push_back(0.0);
        pos.push_back(2.0);
        pos.push_back(0.0);
        cmd.value = std::move(pos);
        proposal.commands.push_back(std::move(cmd));
        return proposal;
    }

    // Default prefab authoring proposal: save a new reusable prefab asset and place an instance with transform override.
    const std::string prefab_path = "prefabs/ai_crate.prefab";
    proposal.summary = "Create prefab \"" + prefab_path + "\" and place an instance in the scene";

    const bool has_primitive_mesh = ctx.command_schema["components"].find("astral::PrimitiveMesh") != nullptr;
    EditorCommand save_cmd;
    save_cmd.kind = CommandKind::SavePrefab;
    save_cmd.asset_path = prefab_path;
    save_cmd.value = make_prop_prefab("AI Crate", {0.25f, 0.72f, 0.92f}, has_primitive_mesh);
    proposal.commands.push_back(std::move(save_cmd));

    EditorCommand place_cmd;
    place_cmd.kind = CommandKind::InstantiatePrefab;
    place_cmd.asset_path = prefab_path;
    Json overrides = Json::object();
    Json tf = Json::object();
    Json pos = Json::array();
    pos.push_back(2.0);
    pos.push_back(0.5);
    pos.push_back(-1.0);
    tf["position"] = std::move(pos);
    overrides["astral::Transform"] = std::move(tf);
    place_cmd.value = std::move(overrides);
    proposal.commands.push_back(std::move(place_cmd));
    return proposal;
}

PreviewResult run_proposal_preview(const EditorSession& session, const AgentProposal& proposal) {
    PreviewResult out;
    if (!session.is_open()) {
        out.errors.push_back("no document open");
        return out;
    }
    if (proposal.base_revision.valid() && proposal.base_revision != session.document().revision) {
        out.errors.push_back("proposal targets stale revision " + std::to_string(proposal.base_revision.number) +
                             " (current is " + std::to_string(session.document().revision.number) + ")");
        return out;
    }

    EditorDocument candidate = session.document();
    std::unordered_map<std::string, Json> virtual_prefabs;
    for (const EditorCommand& cmd : proposal.commands) {
        if (cmd.kind == CommandKind::SavePrefab) {
            if (std::string err = validate_command(candidate, session.types(), cmd); !err.empty()) {
                out.errors.push_back(std::move(err));
                return out;
            }
            virtual_prefabs[cmd.asset_path] = cmd.value;
            continue;
        }
        if (std::string err = apply_command(candidate, session.types(), cmd, nullptr, session.asset_root()); !err.empty()) {
            out.errors.push_back(std::move(err));
            return out;
        }
    }

    out.diff = diff_documents(session.document().root, candidate.root);
    out.candidate_doc = std::move(candidate);

    // Load candidate document into a brand-new temporary ECS World with in-memory virtual prefab overlay.
    auto preview_world = std::make_unique<World>();
    std::unordered_map<std::string, Json> disk_cache;
    IEditorFileSystem& fs = const_cast<EditorSession&>(session).filesystem();
    PrefabLibrary prefabs(session.types(), [&](std::string_view rel_path) -> SourceDocument {
        const std::string key(rel_path);
        if (auto it = virtual_prefabs.find(key); it != virtual_prefabs.end()) {
            return {&it->second, 1, {}};
        }
        if (auto it = disk_cache.find(key); it != disk_cache.end()) {
            return {&it->second, 1, {}};
        }
        const std::filesystem::path full = session.asset_root().empty() ? std::filesystem::path(key) : session.asset_root() / key;
        std::string err;
        auto text = fs.read_text(full, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = disk_cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });

    const LoadResult loaded = load_scene(*preview_world, prefabs, out.candidate_doc.root);
    out.errors = loaded.errors;
    out.entity_count = preview_world->count();
    out.preview_world = std::move(preview_world);
    out.valid = out.errors.empty();
    return out;
}

bool ProposalReview::request(IAgentProvider& provider, DocumentEntityId selected, std::string prompt, std::string* error) {
    if (!session_.is_open()) {
        if (error) *error = "no document open";
        return false;
    }
    AgentContext ctx;
    ctx.document_path = session_.document().path;
    ctx.base_revision = session_.document().revision;
    ctx.selected_entity = std::move(selected);
    ctx.prompt = std::move(prompt);
    ctx.document_snapshot = session_.document().root;
    ctx.command_schema = build_command_schema(session_.types());

    auto proposal = provider.propose(ctx, error);
    if (!proposal) return false;
    return stage_proposal(std::move(*proposal), error);
}

bool ProposalReview::stage_proposal(AgentProposal proposal, std::string* error) {
    preview_ = run_proposal_preview(session_, proposal);
    if (!preview_.valid) {
        if (error) *error = preview_.errors.empty() ? "preview validation failed" : preview_.errors.front();
        return false;
    }
    active_ = std::move(proposal);
    return true;
}

TransactionResult ProposalReview::approve() {
    if (!active_) return {.error = "no active proposal"};
    Transaction tx;
    tx.author = "agent:" + active_->agent_id;
    tx.summary = active_->summary;
    tx.expected_revision = active_->base_revision;
    tx.commands = active_->commands;
    TransactionResult res = session_.apply(tx);
    if (res.ok) {
        active_.reset();
        preview_ = {};
    }
    return res;
}

void ProposalReview::reject(std::string_view reason) {
    if (!active_) return;
    session_.audit_log().record({0, "agent:" + active_->agent_id, "reject", active_->summary,
                                 session_.document().path.string(), session_.document().revision.number,
                                 session_.document().revision.number, preview_.diff.changes.size(), true, std::string(reason)});
    active_.reset();
    preview_ = {};
}

} // namespace astral::editor
