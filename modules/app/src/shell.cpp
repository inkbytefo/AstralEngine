#include <astral/app/shell.hpp>
#include <astral/gpu/device.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_MSC_VER)
#include <share.h>
#endif

namespace astral {

bool ShellOptions::has(std::string_view flag) const noexcept {
    for (const std::string& a : args) {
        if (a == flag) return true;
    }
    return false;
}

std::string ShellOptions::value(std::string_view name, std::string_view fallback) const {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name && args[i + 1].rfind("--", 0) != 0) return args[i + 1];
    }
    return std::string(fallback);
}

std::vector<std::string> ShellOptions::positional(const std::vector<std::string>& value_flags) const {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i].rfind("--", 0) == 0) {
            if (std::find(value_flags.begin(), value_flags.end(), args[i]) != value_flags.end()) ++i;
            continue;
        }
        out.push_back(args[i]);
    }
    return out;
}

ShellOptions parse_shell_args(int argc, const char* const* argv) {
    ShellOptions o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string("0"); };
        if (a == "--capture") o.capture = next();
        else if (a == "--frames") o.frames = static_cast<std::uint32_t>(std::atoi(next().c_str()));
        else if (a == "--width") o.width = static_cast<std::uint32_t>(std::atoi(next().c_str()));
        else if (a == "--height") o.height = static_cast<std::uint32_t>(std::atoi(next().c_str()));
        else if (a == "--script-api") o.script_api = next();
        else if (a == "--script-api-only") {
            o.script_api = next();
            o.script_api_only = true;
        }
        else o.args.push_back(a);
    }
    return o;
}

namespace {

/// Wraps the game: Esc quits (a window run), and the script API is written once the game has started.
class ShellGame final : public Game {
public:
    ShellGame(Game& inner, const ShellOptions& options) : inner_(inner), options_(options) {}
    void on_start(Context& ctx) override {
        inner_.on_start(ctx);
        if (!options_.script_api.empty()) ctx.script_service().write_script_api(options_.script_api);
    }
    void on_update(Context& ctx) override {
        if (!options_.capturing() && ctx.input.pressed(Key::Escape)) ctx.quit();
        inner_.on_update(ctx);
    }
    void on_shutdown(Context& ctx) override { inner_.on_shutdown(ctx); }
    void on_debug_ui(Context& ctx) override { inner_.on_debug_ui(ctx); }

private:
    Game& inner_;
    const ShellOptions& options_;
};

const char* adapter_type_name(gpu::AdapterType type) noexcept {
    switch (type) {
    case gpu::AdapterType::IntegratedGpu: return "integrated";
    case gpu::AdapterType::DiscreteGpu: return "discrete";
    case gpu::AdapterType::VirtualGpu: return "virtual";
    case gpu::AdapterType::Cpu: return "cpu";
    default: return "other";
    }
}

bool write_ppm(const std::string& path, const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h) {
#if defined(_MSC_VER)
    std::FILE* f = _fsopen(path.c_str(), "wb", _SH_DENYNO);
#else
    std::FILE* f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (std::size_t p = 0; p < std::size_t{w} * h; ++p) std::fwrite(&rgba[p * 4], 1, 3, f);
    std::fclose(f);
    return true;
}

} // namespace

int run_game(const ShellOptions& options, AppDesc desc, Game& game, std::uint32_t default_capture_frames) {
    if (options.width) desc.width = options.width;
    if (options.height) desc.height = options.height;
    ShellGame shell(game, options);
    if (options.script_api_only) { // the API documents need the registered types only: a headless frame is enough
        desc.headless = true;
        desc.audio = false;
        desc.hot_reload = false;
        App app(desc);
        app.run_frames(shell, 1);
        return 0;
    }
    if (!options.capturing()) {
        App app(desc);
        return app.run(shell);
    }
    desc.offscreen = true;
    desc.fixed_timestep = 1.0f / 60.0f; // captures are deterministic
    App app(desc);
    app.run_frames(shell, options.frames ? options.frames : default_capture_frames);
    std::vector<std::uint8_t> rgba;
    if (!app.read_frame(rgba) || !write_ppm(options.capture, rgba, desc.width, desc.height)) {
        std::fprintf(stderr, "capture failed (no GPU?)\n");
        return 1;
    }
    if (const gpu::Device* device = app.context().device) {
        const gpu::AdapterInfo info = device->adapter_info();
        std::printf("capture device: name=%.*s | vendor=0x%04x | device=0x%04x | driver=%u | driver_name=%.*s | type=%s\n",
                    static_cast<int>(info.name.size()), info.name.data(), info.vendor_id, info.device_id,
                    info.driver_id, static_cast<int>(info.driver_name.size()), info.driver_name.data(),
                    adapter_type_name(info.type));
    }
    std::printf("wrote %s\n", options.capture.c_str());
    return 0;
}

} // namespace astral
