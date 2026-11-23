#pragma once
// Isolated Play-in-Editor runtime host: clones the current editable document (including unsaved edits)
// into a separate ECS World + PhysicsWorld so gameplay never mutates editor authoring data (ADR-0037).

#include <astral/app/app.hpp>
#include <astral/editor/document.hpp>
#include <astral/editor/workspace.hpp>
#include <astral/physics/physics.hpp>
#include <astral/world/world.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace astral::editor {

class PlayHost {
public:
    PlayHost() = default;
    ~PlayHost();
    PlayHost(const PlayHost&) = delete;
    PlayHost& operator=(const PlayHost&) = delete;

    bool start(Context& ctx, const Json& document_root, const IEditorFileSystem& fs,
               const std::filesystem::path& asset_root, std::string* error = nullptr);
    void toggle_pause() noexcept;
    void stop(Context& ctx);
    void step(float dt);

    [[nodiscard]] PlayState state() const noexcept { return state_; }
    [[nodiscard]] bool active() const noexcept { return state_ != PlayState::Stopped; }
    [[nodiscard]] World* world() noexcept { return world_.get(); }

private:
    PlayState state_ = PlayState::Stopped;
    std::unique_ptr<World> world_;
    std::unique_ptr<PhysicsWorld> physics_;
    std::vector<std::pair<Entity, BodyHandle>> bodies_;
    float accumulator_ = 0.0f;
};

} // namespace astral::editor
