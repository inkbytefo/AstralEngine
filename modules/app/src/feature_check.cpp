#include <astral/app/feature.hpp>
#include <astral/app/feature_check.hpp>

#include "string_hash.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <unordered_map>

namespace astral {

namespace fs = std::filesystem;

namespace {

std::optional<Json> read_json(const fs::path& file, std::string* error) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) {
        *error = "cannot read " + file.string();
        return std::nullopt;
    }
    const auto size = in.tellg();
    if (size < 0) {
        *error = "cannot read " + file.string();
        return std::nullopt;
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    if (!text.empty() && !in.read(text.data(), size)) {
        *error = "cannot read " + file.string();
        return std::nullopt;
    }
    return Json::parse(text, error);
}

bool is_source(const fs::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" || ext == ".inl";
}

} // namespace

std::vector<std::string> check_features(const fs::path& sources, const fs::path& manifests) {
    std::vector<std::string> out;
    std::vector<FeatureManifest> all;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(manifests, ec)) {
        if (!entry.is_directory()) continue;
        const fs::path file = entry.path() / "feature.json";
        if (!fs::exists(file)) continue;
        std::string error;
        const auto doc = read_json(file, &error);
        const auto manifest = doc ? parse_feature_manifest(*doc, &error) : std::nullopt;
        if (!manifest) {
            out.push_back(file.string() + ": " + error);
            continue;
        }
        if (manifest->name != entry.path().filename().string()) {
            out.push_back(file.string() + ": \"name\" is \"" + manifest->name + "\" but the folder is \"" + entry.path().filename().string() + "\"");
        }
        all.push_back(*manifest);
    }
    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    std::string graph_error;
    if (!FeatureSet::order(all, &graph_error)) out.push_back(manifests.string() + ": " + graph_error);

    std::unordered_map<std::string_view, const FeatureManifest*, StringHash, std::equal_to<>> by_name;
    by_name.reserve(all.size());
    for (const auto& m : all) by_name.emplace(m.name, &m);
    const auto find = [&](std::string_view name) -> const FeatureManifest* {
        const auto it = by_name.find(name);
        return it != by_name.end() ? it->second : nullptr;
    };
    static const std::regex include_re(R"re(^\s*#\s*include\s*([<"])([^>"]+)[>"])re");
    // A mutable variable at namespace/class scope: `static T name = ...;` (not const, constexpr or a function).
    static const std::regex global_re(R"re(^\s*static\s+(?!const\b|constexpr\b|inline\s+constexpr\b|assert\b|_Assert\b)[^()]*\b\w+\s*(=|;|\{).*$)re");

    for (const auto& dir : fs::directory_iterator(sources, ec)) {
        if (!dir.is_directory()) continue;
        const std::string feature = dir.path().filename().string();
        const FeatureManifest* self = find(feature);
        if (!self) {
            out.push_back(dir.path().string() + ": no feature.json for this feature in " + manifests.string() + " (every feature folder needs one)");
            continue;
        }
        for (const auto& file : fs::recursive_directory_iterator(dir.path())) {
            if (!file.is_regular_file() || !is_source(file.path())) continue;
            std::ifstream in(file.path());
            std::string line;
            for (int number = 1; std::getline(in, line); ++number) {
                const std::string where = file.path().string() + ":" + std::to_string(number) + ": ";
                std::smatch m;
                if (std::regex_search(line, m, include_re)) {
                    const std::string target = m[2];
                    if (target.find("/detail/") != std::string::npos || target.find("/src/") != std::string::npos) {
                        out.push_back(where + "internal engine header \"" + target + "\"; use the public headers (<astral/astral.hpp>)");
                    }
                    // features/<other>/<header>, or ../<other>/<header>
                    static const std::regex other_re(R"re((?:^|/)(?:features/)?([\w]+)/([\w.]+)$)re");
                    std::smatch o;
                    if (m[1] == "\"" && std::regex_search(target, o, other_re) && o[1] != feature) {
                        const std::string dep_name = o[1].str();
                        if (const FeatureManifest* dep = find(dep_name)) {
                            const bool depends = std::find(self->depends_on.begin(), self->depends_on.end(), dep->name) != self->depends_on.end();
                            const bool exported = std::find(dep->headers.begin(), dep->headers.end(), o[2].str()) != dep->headers.end();
                            if (!depends) out.push_back(where + "feature \"" + feature + "\" includes \"" + dep->name + "\" without depending on it (add it to \"depends_on\" in feature.json, or talk through an event)");
                            else if (!exported) out.push_back(where + "\"" + o[2].str() + "\" is not exported by feature \"" + dep->name + "\" (list it under \"exports.headers\" or use an event)");
                        }
                    }
                }
                if (std::regex_match(line, global_re) && line.find("astral-allow-global") == std::string::npos) {
                    out.push_back(where + "mutable global state (\"static\" variable): keep state in components or world resources (ADR-0023); add `// astral-allow-global: <reason>` if it is truly needed");
                }
            }
        }
    }
    return out;
}

} // namespace astral
