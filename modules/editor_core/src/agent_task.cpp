#include <astral/editor/agent_task.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace astral::editor {

namespace {

std::string extract_json_object(std::string_view raw) {
    const std::size_t fence = raw.find("```");
    if (fence != std::string_view::npos) {
        const std::size_t nl = raw.find('\n', fence);
        const std::size_t end_fence = raw.find("```", nl == std::string_view::npos ? fence + 3 : nl + 1);
        if (nl != std::string_view::npos && end_fence != std::string_view::npos && end_fence > nl) {
            raw = raw.substr(nl + 1, end_fence - nl - 1);
        }
    }
    const std::size_t first = raw.find('{');
    const std::size_t last = raw.rfind('}');
    if (first == std::string_view::npos || last == std::string_view::npos || last < first) return {};
    return std::string(raw.substr(first, last - first + 1));
}

std::string build_codex_prompt(const AgentContext& ctx) {
    std::ostringstream ss;
    ss << "You are Astral Engine's native scene editor assistant. Respond ONLY with a single JSON object:\n"
       << "{\"summary\":\"short description\",\"commands\":[ ... ]}\n"
       << "Each command MUST conform to schema_version " << kEditorCommandSchemaVersion << ":\n"
       << ctx.command_schema.dump() << "\n"
       << "Open document: " << ctx.document_path.filename().string()
       << " (rev " << ctx.base_revision.number << ")\n";
    if (ctx.selected_entity.valid()) {
        ss << "Selected entity id: " << ctx.selected_entity.to_string() << "\n";
    }
    ss << "Current document JSON:\n" << ctx.document_snapshot.dump() << "\n"
       << "User request: " << ctx.prompt << "\n";
    return ss.str();
}

bool run_cli_capture(const std::string& command, std::string& output, std::string& error) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        error = "failed to launch external agent process";
        return false;
    }
    char buf[512];
    while (std::fgets(buf, sizeof buf, pipe) != nullptr) output += buf;
#ifdef _WIN32
    const int rc = _pclose(pipe);
#else
    const int rc = pclose(pipe);
#endif
    if (rc != 0) {
        error = "external agent exited with code " + std::to_string(rc);
        return false;
    }
    return true;
}

std::vector<std::string> summarize_plan_steps(const AgentProposal& proposal) {
    std::vector<std::string> steps;
    steps.reserve(proposal.commands.size());
    for (const EditorCommand& cmd : proposal.commands) {
        std::string line(command_kind_name(cmd.kind));
        if (cmd.target.valid()) line += " #" + cmd.target.to_string();
        if (!cmd.component.empty()) line += " " + cmd.component;
        if (!cmd.field.empty()) line += "." + cmd.field;
        if (!cmd.asset_path.empty()) line += " (" + cmd.asset_path + ")";
        steps.push_back(std::move(line));
    }
    return steps;
}

} // namespace

std::string_view agent_task_state_name(AgentTaskState state) noexcept {
    switch (state) {
    case AgentTaskState::Idle: return "Idle";
    case AgentTaskState::Planning: return "Planning";
    case AgentTaskState::Executing: return "Executing";
    case AgentTaskState::Review: return "Review";
    case AgentTaskState::Blocked: return "Blocked";
    case AgentTaskState::Failed: return "Failed";
    case AgentTaskState::Complete: return "Complete";
    }
    return "Idle";
}

std::optional<AgentProposal> CodexCliProvider::parse_response(std::string_view raw_output, const AgentContext& ctx,
                                                              std::string_view agent_id, std::string* error) {
    const std::string json_text = extract_json_object(raw_output);
    if (json_text.empty()) {
        if (error) *error = "agent response did not contain a valid JSON proposal object";
        return std::nullopt;
    }
    std::string parse_err;
    const auto parsed = Json::parse(json_text, &parse_err);
    if (!parsed || !parsed->is_object()) {
        if (error) *error = "invalid proposal JSON: " + parse_err;
        return std::nullopt;
    }
    const Json* cmds = parsed->find("commands");
    if (!cmds || !cmds->is_array() || cmds->size() == 0) {
        if (error) *error = "proposal JSON must include a non-empty \"commands\" array";
        return std::nullopt;
    }
    AgentProposal proposal;
    proposal.agent_id = std::string(agent_id);
    proposal.base_revision = ctx.base_revision;
    proposal.summary = (*parsed)["summary"].as_string();
    if (proposal.summary.empty()) proposal.summary = "AI proposed edit";
    for (std::size_t i = 0; i < cmds->size(); ++i) {
        std::string cmd_err;
        auto cmd = command_from_json((*cmds)[i], &cmd_err);
        if (!cmd) {
            if (error) *error = "command[" + std::to_string(i) + "]: " + cmd_err;
            return std::nullopt;
        }
        proposal.commands.push_back(std::move(*cmd));
    }
    return proposal;
}

std::optional<AgentProposal> CodexCliProvider::propose(const AgentContext& ctx, std::string* error) {
    if (!ctx.base_revision.valid()) {
        if (error) *error = "open a document before invoking Codex CLI";
        return std::nullopt;
    }
    const std::filesystem::path prompt_file =
        std::filesystem::temp_directory_path() / ("astral_codex_prompt_" + std::to_string(next_id_) + ".txt");
    {
        std::ofstream out(prompt_file, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "failed to stage prompt file for Codex CLI";
            return std::nullopt;
        }
        out << build_codex_prompt(ctx);
    }
    const std::string cmd = executable_ + " exec --skip-git-repo-check - < \"" + prompt_file.string() + "\" 2>&1";
    std::string output;
    std::string cli_err;
    const bool ok = run_cli_capture(cmd, output, cli_err);
    std::error_code rm_ec;
    std::filesystem::remove(prompt_file, rm_ec);
    if (!ok) {
        if (error) *error = cli_err + (output.empty() ? "" : (" (" + output.substr(0, 180) + ")"));
        return std::nullopt;
    }
    auto proposal = parse_response(output, ctx, id(), error);
    if (proposal) proposal->proposal_id = "codex-" + std::to_string(next_id_++);
    return proposal;
}

AgentTaskManager::AgentTaskManager(EditorSession& session, AgentPolicy policy)
    : session_(session), policy_(policy) {}

AgentTaskManager::~AgentTaskManager() {
    cancel();
    join_worker();
}

void AgentTaskManager::join_worker() {
    if (worker_.joinable()) worker_.join();
}

AgentTaskState AgentTaskManager::state() const noexcept {
    return active_ ? active_->state : AgentTaskState::Idle;
}

bool AgentTaskManager::busy() const noexcept {
    const AgentTaskState s = state();
    return s == AgentTaskState::Planning || s == AgentTaskState::Executing;
}

bool AgentTaskManager::submit(IAgentProvider& provider, DocumentEntityId selected, std::string prompt, bool async,
                              std::string* error) {
    if (!session_.is_open()) {
        if (error) *error = "no document open";
        return false;
    }
    if (prompt.empty()) {
        if (error) *error = "prompt cannot be empty";
        return false;
    }
    if (busy()) {
        if (error) *error = "an agent task is already running";
        return false;
    }
    join_worker();
    if (active_) archive_active();
    has_preview_ = false;
    preview_ = {};

    AgentTaskRecord rec;
    rec.task_id = "task-" + std::to_string(next_task_id_++);
    rec.prompt = std::move(prompt);
    rec.provider_id = std::string(provider.id());
    rec.provider_name = std::string(provider.display_name());
    rec.context_entity = std::move(selected);
    rec.base_revision = session_.document().revision;
    rec.events.push_back({"Planning", "Collected document context (rev " + std::to_string(rec.base_revision.number) + ")"});

    if (provider.requires_external_process() && !policy_.allow_external_commands) {
        rec.state = AgentTaskState::Blocked;
        rec.blocked_reason = "External agent provider \"" + rec.provider_name +
                             "\" requires enabling External Commands in Agent Policy.";
        rec.events.push_back({"Blocked", rec.blocked_reason});
        active_ = std::move(rec);
        if (error) *error = active_->blocked_reason;
        return false;
    }

    AgentContext ctx;
    ctx.document_path = session_.document().path;
    ctx.base_revision = session_.document().revision;
    ctx.selected_entity = rec.context_entity;
    ctx.prompt = rec.prompt;
    ctx.document_snapshot = session_.document().root;
    ctx.command_schema = build_command_schema(session_.types());

    rec.state = AgentTaskState::Executing;
    rec.events.push_back({"Executing", "Requesting structured proposal from " + rec.provider_name});
    active_ = std::move(rec);
    cancel_flag_.store(false);

    if (!async) {
        std::string err;
        auto prop = provider.propose(ctx, &err);
        finalize_proposal(std::move(prop), std::move(err));
        if (active_ && active_->state == AgentTaskState::Failed && error) *error = active_->error;
        return active_ && active_->state == AgentTaskState::Review;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        worker_result_ = {};
    }
    worker_ = std::thread([this, &provider, ctx = std::move(ctx)]() mutable {
        std::string err;
        std::optional<AgentProposal> prop = provider.propose(ctx, &err);
        std::lock_guard<std::mutex> lock(mutex_);
        worker_result_.cancelled = cancel_flag_.load();
        worker_result_.proposal = std::move(prop);
        worker_result_.error = std::move(err);
        worker_result_.ready = true;
    });
    return true;
}

bool AgentTaskManager::request_revision(IAgentProvider& provider, std::string feedback, bool async, std::string* error) {
    if (!active_ || active_->state != AgentTaskState::Review || !active_->proposal) {
        if (error) *error = "no active proposal in Review state";
        return false;
    }
    const std::string combined = active_->prompt + "\nRevision feedback: " + feedback;
    return submit(provider, active_->context_entity, combined, async, error);
}

void AgentTaskManager::poll() {
    if (!busy()) return;
    WorkerResult res;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!worker_result_.ready) return;
        res = std::move(worker_result_);
        worker_result_ = {};
    }
    join_worker();
    if (res.cancelled) {
        if (active_) {
            active_->state = AgentTaskState::Idle;
            active_->events.push_back({"Cancelled", "Task cancelled before completion"});
            archive_active();
        }
        return;
    }
    finalize_proposal(std::move(res.proposal), std::move(res.error));
}

void AgentTaskManager::finalize_proposal(std::optional<AgentProposal> proposal, std::string err) {
    if (!active_) return;
    if (!proposal) {
        active_->state = AgentTaskState::Failed;
        active_->error = err.empty() ? "agent returned no proposal" : std::move(err);
        active_->events.push_back({"Failed", active_->error});
        return;
    }
    preview_ = run_proposal_preview(session_, *proposal);
    if (!preview_.valid) {
        active_->state = AgentTaskState::Failed;
        active_->error = preview_.errors.empty() ? "isolated preview validation failed" : preview_.errors.front();
        active_->events.push_back({"Failed", active_->error});
        preview_ = {};
        has_preview_ = false;
        return;
    }
    active_->plan_steps = summarize_plan_steps(*proposal);
    active_->command_selection.assign(proposal->commands.size(), true);
    active_->proposal = std::move(*proposal);
    has_preview_ = true;
    active_->state = AgentTaskState::Review;
    active_->events.push_back({"Review", "Validated in isolated ECS preview (" + preview_.diff.summary() + ")"});
}

void AgentTaskManager::cancel() {
    cancel_flag_.store(true);
    join_worker();
    if (active_ && busy()) {
        active_->events.push_back({"Cancelled", "Cancelled by user"});
        active_->state = AgentTaskState::Idle;
        archive_active();
    }
}

void AgentTaskManager::discard(std::string_view reason) {
    if (!active_) return;
    if (active_->proposal) {
        session_.audit_log().record({0, "agent:" + active_->provider_id, "reject", active_->proposal->summary,
                                     session_.document().path.string(), session_.document().revision.number,
                                     session_.document().revision.number,
                                     has_preview_ ? preview_.diff.changes.size() : 0u, true, std::string(reason)});
    }
    active_->events.push_back({"Discarded", std::string(reason)});
    active_->state = AgentTaskState::Idle;
    has_preview_ = false;
    preview_ = {};
    archive_active();
}

TransactionResult AgentTaskManager::apply_commands(const std::vector<EditorCommand>& cmds) {
    if (!active_ || active_->state != AgentTaskState::Review || !active_->proposal) {
        return {.error = "no proposal awaiting approval"};
    }
    if (cmds.empty()) return {.error = "no commands selected"};
    Transaction tx;
    tx.author = "agent:" + active_->proposal->agent_id;
    tx.summary = active_->proposal->summary;
    tx.expected_revision = active_->proposal->base_revision;
    tx.commands = cmds;
    const TransactionResult res = session_.apply(tx);
    if (!res.ok) {
        active_->state = AgentTaskState::Failed;
        active_->error = res.error;
        active_->events.push_back({"Failed", res.error});
        return res;
    }
    active_->applied_changes = res.diff.changes.size();
    active_->state = AgentTaskState::Complete;
    active_->events.push_back({"Complete", "Applied " + res.diff.summary() + " -> rev " +
                                               std::to_string(res.new_revision.number)});
    has_preview_ = false;
    preview_ = {};
    return res;
}

TransactionResult AgentTaskManager::approve_all() {
    if (!active_ || !active_->proposal) return {.error = "no proposal awaiting approval"};
    return apply_commands(active_->proposal->commands);
}

TransactionResult AgentTaskManager::approve_selected() {
    if (!active_ || !active_->proposal) return {.error = "no proposal awaiting approval"};
    std::vector<EditorCommand> chosen;
    const auto& cmds = active_->proposal->commands;
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        if (i < active_->command_selection.size() && active_->command_selection[i]) chosen.push_back(cmds[i]);
    }
    return apply_commands(chosen);
}

void AgentTaskManager::archive_active() {
    if (!active_) return;
    history_.push_back(std::move(*active_));
    active_.reset();
}

void AgentTaskManager::clear_finished() {
    if (!active_ || busy() || active_->state == AgentTaskState::Review) return;
    archive_active();
    has_preview_ = false;
    preview_ = {};
}

} // namespace astral::editor
