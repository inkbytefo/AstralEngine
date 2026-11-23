#pragma once
// Standalone desktop AI-assisted editor workspace (ADR-0035).
// Public header exposes only engine types (no ImGui headers). Games and dedicated servers never link Astral::Editor.

#include <astral/app/app.hpp>
#include <astral/editor/agent.hpp>
#include <astral/editor/document.hpp>
#include <astral/editor/transaction.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

struct EditorWorkspaceDesc {
    std::filesystem::path asset_root = "assets";
    std::filesystem::path initial_document; ///< Optional scene/prefab relative to asset_root.
};

/// Standalone desktop editor Game host: dockable viewport, asset browser, hierarchy, reflection inspector
/// backed by transactional document commands, AI proposal review with isolated ECS preview, console, and profiler.
class EditorWorkspace final : public Game {
public:
    explicit EditorWorkspace(EditorWorkspaceDesc desc = {});
    ~EditorWorkspace() override;
    EditorWorkspace(const EditorWorkspace&) = delete;
    EditorWorkspace& operator=(const EditorWorkspace&) = delete;

    // Game lifecycle
    void on_start(Context& ctx) override;
    void on_update(Context& ctx) override;
    void on_debug_ui(Context& ctx) override;
    void on_shutdown(Context& ctx) override;

    // Programmatic workflow API (shared by interactive UI and automated acceptance tests)
    bool open_document(Context& ctx, const std::filesystem::path& relative_or_full_path, std::string* error = nullptr);
    bool reload_live_scene(Context& ctx, std::string* error = nullptr);
    bool request_ai_proposal(std::string prompt, std::string* error = nullptr);
    TransactionResult approve_proposal(Context& ctx);
    void reject_proposal(std::string_view reason = "rejected by user");
    TransactionResult undo(Context& ctx);
    TransactionResult redo(Context& ctx);

    void select_entity(DocumentEntityId id) { selected_ = std::move(id); }
    [[nodiscard]] const DocumentEntityId& selected_entity() const noexcept { return selected_; }
    [[nodiscard]] EditorSession* session() noexcept { return session_.get(); }
    [[nodiscard]] const EditorSession* session() const noexcept { return session_.get(); }
    [[nodiscard]] ProposalReview* review() noexcept { return review_.get(); }
    [[nodiscard]] const ProposalReview* review() const noexcept { return review_.get(); }
    [[nodiscard]] const std::vector<std::string>& console_lines() const noexcept { return console_; }
    [[nodiscard]] const std::vector<std::filesystem::path>& asset_files() const noexcept { return asset_files_; }
    [[nodiscard]] bool preview_mode() const noexcept { return preview_mode_; }
    void set_preview_mode(bool enabled) noexcept { preview_mode_ = enabled; }
    void push_console(std::string line);
    void refresh_asset_list();

private:
    void clear_scene_entities(World& world);
    void ensure_editor_camera(Context& ctx);
    void draw_panels(Context& ctx);

    EditorWorkspaceDesc desc_;
    LocalEditorFileSystem fs_;
    std::unique_ptr<EditorSession> session_;
    std::unique_ptr<PrefabAssistantAgent> default_agent_;
    std::unique_ptr<ProposalReview> review_;
    DocumentEntityId selected_{};
    Entity camera_{};
    std::vector<std::filesystem::path> asset_files_;
    std::vector<std::string> console_;
    std::string prompt_input_ = "Create reusable crate prefab";
    std::string status_line_ = "Ready";
    bool preview_mode_ = false;
    float cam_yaw_ = 0.0f;
    float cam_pitch_ = -0.35f;
};

} // namespace astral::editor
