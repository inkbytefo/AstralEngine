// astral-luau-check <astral.d.luau> <script.luau>...
// Type-checks Luau scripts against the generated definitions (astral.d.luau) with Luau's own analyzer.
// Exit code 1 and one line per problem if the definitions do not load or a script has a type error.
// (The stock luau-analyze CLI cannot load a definitions file, hence this small driver.)
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Config.h"
#include "Luau/Frontend.h"
#include "Luau/ModuleResolver.h"
#include "Luau/TypeInfer.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

struct Files final : Luau::FileResolver {
    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override {
        if (auto text = read_file(name)) return Luau::SourceCode{*text, Luau::SourceCode::Module};
        return std::nullopt;
    }
    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo*, Luau::AstExpr*, const Luau::TypeCheckLimits&) override { return std::nullopt; }
};

struct Configs final : Luau::ConfigResolver {
    Luau::Config config;
    const Luau::Config& getConfig(const Luau::ModuleName&, const Luau::TypeCheckLimits&) const override { return config; }
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: astral-luau-check <astral.d.luau> <script.luau>...\n");
        return 2;
    }
    Files files;
    Configs configs;
    configs.config.mode = Luau::Mode::Nonstrict;
    Luau::Frontend frontend(Luau::SolverMode::New, &files, &configs, {});
    Luau::registerBuiltinGlobals(frontend, frontend.globals);

    const auto definitions = read_file(argv[1]);
    if (!definitions) {
        std::fprintf(stderr, "%s: cannot read\n", argv[1]);
        return 1;
    }
    const Luau::LoadDefinitionFileResult loaded = frontend.loadDefinitionFile(frontend.globals, frontend.globals.globalScope, *definitions, "@astral", false);
    if (!loaded.success) {
        for (const auto& e : loaded.parseResult.errors) std::fprintf(stderr, "%s(%d,%d): definitions: %s\n", argv[1], e.getLocation().begin.line + 1, e.getLocation().begin.column + 1, e.getMessage().c_str());
        if (loaded.module) {
            for (const auto& e : loaded.module->errors) {
                std::fprintf(stderr, "%s(%d,%d): definitions: %s\n", argv[1], e.location.begin.line + 1, e.location.begin.column + 1, Luau::toString(e).c_str());
            }
        }
        return 1;
    }
    Luau::freeze(frontend.globals.globalTypes);

    int problems = 0;
    for (int i = 2; i < argc; ++i) {
        const Luau::CheckResult result = frontend.check(argv[i]);
        for (const Luau::TypeError& e : result.errors) {
            std::fprintf(stderr, "%s(%d,%d): %s\n", argv[i], e.location.begin.line + 1, e.location.begin.column + 1, Luau::toString(e).c_str());
            ++problems;
        }
    }
    if (problems) return 1;
    std::printf("luau scripts ok\n");
    return 0;
}
