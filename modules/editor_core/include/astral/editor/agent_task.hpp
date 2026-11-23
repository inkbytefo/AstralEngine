#pragma once
// Asynchronous 7-state AI agent task lifecycle, policy gate, selective approval, and Codex CLI adapter (ADR-0037).
// GPU/UI-free (Katman 3: Astral::Editor_core).

#include <astral/editor/agent.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace astral::editor {

enum class AgentTaskState : std::uint8_t {
    Idle,
    Planning,
    Executing,
    Review,
    Blocked,
    Failed,
    Complete,
};

[[nodiscard]] std::string_view agent_task_state_name(AgentTaskState state) noexcept;

/// Explicit safety & capability policy governing what an agent may perform automatically vs with approval.
struct AgentPolicy {
    bool allow_read_and_preview = true;      ///< Read project metadata & run isolated ECS preview.
    bool require_approval_for_edits = true;  ///< Every live document/disk mutation requires explicit approval.
    bool allow_external_commands = false;    ///< Allow invoking external CLI providers (e.g. Codex CLI).
    bool allow_network = false;              ///< Explicit permission for network-backed providers.
};

struct AgentTaskEvent {
    std::string stage;
    std::string message;
};

struct AgentTaskRecord {
    std::string task_id;
    std::string prompt;
    std::string provider_id;
    std::string provider_name;
    DocumentEntityId context_entity{};
    AgentTaskState state = AgentTaskState::Idle;
    DocumentRevision base_revision{};
    std::vector<std::string> plan_steps;
    std::vector<AgentTaskEvent> events;
    std::optional<AgentProposal> proposal;
    std::vector<bool> command_selection; ///< Per-command toggle for targeted "Approve selected".
    std::string blocked_reason;
    std::string error;
    std::size_t applied_changes = 0;
};

/// External local Codex CLI provider implementing IAgentProvider via structured JSON command protocol.
class CodexCliProvider final : public IAgentProvider {
public:
    explicit CodexCliProvider(std::string executable = "codex") : executable_(std::move(executable)) {}

    [[nodiscard]] std::string_view id() const noexcept override { return "codex_cli"; }
    [[nodiscard]] std::string_view display_name() const noexcept override { return "Codex CLI (Local)"; }
    [[nodiscard]] bool requires_external_process() const noexcept override { return true; }
    [[nodiscard]] std::optional<AgentProposal> propose(const AgentContext& ctx, std::string* error = nullptr) override;

    /// Parses a raw JSON or fenced-JSON response into a validated AgentProposal (unit-testable offline).
    [[nodiscard]] static std::optional<AgentProposal> parse_response(std::string_view raw_output,
                                                                     const AgentContext& ctx,
                                                                     std::string_view agent_id,
                                                                     std::string* error = nullptr);

private:
    std::string executable_;
    std::uint64_t next_id_ = 1;
};

/// Manages asynchronous agent task execution, isolated ECS preview validation, selective approval, and history.
class AgentTaskManager {
public:
    explicit AgentTaskManager(EditorSession& session, AgentPolicy policy = {});
    ~AgentTaskManager();
    AgentTaskManager(const AgentTaskManager&) = delete;
    AgentTaskManager& operator=(const AgentTaskManager&) = delete;

    [[nodiscard]] const AgentPolicy& policy() const noexcept { return policy_; }
    void set_policy(AgentPolicy policy) noexcept { policy_ = policy; }

    /// Submits a new asynchronous task (or runs synchronously when `async == false`, useful in deterministic tests).
    bool submit(IAgentProvider& provider, DocumentEntityId selected, std::string prompt,
                bool async = true, std::string* error = nullptr);
    /// Requests a revision of the active Review task using natural-language feedback.
    bool request_revision(IAgentProvider& provider, std::string feedback,
                          bool async = true, std::string* error = nullptr);
    /// Polls background worker completion and transitions Executing -> Review / Failed on the main thread.
    void poll();
    /// Cancels an in-flight task (safe and idempotent).
    void cancel();
    /// Discards the active proposal without modifying the document.
    void discard(std::string_view reason = "discarded by user");

    /// Applies all commands (or only commands marked true in `current()->command_selection`).
    TransactionResult approve_all();
    TransactionResult approve_selected();
    /// Resets a completed/failed/blocked task back to Idle.
    void clear_finished();

    [[nodiscard]] AgentTaskState state() const noexcept;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] AgentTaskRecord* current() noexcept { return active_ ? &*active_ : nullptr; }
    [[nodiscard]] const AgentTaskRecord* current() const noexcept { return active_ ? &*active_ : nullptr; }
    [[nodiscard]] const PreviewResult* preview() const noexcept { return has_preview_ ? &preview_ : nullptr; }
    [[nodiscard]] const std::vector<AgentTaskRecord>& history() const noexcept { return history_; }

private:
    struct WorkerResult {
        bool ready = false;
        bool cancelled = false;
        std::optional<AgentProposal> proposal;
        std::string error;
    };

    void join_worker();
    void finalize_proposal(std::optional<AgentProposal> proposal, std::string err);
    void archive_active();
    TransactionResult apply_commands(const std::vector<EditorCommand>& cmds);

    EditorSession& session_;
    AgentPolicy policy_;
    std::optional<AgentTaskRecord> active_;
    PreviewResult preview_;
    bool has_preview_ = false;
    std::vector<AgentTaskRecord> history_;
    std::uint64_t next_task_id_ = 1;

    std::thread worker_;
    std::mutex mutex_;
    std::atomic<bool> cancel_flag_{false};
    WorkerResult worker_result_;
};

} // namespace astral::editor
