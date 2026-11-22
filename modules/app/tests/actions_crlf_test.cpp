#include <astral/app/app.hpp>
#include <astral/app/input_actions.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using namespace astral;

namespace {

std::string to_crlf(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 64);
    for (char c : text) {
        if (c == '\n') out += "\r\n";
        else if (c != '\r') out += c;
    }
    return out;
}

void write_raw(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(out.good());
}

} // namespace

TEST_CASE("actions files with CRLF line endings load identically to LF") {
    const std::string lf =
        "{\n"
        "  \"format\": \"astral.actions\",\n"
        "  \"version\": 1,\n"
        "  \"actions\": {\n"
        "    \"fire\": {\n"
        "      \"type\": \"button\",\n"
        "      \"keys\": [\"Space\"],\n"
        "      \"mouse\": [\"Left\"],\n"
        "      \"gamepad\": [\"A\"]\n"
        "    },\n"
        "    \"throttle\": {\n"
        "      \"type\": \"axis\",\n"
        "      \"negative\": [\"S\"],\n"
        "      \"positive\": [\"W\"],\n"
        "      \"gamepad_axis\": \"RightTrigger\"\n"
        "    },\n"
        "    \"move\": {\n"
        "      \"type\": \"axis2\",\n"
        "      \"x\": {\"negative\": [\"A\", \"Left\"], \"positive\": [\"D\", \"Right\"]},\n"
        "      \"y\": {\"negative\": [\"S\"], \"positive\": [\"W\"]},\n"
        "      \"stick\": \"left\",\n"
        "      \"dpad\": true\n"
        "    }\n"
        "  }\n"
        "}\n";
    const std::string crlf = to_crlf(lf);

    std::string err_lf;
    std::string err_crlf;
    const auto doc_lf = Json::parse(lf, &err_lf);
    const auto doc_crlf = Json::parse(crlf, &err_crlf);
    REQUIRE_MESSAGE(doc_lf.has_value(), err_lf);
    REQUIRE_MESSAGE(doc_crlf.has_value(), err_crlf);

    InputActions act_lf;
    InputActions act_crlf;
    REQUIRE_MESSAGE(act_lf.load(*doc_lf, &err_lf), err_lf);
    REQUIRE_MESSAGE(act_crlf.load(*doc_crlf, &err_crlf), err_crlf);
    CHECK(act_crlf.names() == act_lf.names());

    Input in;
    in.begin_frame();
    in.set_key(Key::Space, true);
    in.set_key(Key::D, true);
    in.set_key(Key::W, true);
    act_lf.update(in);
    act_crlf.update(in);
    CHECK(act_crlf.pressed("fire") == act_lf.pressed("fire"));
    CHECK(act_crlf.value("throttle") == act_lf.value("throttle"));
    CHECK(act_crlf.axis("move").x == act_lf.axis("move").x);
    CHECK(act_crlf.axis("move").y == act_lf.axis("move").y);

    // Also exercise file loading through Context::load_actions.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "astral_actions_crlf_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    write_raw(dir / "lf.actions", lf);
    write_raw(dir / "crlf.actions", crlf);

    auto load_via_app = [&](const char* path) {
        std::vector<std::string> names;
        struct G final : Game {
            const char* file = nullptr;
            bool ok = false;
            std::vector<std::string>* names = nullptr;
            void on_start(Context& ctx) override {
                ok = ctx.load_actions(file);
                *names = ctx.actions.names();
            }
        } game;
        game.file = path;
        game.names = &names;
        App app({.headless = true, .asset_root = dir.string(), .hot_reload = false});
        app.run_frames(game, 1);
        REQUIRE(game.ok);
        return names;
    };

    CHECK(load_via_app("crlf.actions") == load_via_app("lf.actions"));
    std::filesystem::remove_all(dir, ec);
}
