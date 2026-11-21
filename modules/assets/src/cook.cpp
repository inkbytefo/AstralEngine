#include <astral/assets/cook.hpp>
#include <astral/core/json.hpp>

#include "file_util.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <set>
#include <unordered_map>

namespace astral {

namespace {

constexpr std::string_view cache_name = ".astral-cook.json";
constexpr int cache_version = 1;
/// Bump when cooked formats or the cook's decisions change: everything is cooked again.
constexpr std::string_view cook_version = "astral-cook 1";

enum class Kind { Model, Document, Copy };

Kind kind_of(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".gltf" || ext == ".glb") return Kind::Model;
    if (ext == ".scene" || ext == ".prefab" || ext == ".json") return Kind::Document;
    return Kind::Copy;
}

const char* kind_name(Kind k) { return k == Kind::Model ? "model" : k == Kind::Document ? "document" : "copy"; }

std::string hex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

struct CacheEntry {
    std::string kind;
    std::string hash;
    std::vector<std::string> deps;
};

/// Hash of a file and its dependencies (nullopt if any is unreadable).
std::optional<std::uint64_t> content_hash(const std::filesystem::path& root, const std::string& rel, const std::vector<std::string>& deps,
                                          std::uint64_t seed) {
    std::uint64_t h = seed;
    for (const std::string* f = &rel; f; f = nullptr) {
        auto bytes = detail::read_file(root / *f);
        if (!bytes) return std::nullopt;
        h = detail::hash_bytes(*bytes, h);
    }
    for (const std::string& d : deps) {
        auto bytes = detail::read_file(root / d);
        if (!bytes) return std::nullopt;
        h = detail::hash_bytes(std::span(reinterpret_cast<const std::uint8_t*>(d.data()), d.size()), h);
        h = detail::hash_bytes(*bytes, h);
    }
    return h;
}

} // namespace

CookReport cook_directory(const CookOptions& o) {
    CookReport report;
    std::mutex report_mutex;
    auto error = [&](const std::string& message) {
        std::lock_guard lock(report_mutex);
        report.errors.push_back(message);
        if (o.logger) o.logger->log(LogLevel::Error, "cook: " + message);
    };
    std::error_code ec;
    if (!std::filesystem::is_directory(o.source, ec)) {
        error(o.source.string() + " is not a folder");
        return report;
    }
    std::filesystem::create_directories(o.output, ec);

    // Options that change outputs are part of every hash.
    const std::string options_key = std::string(cook_version) + (o.textures.compress ? ";bc" : ";rgba8") +
                                    ";q" + std::to_string(o.textures.bc7_quality);
    const std::uint64_t seed = detail::hash_bytes(std::span(reinterpret_cast<const std::uint8_t*>(options_key.data()), options_key.size()));

    std::unordered_map<std::string, CacheEntry> old_cache;
    if (auto text = detail::read_file(o.output / cache_name)) {
        auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
        if (doc && (*doc)["version"].as_number() == cache_version) {
            for (const auto& [path, e] : (*doc)["entries"].as_object()) {
                CacheEntry entry{e["kind"].as_string(), e["hash"].as_string(), {}};
                for (const Json& d : e["deps"].as_array()) entry.deps.push_back(d.as_string());
                old_cache.emplace(path, std::move(entry));
            }
        }
    }

    std::vector<std::string> sources;
    for (auto it = std::filesystem::recursive_directory_iterator(o.source, ec); !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.starts_with('.')) {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file()) sources.push_back(detail::relative_path(it->path(), o.source));
    }
    std::sort(sources.begin(), sources.end());

    std::unordered_map<std::string, CacheEntry> cache;
    auto up_to_date = [&](const std::string& rel, Kind kind, std::uint64_t* hash_out) {
        auto it = old_cache.find(rel);
        const std::vector<std::string> no_deps;
        const auto h = content_hash(o.source, rel, it != old_cache.end() ? it->second.deps : no_deps, seed);
        if (h) *hash_out = *h;
        return it != old_cache.end() && h && it->second.kind == kind_name(kind) && it->second.hash == hex(*h) &&
               std::filesystem::exists(o.output / rel);
    };

    // Models first: the buffers and images they read are folded in, not copied.
    struct ModelJob {
        std::string rel;
    };
    std::vector<ModelJob> models;
    std::set<std::string> deps;
    for (const std::string& rel : sources) {
        if (kind_of(rel) != Kind::Model) continue;
        std::uint64_t h = 0;
        if (up_to_date(rel, Kind::Model, &h)) {
            ++report.up_to_date;
            cache[rel] = old_cache[rel];
            deps.insert(cache[rel].deps.begin(), cache[rel].deps.end());
        } else {
            models.push_back({rel});
        }
    }
    auto cook_model = [&](const ModelJob& job) {
        std::string load_error;
        auto data = load_gltf(o.source / job.rel, &load_error);
        if (!data) return error(job.rel + ": " + load_error);
        CacheEntry entry{kind_name(Kind::Model), {}, {}};
        for (const auto& f : data->files) {
            const std::string d = detail::relative_path(f, o.source);
            if (d != job.rel) entry.deps.push_back(d);
        }
        std::sort(entry.deps.begin(), entry.deps.end());
        cook_model_images(*data, o.textures, nullptr); // models already run in parallel
        std::string write_error;
        if (!detail::write_file(o.output / job.rel, write_cooked_model(*data), &write_error)) return error(write_error);
        const auto h = content_hash(o.source, job.rel, entry.deps, seed);
        entry.hash = h ? hex(*h) : std::string();
        std::lock_guard lock(report_mutex);
        ++report.cooked;
        deps.insert(entry.deps.begin(), entry.deps.end());
        cache[job.rel] = std::move(entry);
    };
    if (o.jobs && models.size() > 1) {
        JobCounter counter;
        o.jobs->parallel_for(counter, models.size(), 1, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) cook_model(models[i]);
        });
        o.jobs->wait(counter);
    } else {
        for (const ModelJob& job : models) cook_model(job);
    }

    for (const std::string& rel : sources) {
        const Kind kind = kind_of(rel);
        if (kind == Kind::Model || deps.count(rel)) continue;
        std::uint64_t h = 0;
        if (up_to_date(rel, kind, &h)) {
            ++report.up_to_date;
            cache[rel] = old_cache[rel];
            continue;
        }
        std::string io_error;
        auto bytes = detail::read_file(o.source / rel, &io_error);
        if (!bytes) {
            error(io_error);
            continue;
        }
        std::vector<std::uint8_t> out;
        if (kind == Kind::Document && !Json::is_binary(*bytes)) {
            std::string parse_error;
            auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), &parse_error);
            if (!doc) {
                error(rel + ": " + parse_error);
                continue;
            }
            out = doc->to_binary();
        } else {
            out = std::move(*bytes);
        }
        if (!detail::write_file(o.output / rel, out, &io_error)) {
            error(io_error);
            continue;
        }
        if (kind == Kind::Document) ++report.cooked;
        else ++report.copied;
        cache[rel] = CacheEntry{kind_name(kind), hex(h), {}};
    }

    // Outputs whose source is gone (or became a dependency) go away.
    for (const auto& [rel, entry] : old_cache) {
        if (cache.count(rel)) continue;
        if (std::filesystem::remove(o.output / rel, ec)) ++report.removed;
    }

    Json doc = Json::object();
    doc["format"] = "astral.cook-cache";
    doc["version"] = cache_version;
    Json& entries = doc["entries"];
    entries = Json::object();
    std::vector<std::string> keys;
    for (const auto& [rel, entry] : cache) keys.push_back(rel);
    std::sort(keys.begin(), keys.end());
    for (const std::string& rel : keys) {
        const CacheEntry& e = cache[rel];
        if (e.hash.empty()) continue; // could not hash: cook again next time
        Json j = Json::object();
        j["kind"] = e.kind;
        j["hash"] = e.hash;
        if (!e.deps.empty()) {
            Json& list = j["deps"];
            list = Json::array();
            for (const std::string& d : e.deps) list.push_back(d);
        }
        entries[rel] = std::move(j);
    }
    const std::string text = doc.dump(1);
    std::string write_error;
    if (!detail::write_file(o.output / cache_name, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), &write_error)) {
        error(write_error);
    }
    if (o.logger) {
        o.logger->log(LogLevel::Info, "cook: " + std::to_string(report.cooked) + " cooked, " + std::to_string(report.copied) +
                                          " copied, " + std::to_string(report.up_to_date) + " up to date, " +
                                          std::to_string(report.removed) + " removed");
    }
    return report;
}

} // namespace astral
