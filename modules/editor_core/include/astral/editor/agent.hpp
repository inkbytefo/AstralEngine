#pragma once
// AI agent proposal interface, isolated ECS preview sandbox, and human approval workflow (ADR-0035).

#include <astral/editor/command.hpp>
#include <astral/editor/transaction.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/world.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astral::editor {

struct AgentContext {
    std::filesystem::path document_path;
    DocumentRevision base_revision{};
    DocumentEntityId selected_entity{};
    std::string prompt;
    Json document_snapshot = Json::object();
    Json command_schema = Json::object();
};

struct AgentProposal {
    std::string proposal_id;
    std::string agent_id;
    std::string summary;
    DocumentRevision base_revision{};
    std::vector<EditorCommand> commands;
};

/// Interface for an AI agent provider that proposes structured, schema-validated document commands.
class IAgentProvider {
public:
    virtual ~IAgentProvider() = default;
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view display_name() const noexcept = 0;
    [[nodiscard]] virtual bool requires_external_process() const noexcept { return false; }
    [[nodiscard]] virtual std::optional<AgentProposal> propose(const AgentContext& ctx, std::string* error = nullptr) = 0;
};

/// Built-in deterministic reference agent: proposes creating/placing a reusable prefab or tuning a target entity.
class PrefabAssistantAgent final : public IAgentProvider {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "prefab_assistant"; }
    [[nodiscard]] std::string_view display_name() const noexcept override { return "Prefab Assistant (Built-in)"; }
    [[nodiscard]] std::optional<AgentProposal> propose(const AgentContext& ctx, std::string* error = nullptr) override;

private:
    std::uint64_t next_id_ = 1;
};

/// Temporary isolated ECS World that previews an AgentProposal without touching the main document or live world.
struct PreviewResult {
    bool valid = false;
    std::size_t entity_count = 0;
    std::vector<std::string> errors;
    DocumentDiff diff;
    EditorDocument candidate_doc;
    std::unique_ptr<World> preview_world;
};

[[nodiscard]] PreviewResult run_proposal_preview(const EditorSession& session, const AgentProposal& proposal);

/// Human-in-the-loop review session: generate proposal -> inspect diff & isolated ECS preview -> approve or reject.
class ProposalReview {
public:
    explicit ProposalReview(EditorSession& session) : session_(session) {}

    bool request(IAgentProvider& provider, DocumentEntityId selected, std::string prompt, std::string* error = nullptr);
    bool stage_proposal(AgentProposal proposal, std::string* error = nullptr);

    [[nodiscard]] bool has_active() const noexcept { return active_.has_value(); }
    [[nodiscard]] const AgentProposal* active() const noexcept { return active_ ? &*active_ : nullptr; }
    [[nodiscard]] const PreviewResult* preview() const noexcept { return active_ ? &preview_ : nullptr; }

    /// Applies the reviewed proposal as a single atomic transaction on EditorSession and clears the active review.
    TransactionResult approve();
    /// Discards the active proposal and temporary preview world without touching the document or disk.
    void reject(std::string_view reason = "rejected by user");

private:
    EditorSession& session_;
    std::optional<AgentProposal> active_;
    PreviewResult preview_;
};

} // namespace astral::editor
