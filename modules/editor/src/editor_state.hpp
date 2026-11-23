#pragma once
// Internal state structs for EditorWorkspace (kept small and modular to respect size budgets).

#include "services/play_host.hpp"
#include "shell/editor_panel.hpp"
#include "viewport/viewport_math.hpp"

#include <astral/editor/workspace.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace astral::editor {

struct WorldAuthoringUiState {
    std::vector<PlacementToolDef> tools;
    std::size_t selected_tool = 0;
    std::uint64_t selected_instance_id = 0;
    double place_x = 0.0;
    double place_z = 0.0;
    std::string edit_local_path = "1";
    std::string edit_component = "astral::Name";
    std::string edit_field = "value";
    std::string edit_value = "Renamed";
};

struct DocumentServices {
    LocalEditorFileSystem fs;
    std::unique_ptr<EditorSession> session;
    std::unique_ptr<WorldAuthoringSession> world_authoring;
    std::unique_ptr<PrefabAssistantAgent> builtin_agent;
    std::unique_ptr<CodexCliProvider> codex_agent;
    std::unique_ptr<ProposalReview> review;
    std::unique_ptr<AgentTaskManager> tasks;
    int provider_index = 0; ///< 0 = Built-in Offline, 1 = Codex CLI
    bool world_mode = false;
};

struct SelectionState {
    DocumentEntityId primary{};
    std::vector<DocumentEntityId> multi;
    std::unordered_map<std::uint32_t, Entity> record_to_entity;
    std::string tree_filter;
    bool attach_to_agent = true;
};

struct ViewportState {
    Entity camera{};
    Vec3 cam_pos{0.0f, 3.5f, 8.0f};
    float yaw = 0.0f;
    float pitch = -0.35f;
    float fly_speed = 6.0f;
    GizmoMode gizmo = GizmoMode::Translate;
    TransformSpace space = TransformSpace::World;
    bool snap_enabled = false;
    float snap_step = 0.5f;
    bool show_grid = true;
    bool hovered = false;
    bool focused = false;
    Vec2 rect_min{};
    Vec2 rect_size{960.0f, 540.0f};
    ViewportPreviewMode preview_view = ViewportPreviewMode::Live;
    GizmoDragState drag{};
};

struct ShellVisibility {
    WorkspacePreset preset = WorkspacePreset::Focus;
    int active_rail = 0; ///< 0=Workspace, 1=Assets, 2=World, 3=Tools, 4=AI
    bool layout_dirty = true;
    bool theme_applied = false;
    bool scene_tree = true;
    bool agent = true;
    bool content_shelf = false;
    bool inspector = false;
    bool command_palette = false;
    bool world_panel = false;
    bool console = false;
    bool profiler = false;
    bool render_settings = false;
    bool materials = false;
    bool shortcuts_modal = false;
    bool conflict_modal = false;
    bool autosave_modal = false;
};

struct ShellBuffers {
    std::vector<std::filesystem::path> asset_files;
    std::vector<std::filesystem::path> recent_assets;
    std::vector<std::string> console;
    std::string status_line = "Ready";
    std::string prompt_input;
    std::string revision_input;
    std::string palette_query;
    std::string content_search;
    std::string conflict_message;
    std::string rename_buffer;
    DocumentEntityId renaming_entity{};
    float autosave_timer = 0.0f;
    int agent_tab = 0; ///< 0 = Task, 1 = History
    bool preview_mode = false;
};

struct EditorWorkspace::Impl {
    EditorWorkspaceDesc desc;
    DocumentServices docs;
    SelectionState selection;
    ViewportState viewport;
    PlayHost play;
    WorldAuthoringUiState world_ui;
    ShellVisibility vis;
    ShellBuffers ui;
    PanelRegistry panels;

    explicit Impl(EditorWorkspaceDesc d) : desc(std::move(d)) {
        docs.builtin_agent = std::make_unique<PrefabAssistantAgent>();
        docs.codex_agent = std::make_unique<CodexCliProvider>();
    }
};

struct EditorPanelContext {
    EditorWorkspace& workspace;
    EditorWorkspace::Impl& state;
    Context& app;
};

// Shared internal helpers implemented across modular translation units.
void register_editor_panels(PanelRegistry& registry);
void draw_editor_shell(EditorPanelContext& ec);
void draw_world_authoring_contents(EditorPanelContext& ec);
void sync_scene_to_world_live(EditorPanelContext& ec);
CameraData compute_editor_camera(const ViewportState& vp);
IAgentProvider& active_agent_provider(DocumentServices& docs);

} // namespace astral::editor
