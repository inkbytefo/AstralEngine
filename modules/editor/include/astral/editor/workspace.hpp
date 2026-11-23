#pragma once
// Standalone AI-First Native Desktop Editor workspace (ADR-0035, ADR-0036, ADR-0037).
// Public header exposes ONLY engine types (zero ImGui headers). Games and dedicated servers never link Astral::Editor.

#include <astral/app/app.hpp>
#include <astral/editor/agent.hpp>
#include <astral/editor/agent_task.hpp>
#include <astral/editor/document.hpp>
#include <astral/editor/transaction.hpp>
#include <astral/editor/world_authoring.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

enum class WorkspacePreset : std::uint8_t { Focus, Advanced };
enum class GizmoMode : std::uint8_t { Select, Translate, Rotate, Scale };
enum class TransformSpace : std::uint8_t { World, Local };
enum class PlayState : std::uint8_t { Stopped, Playing, Paused };
enum class ViewportPreviewMode : std::uint8_t { Live, CandidateAfter };

struct EditorWorkspaceDesc {
    std::filesystem::path asset_root = "assets";
    std::filesystem::path initial_document; ///< Optional scene/prefab/world relative to asset_root.
    std::string project_name = "Astral Workshop";
    WorkspacePreset initial_preset = WorkspacePreset::Focus;
    bool manual_save = false; ///< True for interactive Ctrl+S dirty buffer; false keeps E0 immediate persistence.
};

/// Standalone desktop editor Game host: calm Focus workspace with live offscreen 3D viewport,
/// progressive Advanced tools, unified transactional editing (Gizmo/Inspector/AI), isolated Play mode,
/// and async 7-state AI review with genuine Before/After 3D preview.
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
    bool open_world_map(Context& ctx, const std::filesystem::path& relative_or_full_path, std::string* error = nullptr);
    bool reload_live_scene(Context& ctx, std::string* error = nullptr);
    bool reload_world_preview(Context& ctx, std::string* error = nullptr);
    bool save_document(std::string* error = nullptr, bool force_overwrite = false);

    bool request_ai_proposal(std::string prompt, std::string* error = nullptr);
    TransactionResult approve_proposal(Context& ctx);
    void reject_proposal(std::string_view reason = "rejected by user");
    TransactionResult undo(Context& ctx);
    TransactionResult redo(Context& ctx);

    // Selection, viewport, gizmo, and Play controls
    void select_entity(DocumentEntityId id);
    [[nodiscard]] const DocumentEntityId& selected_entity() const noexcept;
    [[nodiscard]] const std::vector<DocumentEntityId>& selected_entities() const noexcept;
    [[nodiscard]] DocumentEntityId pick_entity_at(Context& ctx, Vec2 viewport_uv);
    void focus_selected(Context& ctx);

    void set_workspace_preset(WorkspacePreset preset);
    [[nodiscard]] WorkspacePreset workspace_preset() const noexcept;
    void reset_workspace_layout();
    void open_inspector(bool open = true);
    [[nodiscard]] bool is_inspector_open() const noexcept;
    void open_command_palette(bool open = true);

    void set_gizmo_mode(GizmoMode mode) noexcept;
    [[nodiscard]] GizmoMode gizmo_mode() const noexcept;
    void set_transform_space(TransformSpace space) noexcept;
    [[nodiscard]] TransformSpace transform_space() const noexcept;
    void set_snap(bool enabled, float step = 0.5f) noexcept;

    bool enter_play_mode(Context& ctx, std::string* error = nullptr);
    void toggle_pause_play_mode() noexcept;
    void stop_play_mode(Context& ctx);
    [[nodiscard]] PlayState play_state() const noexcept;

    void set_viewport_preview_mode(ViewportPreviewMode mode) noexcept;
    [[nodiscard]] ViewportPreviewMode viewport_preview_mode() const noexcept;

    // Session and diagnostic accessors
    [[nodiscard]] EditorSession* session() noexcept;
    [[nodiscard]] const EditorSession* session() const noexcept;
    [[nodiscard]] WorldAuthoringSession* world_authoring() noexcept;
    [[nodiscard]] const WorldAuthoringSession* world_authoring() const noexcept;
    [[nodiscard]] const std::vector<PlacementToolDef>& placement_tools() const noexcept;
    [[nodiscard]] ProposalReview* review() noexcept;
    [[nodiscard]] const ProposalReview* review() const noexcept;
    [[nodiscard]] AgentTaskManager* agent_tasks() noexcept;
    [[nodiscard]] const AgentTaskManager* agent_tasks() const noexcept;
    [[nodiscard]] const std::vector<std::string>& console_lines() const noexcept;
    [[nodiscard]] const std::vector<std::filesystem::path>& asset_files() const noexcept;
    [[nodiscard]] bool preview_mode() const noexcept;
    void set_preview_mode(bool enabled) noexcept;
    void push_console(std::string line);
    void refresh_asset_list();

    struct Impl; ///< Opaque internal workspace state.

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace astral::editor
