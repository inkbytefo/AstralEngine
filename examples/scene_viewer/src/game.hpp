#pragma once
#include <astral/astral.hpp>

#include <string>
#include <vector>

namespace scene_viewer {

/// Shows a scene file and reloads it whole when it changes on disk (prefab files reload by themselves).
/// `debug`: light ranges, axes, a grid; `ui`: the debug UI open at start (F1 toggles it).
class ViewerGame final : public astral::Game {
public:
    ViewerGame(std::string file, bool debug, bool ui) : file_(std::move(file)), debug_(debug), ui_(ui) {}
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

private:
    void load(astral::Context& ctx);

    std::string file_;
    bool debug_ = false;
    bool ui_ = false;
    std::vector<astral::Entity> loaded_;
    astral::AssetId doc_id_{};
    std::uint32_t version_ = 0;
};

} // namespace scene_viewer
