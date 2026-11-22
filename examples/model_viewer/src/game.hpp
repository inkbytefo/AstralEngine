#pragma once
#include <astral/astral.hpp>

#include <string>

namespace model_viewer {

/// Shows `model_file` (a path relative to the asset root) in the studio.
class ViewerGame final : public astral::Game {
public:
    explicit ViewerGame(std::string model_file) : file_(std::move(model_file)) {}
    void on_start(astral::Context& ctx) override;

private:
    std::string file_;
};

} // namespace model_viewer
