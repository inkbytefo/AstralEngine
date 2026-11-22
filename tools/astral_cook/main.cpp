// astral-cook: source assets -> cooked files -> .apak.
//
//   astral-cook <source-dir> -o <cooked-dir> [--pack game.apak] [options]
//
//   -o, --out DIR          cooked files (kept between runs: only changed sources cook again)
//   --pack FILE            also write every cooked file into one .apak
//   --uncompressed         RGBA8 textures instead of BC7/BC5
//   --quality 0..4         BC7 effort (default 1)
//   --level 1..19          zstd level for the pack (default 9)
//   --jobs N               worker threads (default: all cores)
//   -q, --quiet            errors only

#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <astral/core/log.hpp>
#include <astral/script/compile.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

using namespace astral;

namespace {

int usage() {
    std::fprintf(stderr,
                 "usage: astral-cook <source-dir> -o <cooked-dir> [--pack FILE] [--uncompressed] [--quality 0..4]\n"
                 "                   [--level 1..19] [--jobs N] [-q]\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path source, output, pack;
    TextureCookOptions textures;
    PackWriteOptions pack_options;
    unsigned workers = 0;
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (arg == "-o" || arg == "--out") {
            const char* v = value();
            if (!v) return usage();
            output = v;
        } else if (arg == "--pack") {
            const char* v = value();
            if (!v) return usage();
            pack = v;
        } else if (arg == "--uncompressed") {
            textures.compress = false;
        } else if (arg == "--quality") {
            const char* v = value();
            if (!v) return usage();
            textures.bc7_quality = static_cast<std::uint32_t>(std::atoi(v));
        } else if (arg == "--level") {
            const char* v = value();
            if (!v) return usage();
            pack_options.level = std::atoi(v);
        } else if (arg == "--jobs") {
            const char* v = value();
            if (!v) return usage();
            workers = static_cast<unsigned>(std::atoi(v));
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else if (arg == "-h" || arg == "--help") {
            return usage();
        } else if (!arg.empty() && arg[0] != '-' && source.empty()) {
            source = arg;
        } else {
            std::fprintf(stderr, "astral-cook: unknown argument %s\n", arg.c_str());
            return usage();
        }
    }
    if (source.empty() || output.empty()) return usage();

    Logger log;
    auto sink = std::make_shared<ConsoleSink>();
    log.add_sink(sink);
    if (quiet) log.set_level(LogLevel::Error);
    JobSystem jobs(workers);
    const auto start = std::chrono::steady_clock::now();
    const CookReport report = cook_directory({.source = source, .output = output, .textures = textures, .jobs = &jobs,
                                             .compile_script = [](std::string_view name, const std::vector<std::uint8_t>& text, std::vector<std::uint8_t>& out, std::string* error) {
                                                 return compile_script(name, std::string_view(reinterpret_cast<const char*>(text.data()), text.size()), out, error);
                                             },
                                             .logger = &log});
    if (!report.ok()) {
        std::fprintf(stderr, "astral-cook: %zu error(s)\n", report.errors.size());
        return 1;
    }
    if (!pack.empty()) {
        std::string error;
        if (!write_pack(output, pack, &error, pack_options)) {
            std::fprintf(stderr, "astral-cook: %s\n", error.c_str());
            return 1;
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(pack, ec);
        log.info("pack: {} ({:.1f} MB)", pack.string(), static_cast<double>(size) / (1024.0 * 1024.0));
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    log.info("done in {:.2f} s", seconds);
    return 0;
}
