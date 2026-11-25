#include <astral/app/feature.hpp>

#include "json_asset.hpp"

#include <algorithm>
#include <unordered_map>

namespace astral {

namespace {
std::vector<std::string> string_list(const Json& j, std::string_view key, std::string* error, std::string_view who) {
    std::vector<std::string> out;
    const Json* list = j.find(key);
    if (!list) return out;
    if (!list->is_array()) {
        if (error) *error = std::string(who) + ": \"" + std::string(key) + "\" must be a list of strings";
        return {};
    }
    for (const Json& item : list->as_array()) {
        if (!item.is_string()) {
            if (error) *error = std::string(who) + ": \"" + std::string(key) + "\" must be a list of strings";
            return {};
        }
        out.push_back(item.as_string());
    }
    return out;
}
} // namespace

std::optional<FeatureManifest> parse_feature_manifest(const Json& doc, std::string* error) {
    const auto fail = [&](std::string message) -> std::optional<FeatureManifest> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    if (!doc.is_object() || doc["format"].as_string() != "astral.feature") return fail("not a feature.json (\"format\" must be \"astral.feature\")");
    FeatureManifest m;
    m.name = doc["name"].as_string();
    if (m.name.empty()) return fail("feature.json needs a \"name\"");
    m.description = doc["description"].as_string();
    std::string problem;
    m.depends_on = string_list(doc, "depends_on", &problem, m.name);
    m.components = string_list(doc, "components", &problem, m.name);
    m.events = string_list(doc, "events", &problem, m.name);
    m.scripts = string_list(doc, "scripts", &problem, m.name);
    m.actions = string_list(doc, "actions", &problem, m.name);
    if (const Json* exports = doc.find("exports")) {
        if (!exports->is_object()) return fail(m.name + ": \"exports\" must be an object");
        m.headers = string_list(*exports, "headers", &problem, m.name);
    }
    if (!problem.empty()) return fail(problem);
    if (const Json* settings = doc.find("settings")) {
        if (!settings->is_object()) return fail(m.name + ": \"settings\" must be an object");
        m.settings = *settings;
    }
    return m;
}

void FeatureContext::declare(std::string_view name, std::vector<std::string>& seen, const std::vector<std::string>& allowed, const char* what) {
    seen.emplace_back(name);
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) {
        errors_.push_back("feature \"" + manifest.name + "\" registers " + std::string(name) + " but its feature.json does not list it under \"" + what +
                          "\" (add it: the manifest is the feature's public API)");
    }
}

double FeatureContext::number(std::string_view key, double fallback) const {
    const Json* v = manifest.settings.find(key);
    return v && v->is_number() ? v->as_number() : fallback;
}
std::string FeatureContext::text(std::string_view key, std::string_view fallback) const {
    const Json* v = manifest.settings.find(key);
    return v && v->is_string() ? v->as_string() : std::string(fallback);
}
bool FeatureContext::flag(std::string_view key, bool fallback) const {
    const Json* v = manifest.settings.find(key);
    return v && v->is_bool() ? v->as_bool() : fallback;
}

void FeatureSet::add(std::string name, Install install) { entries_.push_back({std::move(name), std::nullopt, std::move(install)}); }

void FeatureSet::add(FeatureManifest manifest, Install install) {
    std::string name = manifest.name;
    entries_.push_back({std::move(name), std::move(manifest), std::move(install)});
}

std::optional<std::vector<std::size_t>> FeatureSet::order(const std::vector<FeatureManifest>& manifests, std::string* error) {
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < manifests.size(); ++i) {
        if (!index.emplace(manifests[i].name, i).second) {
            if (error) *error = "feature \"" + manifests[i].name + "\" is added twice";
            return std::nullopt;
        }
    }
    for (const FeatureManifest& m : manifests) {
        for (const std::string& dep : m.depends_on) {
            if (!index.contains(dep)) {
                if (error) *error = "feature \"" + m.name + "\" depends on \"" + dep + "\", which is not part of the game (add it to the FeatureSet)";
                return std::nullopt;
            }
        }
    }
    std::vector<std::size_t> out;
    enum class Mark : std::uint8_t { New, Active, Done };
    std::vector<Mark> marks(manifests.size(), Mark::New);
    std::vector<std::string> path;
    const auto visit = [&](auto&& self, std::size_t i) -> bool {
        if (marks[i] == Mark::Done) return true;
        path.push_back(manifests[i].name);
        if (marks[i] == Mark::Active) {
            std::string cycle;
            const auto start = std::find(path.begin(), path.end() - 1, manifests[i].name);
            for (auto it = start; it != path.end(); ++it) cycle += (cycle.empty() ? "" : " -> ") + *it;
            if (error) *error = "feature dependency cycle: " + cycle;
            return false;
        }
        marks[i] = Mark::Active;
        for (const std::string& dep : manifests[i].depends_on) {
            if (!self(self, index[dep])) return false;
        }
        marks[i] = Mark::Done;
        path.pop_back();
        out.push_back(i);
        return true;
    };
    for (std::size_t i = 0; i < manifests.size(); ++i) {
        if (!visit(visit, i)) return std::nullopt;
    }
    return out;
}

bool FeatureSet::install(Context& ctx, std::string_view folder) {
    const auto fail = [&](const std::string& message) {
        ctx.log.log(LogLevel::Error, "features: " + message);
        return false;
    };
    manifests_.clear();
    for (Entry& e : entries_) {
        if (!e.manifest) {
            const std::string path = std::string(folder) + "/" + e.name + "/feature.json";
            std::string error;
            const auto bytes = ctx.assets->read(path, &error);
            if (!bytes) return fail(path + ": " + error);
            const auto doc = parse_json_asset(*bytes, &error);
            if (!doc) return fail(path + ": " + error);
            e.manifest = parse_feature_manifest(*doc, &error);
            if (!e.manifest) return fail(path + ": " + error);
            if (e.manifest->name != e.name) return fail(path + ": \"name\" is \"" + e.manifest->name + "\" but the feature was added as \"" + e.name + "\"");
        }
        manifests_.push_back(*e.manifest);
    }
    std::string error;
    const auto sequence = order(manifests_, &error);
    if (!sequence) return fail(error);
    for (const std::size_t i : *sequence) {
        const FeatureManifest& m = manifests_[i];
        FeatureContext fc(ctx, m);
        if (entries_[i].install) entries_[i].install(fc);
        if (!fc.errors().empty()) return fail(fc.errors().front());
        for (const std::string& name : m.components) ctx.api_owners[name] = m.name;
        for (const std::string& name : m.events) ctx.api_owners[name] = m.name;
        for (const std::string& path : m.actions) {
            if (!ctx.script_service().load_actions(path)) return fail("feature \"" + m.name + "\": cannot load " + path);
        }
        for (const std::string& path : m.scripts) {
            const auto problems = ctx.script_service().load_script(path);
            if (!problems.empty()) return fail("feature \"" + m.name + "\": " + problems.front());
        }
    }
    return true;
}

} // namespace astral
