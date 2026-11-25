#include <astral/assets/cook.hpp>
#include <astral/assets/terrain.hpp>
#include <astral/core/json.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/world/level_instance.hpp>
#include <astral/world/map.hpp>

#include "file_util.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

namespace astral {

namespace {

constexpr std::string_view cache_name = ".astral-cook.json";
constexpr int cache_version = 2; // 2: world entries list their cell outputs
/// Bump when cooked formats or the cook's decisions change: everything is cooked again.
constexpr std::string_view cook_version = "astral-cook 7"; // 7: AJB uint64 tag, typed shader words (ADR-0041/0042)

enum class Kind { Model, World, Terrain, Shader, Document, Script, Copy };

Kind kind_of(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".gltf" || ext == ".glb") return Kind::Model;
    if (ext == ".world") return Kind::World;
    if (ext == ".aterrain") return Kind::Terrain;
    if (ext == ".ashader") return Kind::Shader;
    if (ext == ".luau") return Kind::Script;
    if (ext == ".scene" || ext == ".prefab" || ext == ".json" || ext == ".material" || ext == ".vfx") return Kind::Document;
    return Kind::Copy;
}

const char* kind_name(Kind k) {
    switch (k) {
    case Kind::Model: return "model";
    case Kind::World: return "world";
    case Kind::Terrain: return "terrain";
    case Kind::Shader: return "shader";
    case Kind::Document: return "document";
    case Kind::Script: return "script";
    case Kind::Copy: break;
    }
    return "copy";
}

/// "maps/island.world" -> "maps/island" (the entity folder and cell folder sit next to it).
std::string world_stem(const std::string& rel) {
    return rel.substr(0, rel.size() - std::string_view(".world").size());
}

std::string hex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

struct CacheEntry {
    std::string kind;
    std::string hash; ///< Empty: cook again next time (could not hash, or the cook had problems).
    std::vector<std::string> deps;
    std::vector<std::string> outputs; ///< Generated files besides `rel` itself (world cells), output-relative.
};

using CacheMap = std::unordered_map<std::string, CacheEntry>;

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

struct CookSession {
    const CookOptions& o;
    CookReport& report;
    std::mutex report_mutex;
    std::uint64_t seed = 0;
    CacheMap old_cache;
    CacheMap cache;
    std::vector<std::string> sources;
    std::set<std::string> deps;

    void error(const std::string& message) {
        std::lock_guard lock(report_mutex);
        report.errors.push_back(message);
        if (o.logger) o.logger->log(LogLevel::Error, "cook: " + message);
    }

    bool up_to_date(const std::string& rel, Kind kind, std::uint64_t* hash_out) const {
        auto it = old_cache.find(rel);
        const std::vector<std::string> no_deps;
        const auto h = content_hash(o.source, rel, it != old_cache.end() ? it->second.deps : no_deps, seed);
        if (h) *hash_out = *h;
        return it != old_cache.end() && h && it->second.kind == kind_name(kind) && it->second.hash == hex(*h) &&
               std::filesystem::exists(o.output / rel);
    }
};

CacheMap read_cook_cache(const std::filesystem::path& path) {
    CacheMap old_cache;
    if (auto text = detail::read_file(path)) {
        auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
        if (doc && (*doc)["version"].as_number() == cache_version) {
            for (const auto& [rel, e] : (*doc)["entries"].as_object()) {
                CacheEntry entry{e["kind"].as_string(), e["hash"].as_string(), {}, {}};
                for (const Json& d : e["deps"].as_array()) entry.deps.push_back(d.as_string());
                for (const Json& d : e["outputs"].as_array()) entry.outputs.push_back(d.as_string());
                old_cache.emplace(rel, std::move(entry));
            }
        }
    }
    return old_cache;
}

/// Every source file under `root`, sorted. `complete` is false when the walk hit an error: the list
/// is then partial and must not be used to decide which outputs are stale.
std::vector<std::string> scan_source_files(const std::filesystem::path& root, bool& complete) {
    std::vector<std::string> sources;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.starts_with('.')) {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file()) sources.push_back(detail::relative_path(it->path(), root));
    }
    complete = !ec;
    std::sort(sources.begin(), sources.end());
    return sources;
}

void cook_models(CookSession& s) {
    std::vector<std::string> models;
    for (const std::string& rel : s.sources) {
        if (kind_of(rel) != Kind::Model) continue;
        std::uint64_t h = 0;
        if (s.up_to_date(rel, Kind::Model, &h)) {
            ++s.report.up_to_date;
            s.cache[rel] = s.old_cache[rel];
            s.deps.insert(s.cache[rel].deps.begin(), s.cache[rel].deps.end());
        } else {
            models.push_back(rel);
        }
    }
    auto cook_one = [&](const std::string& rel) {
        std::string load_error;
        auto data = load_gltf(s.o.source / rel, &load_error);
        if (!data) return s.error(rel + ": " + load_error);
        CacheEntry entry{kind_name(Kind::Model), {}, {}, {}};
        for (const auto& f : data->files) {
            const std::string d = detail::relative_path(f, s.o.source);
            if (d != rel) entry.deps.push_back(d);
        }
        std::sort(entry.deps.begin(), entry.deps.end());
        cook_model_images(*data, s.o.textures, nullptr); // models already run in parallel
        std::string write_error;
        if (!detail::write_file(s.o.output / rel, write_cooked_model(*data), &write_error)) return s.error(write_error);
        const auto h = content_hash(s.o.source, rel, entry.deps, s.seed);
        entry.hash = h ? hex(*h) : std::string();
        std::lock_guard lock(s.report_mutex);
        ++s.report.cooked;
        s.deps.insert(entry.deps.begin(), entry.deps.end());
        s.cache[rel] = std::move(entry);
    };
    if (s.o.jobs && models.size() > 1) {
        JobCounter counter;
        s.o.jobs->parallel_for(counter, models.size(), 1, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) cook_one(models[i]);
        });
        s.o.jobs->wait(counter);
    } else {
        for (const std::string& rel : models) cook_one(rel);
    }
}

std::optional<Json> read_cook_json(CookSession& s, const std::string& file) {
    std::string io_error;
    auto bytes = detail::read_file(s.o.source / file, &io_error);
    if (!bytes) {
        s.error(io_error);
        return std::nullopt;
    }
    if (Json::is_binary(*bytes)) return Json::from_binary(*bytes);
    std::string parse_error;
    auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), &parse_error);
    if (!doc) s.error(file + ": " + parse_error);
    return doc;
}

MapSource expand_world_for_cook(CookSession& s, const MapSource& raw) {
    TypeRegistry types;
    register_world_types(types);
    std::unordered_map<std::string, Json> scene_cache;
    const DocumentSource loader = [&](std::string_view rel) -> SourceDocument {
        const std::string key(rel);
        if (auto it = scene_cache.find(key); it != scene_cache.end()) return {&it->second, 1, {}};
        auto doc = read_cook_json(s, key);
        if (!doc) return {nullptr, 0, "cannot read " + key};
        auto [ins, _] = scene_cache.emplace(key, std::move(*doc));
        return {&ins->second, 1, {}};
    };
    return expand_map_level_instances(raw, types, loader);
}

void cook_one_world(CookSession& s, const std::string& rel) {
    const std::string stem = world_stem(rel);
    const std::string folder = stem + ".entities/";
    CacheEntry entry{kind_name(Kind::World), {}, {}, {}};
    for (const std::string& f : s.sources) {
        if (f.starts_with(folder)) entry.deps.push_back(f);
    }
    s.deps.insert(entry.deps.begin(), entry.deps.end());
    const auto h = content_hash(s.o.source, rel, entry.deps, s.seed);
    auto old = s.old_cache.find(rel);
    const auto outputs_exist = [&](const CacheEntry& e) {
        if (!std::filesystem::exists(s.o.output / rel)) return false;
        return std::all_of(e.outputs.begin(), e.outputs.end(),
                           [&](const std::string& out) { return std::filesystem::is_regular_file(s.o.output / out); });
    };
    if (h && old != s.old_cache.end() && old->second.kind == entry.kind && old->second.hash == hex(*h) &&
        outputs_exist(old->second)) {
        ++s.report.up_to_date;
        s.cache[rel] = old->second;
        return;
    }
    auto world_doc = read_cook_json(s, rel);
    if (!world_doc) return;
    std::string map_error;
    auto map = parse_map(*world_doc, &map_error);
    if (!map) return s.error(rel + ": " + map_error);

    bool clean = true;
    std::vector<EntityFile> files;
    for (const std::string& f : entry.deps) {
        if (auto doc = read_cook_json(s, f)) files.push_back({f, std::move(*doc)});
        else clean = false;
    }
    const MapSource raw_source = read_map_source(*map, files);
    const MapSource source = expand_world_for_cook(s, raw_source);
    for (const std::string& e : source.errors) {
        s.error(e);
        clean = false;
    }
    const CookedMap cooked = cook_map(source);
    const std::filesystem::path cells_dir = s.o.output / (stem + ".cells");
    std::error_code ec;
    std::filesystem::remove_all(cells_dir, ec);
    std::string io_error;
    bool written = detail::write_file(s.o.output / rel, cooked.index.to_binary(), &io_error);
    for (const CookedCell& cell : cooked.cells) {
        written = written && detail::write_file(cells_dir / (cell.name + ".cell"), cell.document.to_binary(), &io_error);
        entry.outputs.push_back(stem + ".cells/" + cell.name + ".cell");
    }
    if (!written) return s.error(io_error);
    ++s.report.cooked;
    if (clean && h) entry.hash = hex(*h); // problems: cook (and report) again next time
    s.cache[rel] = std::move(entry);
}

bool validate_shader(CookSession& s, const std::string& rel, const std::vector<std::uint8_t>& bytes) {
    // The game compiles shaders itself; the cook makes sure they compile (errors with .ashader lines).
    const ShaderParse parsed = parse_ashader(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    std::vector<ShaderError> problems = parsed.errors;
    if (problems.empty() && parsed.source.type == ShaderType::Surface) problems = compile_surface_shader(parsed.source, rel).errors;
    else if (problems.empty() && (parsed.source.type == ShaderType::Post || parsed.source.type == ShaderType::Sky ||
               parsed.source.type == ShaderType::Volume || parsed.source.type == ShaderType::Particle)) {
        problems = compile_screen_shader(parsed.source, rel).errors;
    }
    for (const ShaderError& e : problems) s.error(rel + ":" + std::to_string(e.line) + ": " + e.message);
    return problems.empty();
}

std::string terrain_tile_path(const std::string& manifest, const TerrainTileSource& tile) {
    return manifest + ".tiles/" + std::to_string(tile.x) + "_" + std::to_string(tile.z) + ".aheight";
}

using TileAddress = std::pair<std::int64_t, std::int64_t>;
using TerrainSamples = std::map<TileAddress, std::vector<std::uint16_t>>;

bool terrain_edges_match(const TerrainSamples& tiles, std::size_t n) {
    const std::size_t edge = n - 1;
    for (const auto& [address, tile] : tiles) {
        const auto [x, z] = address;
        const auto east = tiles.find({x + 1, z});
        const auto south = tiles.find({x, z + 1});
        const auto southeast = tiles.find({x + 1, z + 1});
        const auto southwest = tiles.find({x - 1, z + 1});
        if (southeast != tiles.end() && tile.back() != southeast->second.front()) return false;
        if (southwest != tiles.end() && tile[edge * n] != southwest->second[edge]) return false;
        for (std::size_t row = 0; row < n; ++row) {
            if (east != tiles.end() && tile[row * n + edge] != east->second[row * n]) return false;
            if (south != tiles.end() && tile[edge * n + row] != south->second[row]) return false;
        }
    }
    return true;
}

void cook_one_terrain(CookSession& s, const std::string& rel) {
    std::string error;
    const auto bytes = detail::read_file(s.o.source / rel, &error);
    if (!bytes) return s.error(error);
    const auto manifest = read_terrain_manifest(*bytes, &error);
    if (!manifest) return s.error(rel + ": " + error);

    const std::string folder = rel + ".tiles";
    if (std::filesystem::exists(s.o.source / folder)) return s.error(rel + ": reserved cooked tile folder exists in source");
    std::uint64_t hash = 0;
    if (s.up_to_date(rel, Kind::Terrain, &hash) &&
        std::all_of(manifest->tiles.begin(), manifest->tiles.end(), [&](const TerrainTileSource& tile) {
            return std::filesystem::is_regular_file(s.o.output / terrain_tile_path(rel, tile));
        })) {
        ++s.report.up_to_date;
        s.cache[rel] = s.old_cache[rel];
        return;
    }

    TerrainSamples samples;
    std::set<std::string> dependencies;
    for (const auto& tile : manifest->tiles) {
        const auto image = detail::read_file(s.o.source / tile.file, &error);
        if (!image) return s.error(rel + ": " + error);
        auto heights = read_terrain_tile(*image, manifest->samples, &error);
        if (!heights) return s.error(rel + ": " + tile.file + ": " + error);
        samples.emplace(TileAddress{tile.x, tile.z}, std::move(*heights));
        dependencies.insert(tile.file);
    }
    if (!terrain_edges_match(samples, manifest->samples)) return s.error(rel + ": terrain tiles disagree at a shared boundary");

    CacheEntry entry{kind_name(Kind::Terrain), {}, {dependencies.begin(), dependencies.end()}, {}};
    const auto input_hash = content_hash(s.o.source, rel, entry.deps, s.seed);
    if (!input_hash) return s.error(rel + ": could not hash terrain dependencies");
    entry.hash = hex(*input_hash);

    Json doc = Json::object();
    doc["format"] = "astral.terrain";
    doc["version"] = 1;
    doc["samples"] = manifest->samples;
    doc["spacing_m"] = manifest->spacing_m;
    doc["min_height_m"] = manifest->min_height_m;
    doc["max_height_m"] = manifest->max_height_m;
    doc["tiles"] = Json::array();
    std::error_code ec;
    std::filesystem::remove_all(s.o.output / folder, ec);
    if (ec) return s.error(rel + ": " + ec.message());
    for (const auto& tile : manifest->tiles) {
        const std::string output = terrain_tile_path(rel, tile);
        const auto encoded = write_terrain_tile(samples.at({tile.x, tile.z}));
        if (!detail::write_file(s.o.output / output, encoded, &error)) {
            s.error(error);
            std::filesystem::remove_all(s.o.output / folder, ec);
            return;
        }
        Json item = Json::object();
        item["x"] = tile.x;
        item["z"] = tile.z;
        item["file"] = output;
        doc["tiles"].push_back(std::move(item));
    }
    if (!detail::write_file(s.o.output / rel, doc.to_binary(), &error)) {
        s.error(error);
        std::filesystem::remove_all(s.o.output / folder, ec);
        return;
    }
    ++s.report.cooked;
    s.cache[rel] = std::move(entry);
}

/// Source files that documents (.material, .scene, .prefab, .vfx, .json) name by path: the game opens
/// them at run time, so they ship as files even when a model also embeds them (a PNG shared by a
/// glTF and a .material must not exist only inside the cooked model).
std::set<std::string> runtime_references(const CookSession& s) {
    std::set<std::string> refs;
    std::vector<const Json*> stack;
    for (const std::string& rel : s.sources) {
        if (kind_of(rel) != Kind::Document) continue;
        const auto bytes = detail::read_file(s.o.source / rel);
        if (!bytes || Json::is_binary(*bytes)) continue;
        const auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        if (!doc) continue; // reported when the document itself is cooked
        stack.assign(1, &*doc);
        while (!stack.empty()) {
            const Json* j = stack.back();
            stack.pop_back();
            if (j->is_string()) {
                if (std::binary_search(s.sources.begin(), s.sources.end(), j->as_string())) refs.insert(j->as_string());
            } else if (j->is_array()) {
                for (const Json& item : j->as_array()) stack.push_back(&item);
            } else if (j->is_object()) {
                for (const auto& [key, value] : j->as_object()) stack.push_back(&value);
            }
        }
    }
    return refs;
}

void cook_remaining_files(CookSession& s) {
    const std::set<std::string> runtime_refs = runtime_references(s);
    for (const std::string& rel : s.sources) {
        const Kind kind = kind_of(rel);
        if (kind == Kind::Model || kind == Kind::World || kind == Kind::Terrain) continue;
        if (s.deps.count(rel) && !runtime_refs.count(rel)) continue; // only ever read through its model
        std::uint64_t h = 0;
        if (s.up_to_date(rel, kind, &h)) {
            ++s.report.up_to_date;
            s.cache[rel] = s.old_cache[rel];
            continue;
        }
        std::string io_error;
        auto bytes = detail::read_file(s.o.source / rel, &io_error);
        if (!bytes) {
            s.error(io_error);
            continue;
        }
        std::vector<std::uint8_t> out;
        if (kind == Kind::Shader) {
            if (!validate_shader(s, rel, *bytes)) continue;
            out = std::move(*bytes);
        } else if (kind == Kind::Script && s.o.compile_script) {
            std::string compile_error;
            if (!s.o.compile_script(rel, *bytes, out, &compile_error)) {
                s.error(compile_error);
                continue;
            }
        } else if (kind == Kind::Document && !Json::is_binary(*bytes)) {
            std::string parse_error;
            auto doc = Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), &parse_error);
            if (!doc) {
                s.error(rel + ": " + parse_error);
                continue;
            }
            out = doc->to_binary();
        } else {
            out = std::move(*bytes);
        }
        if (!detail::write_file(s.o.output / rel, out, &io_error)) {
            s.error(io_error);
            continue;
        }
        if (kind == Kind::Document || (kind == Kind::Script && s.o.compile_script)) ++s.report.cooked;
        else ++s.report.copied;
        s.cache[rel] = CacheEntry{kind_name(kind), hex(h), {}, {}};
    }
}

void write_cook_cache(CookSession& s) {
    Json doc = Json::object();
    doc["format"] = "astral.cook-cache";
    doc["version"] = cache_version;
    Json& entries = doc["entries"];
    entries = Json::object();
    std::vector<std::string> keys;
    for (const auto& [rel, entry] : s.cache) keys.push_back(rel);
    std::sort(keys.begin(), keys.end());
    for (const std::string& rel : keys) {
        const CacheEntry& e = s.cache[rel];
        // Entries with an empty hash are kept too (they cook again next time): the next cook must still
        // know their outputs exist, so it can remove them when the source goes away.
        Json j = Json::object();
        j["kind"] = e.kind;
        j["hash"] = e.hash;
        if (!e.deps.empty()) {
            Json& list = j["deps"];
            list = Json::array();
            for (const std::string& d : e.deps) list.push_back(d);
        }
        if (!e.outputs.empty()) {
            Json& list = j["outputs"];
            list = Json::array();
            for (const std::string& d : e.outputs) list.push_back(d);
        }
        entries[rel] = std::move(j);
    }
    const std::string text = doc.dump(1);
    std::string write_error;
    if (!detail::write_file(s.o.output / cache_name, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), &write_error)) {
        s.error(write_error);
    }
}

} // namespace

CookReport cook_directory(const CookOptions& o) {
    CookReport report;
    CookSession s{o, report, {}, 0, {}, {}, {}, {}};
    std::error_code ec;
    if (!std::filesystem::is_directory(o.source, ec)) {
        s.error(o.source.string() + " is not a folder");
        return report;
    }
    std::filesystem::create_directories(o.output, ec);

    // Options that change outputs are part of every hash.
    const std::string options_key = std::string(cook_version) + (o.textures.compress ? ";bc" : ";rgba8") +
                                    ";q" + std::to_string(o.textures.bc7_quality);
    s.seed = detail::hash_bytes(std::span(reinterpret_cast<const std::uint8_t*>(options_key.data()), options_key.size()));
    s.old_cache = read_cook_cache(o.output / cache_name);
    bool scan_complete = true;
    s.sources = scan_source_files(o.source, scan_complete);
    if (!scan_complete) s.error("cannot list every source file; stale outputs are kept this time");

    cook_models(s);
    for (const std::string& rel : s.sources) {
        if (kind_of(rel) == Kind::World) cook_one_world(s, rel);
        if (kind_of(rel) == Kind::Terrain) cook_one_terrain(s, rel);
    }
    cook_remaining_files(s);

    // Outputs whose source is gone (or became a dependency) go away. A source that is still there but
    // failed to cook keeps its last good output (and its cache entry, so it cooks again next time).
    for (const auto& [rel, entry] : s.old_cache) {
        if (!scan_complete) break;
        if (s.cache.count(rel)) continue;
        if (std::binary_search(s.sources.begin(), s.sources.end(), rel) && !s.deps.count(rel)) {
            CacheEntry kept = entry;
            kept.hash.clear();
            s.cache[rel] = std::move(kept);
            continue;
        }
        if (std::filesystem::remove(o.output / rel, ec)) ++report.removed;
        if (entry.kind == kind_name(Kind::World)) std::filesystem::remove_all(o.output / (world_stem(rel) + ".cells"), ec);
        if (entry.kind == kind_name(Kind::Terrain)) std::filesystem::remove_all(o.output / (rel + ".tiles"), ec);
    }
    write_cook_cache(s);
    if (o.logger) {
        o.logger->log(LogLevel::Info, "cook: " + std::to_string(report.cooked) + " cooked, " + std::to_string(report.copied) +
                                          " copied, " + std::to_string(report.up_to_date) + " up to date, " +
                                          std::to_string(report.removed) + " removed");
    }
    return report;
}

} // namespace astral
