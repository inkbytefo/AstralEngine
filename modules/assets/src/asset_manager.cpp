#include <astral/assets/asset_manager.hpp>
#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>

#include "file_util.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <cstdio>
#include <system_error>
#include <unordered_map>

namespace astral {

AssetId AssetId::from_path(std::string_view relative_path) noexcept {
    std::string normal;
    normal.reserve(relative_path.size());
    for (char c : relative_path) {
        if (c == '\\') c = '/';
        normal += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (normal.starts_with("./")) normal.erase(0, 2);
    const std::uint64_t h = stable_hash(normal);
    return {h == 0 ? 1 : h};
}

namespace {

using FileTime = std::filesystem::file_time_type;

FileTime mtime(const std::filesystem::path& p) {
    std::error_code ec;
    const FileTime t = std::filesystem::last_write_time(p, ec);
    return ec ? FileTime{} : t;
}

/// A load running (or finished) on a worker.
struct Pending {
    AssetId id;
    std::string path;
    std::filesystem::path full;
    JobCounter counter;
    std::optional<ModelData> data;
    std::string error;
};

} // namespace

struct AssetManager::Impl {
    AssetManagerDesc desc;
    struct Entry {
        std::string path;
        std::filesystem::path full;
        AssetState state = AssetState::Unknown;
        std::string error;
        std::unique_ptr<Model> model;
        std::unique_ptr<Document> document; ///< Set for documents (kind decided by the first load call).
        bool is_document = false;
        // Renderer resources this asset owns, in ModelData order (reused in place on reload).
        std::vector<MeshHandle> meshes;
        std::vector<MaterialHandle> materials;
        std::vector<TextureHandle> textures;
        std::vector<std::pair<std::filesystem::path, FileTime>> watched;
        bool reloading = false;
    };
    std::unordered_map<std::uint64_t, Entry> entries;
    std::vector<std::unique_ptr<Pending>> pending;
    std::uint32_t reload_count = 0;
    std::chrono::steady_clock::time_point last_poll = std::chrono::steady_clock::now();

    std::unique_ptr<Pack> pack;

    explicit Impl(const AssetManagerDesc& d) : desc(d) {
        if (!desc.pack.empty()) {
            std::string error;
            pack = Pack::open(desc.pack, &error);
            if (!pack) log_error("assets: " + error + " (every load will fail)");
            desc.hot_reload = false;
        }
    }

    /// Bytes of an asset, from the pack or the file under the root.
    std::optional<std::vector<std::uint8_t>> read_bytes(const std::string& path, const std::filesystem::path& full,
                                                        std::string* error) const {
        if (!desc.pack.empty()) {
            if (!pack) {
                *error = path + ": the asset pack could not be opened";
                return std::nullopt;
            }
            return pack->read(path, error);
        }
        auto bytes = detail::read_file(full, error);
        if (!bytes) *error = path + ": " + *error;
        return bytes;
    }

    /// A model from a cooked file (pack or loose) or a glTF source.
    std::optional<ModelData> load_model_data(const std::string& path, const std::filesystem::path& full, std::string* error) const {
        bool cooked = !desc.pack.empty();
        if (!cooked) { // peek at the header: a cooked file keeps the source's name
            std::ifstream in(full, std::ios::binary);
            std::uint8_t head[4] = {};
            in.read(reinterpret_cast<char*>(head), 4);
            cooked = in.gcount() == 4 && is_cooked_model(head);
        }
        if (!cooked) return load_gltf(full, error);
        auto bytes = read_bytes(path, full, error);
        if (!bytes) return std::nullopt;
        auto model = read_cooked_model(*bytes, error);
        if (!model && error) *error = path + ": " + *error;
        if (model && desc.pack.empty()) model->files = {full}; // hot reload watches the cooked file
        return model;
    }

    void log_error(const std::string& message) const {
        if (desc.logger) desc.logger->log(LogLevel::Error, message);
    }
    void log_warn(const std::string& message) const {
        if (desc.logger) desc.logger->log(LogLevel::Warn, message);
    }

    /// Documents load synchronously: read, parse, keep the last good version on failure.
    void read_document(Entry& e) {
        std::string error;
        if (auto bytes = read_bytes(e.path, e.full, &error)) {
            auto json = Json::is_binary(*bytes)
                            ? Json::from_binary(*bytes, &error)
                            : Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), &error);
            if (json) {
                const bool reload = e.document != nullptr;
                const std::uint32_t version = reload ? e.document->version + 1 : 1;
                e.document = std::make_unique<Document>(Document{std::move(*json), version});
                e.state = AssetState::Ready;
                e.error.clear();
                if (reload) ++reload_count;
            } else {
                error = e.path + ": " + error;
            }
        }
        if (!error.empty()) {
            if (e.document) {
                log_error("hot reload failed, keeping the previous version: " + error);
            } else {
                e.state = AssetState::Failed;
                e.error = error;
                log_error(error);
            }
        }
        if (desc.pack.empty()) e.watched = {{e.full, mtime(e.full)}};
    }

    void start(AssetId id, const std::string& path, const std::filesystem::path& full) {
        auto p = std::make_unique<Pending>();
        p->id = id;
        p->path = path;
        p->full = full;
        Pending* raw = p.get();
        auto job = [this, raw] { raw->data = load_model_data(raw->path, raw->full, &raw->error); };
        if (desc.jobs) desc.jobs->submit(raw->counter, job);
        else job();
        pending.push_back(std::move(p));
    }

    void destroy_resources(Entry& e) {
        if (!desc.renderer) return;
        for (auto h : e.meshes) desc.renderer->destroy(h);
        for (auto h : e.materials) desc.renderer->destroy(h);
        for (auto h : e.textures) desc.renderer->destroy(h);
        e.meshes.clear();
        e.materials.clear();
        e.textures.clear();
    }

    static bool same_structure(const Model& m, const ModelData& d) {
        if (m.nodes.size() != d.nodes.size() || m.roots != d.roots) return false;
        for (std::size_t i = 0; i < m.nodes.size(); ++i) {
            if (m.nodes[i].children != d.nodes[i].children || m.nodes[i].pieces.size() != d.nodes[i].primitives.size()) return false;
        }
        return true;
    }

    /// Main thread: data -> renderer resources -> Model. Reuses handles when the shapes allow it.
    void finish(Entry& e, ModelData& data) {
        Renderer* r = desc.renderer;
        const bool reload = e.model != nullptr;
        // Textures, then materials that reference them, then meshes; update in place when counts match.
        const bool keep_textures = e.textures.size() == data.images.size();
        const bool keep_materials = e.materials.size() == data.materials.size();
        const bool keep_meshes = e.meshes.size() == data.primitives.size();
        std::vector<TextureHandle> textures;
        std::vector<MaterialHandle> materials;
        std::vector<MeshHandle> meshes;
        if (r) {
            for (std::size_t i = 0; i < data.images.size(); ++i) {
                ImageData& img = data.images[i].image;
                if (!img.levels.empty() && !r->supports(img.format)) img = decompress_image(img); // GPU without BC
                if (img.width == 0) { // declared but unused by any material
                    textures.push_back(keep_textures ? e.textures[i] : TextureHandle{});
                    continue;
                }
                if (keep_textures && e.textures[i].valid() && r->update_texture(e.textures[i], img)) {
                    textures.push_back(e.textures[i]);
                } else {
                    textures.push_back(r->create_texture(img));
                    if (keep_textures) r->destroy(e.textures[i]);
                }
            }
            auto tex = [&](int i) { return i >= 0 && static_cast<std::size_t>(i) < textures.size() ? textures[static_cast<std::size_t>(i)] : TextureHandle{}; };
            for (std::size_t i = 0; i < data.materials.size(); ++i) {
                const ModelMaterial& m = data.materials[i];
                const MaterialDesc desc_m{.base_color = m.base_color, .roughness = m.roughness, .metallic = m.metallic,
                                          .emissive = m.emissive, .base_color_texture = tex(m.base_color_image),
                                          .metallic_roughness_texture = tex(m.metallic_roughness_image),
                                          .normal_texture = tex(m.normal_image), .emissive_texture = tex(m.emissive_image),
                                          .normal_scale = m.normal_scale};
                if (keep_materials && r->update_material(e.materials[i], desc_m)) materials.push_back(e.materials[i]);
                else materials.push_back(r->create_material(desc_m));
            }
            for (std::size_t i = 0; i < data.primitives.size(); ++i) {
                if (keep_meshes && r->update_mesh(e.meshes[i], data.primitives[i].mesh)) meshes.push_back(e.meshes[i]);
                else meshes.push_back(r->create_mesh(data.primitives[i].mesh));
            }
            // Old resources not carried over.
            if (!keep_textures) for (auto h : e.textures) r->destroy(h);
            if (!keep_materials) for (auto h : e.materials) r->destroy(h);
            if (!keep_meshes) for (auto h : e.meshes) r->destroy(h);
        }
        e.textures = std::move(textures);
        e.materials = std::move(materials);
        e.meshes = std::move(meshes);

        auto model = std::make_unique<Model>();
        model->roots = data.roots;
        for (const ModelNode& n : data.nodes) {
            Model::Node node{n.name, n.translation, n.rotation, n.scale, {}, n.children};
            for (std::uint32_t p : n.primitives) {
                const int mat = data.primitives[p].material;
                node.pieces.push_back({r ? e.meshes[p] : MeshHandle{},
                                       r && mat >= 0 ? e.materials[static_cast<std::size_t>(mat)] : MaterialHandle{}});
            }
            model->nodes.push_back(std::move(node));
        }
        if (reload) {
            // Spawned entities hold mesh/material handles: they go stale if the tree changed or if
            // those handles had to be recreated.
            const bool handles_changed = r && (!keep_meshes || !keep_materials);
            model->version = same_structure(*e.model, data) && !handles_changed ? e.model->version : e.model->version + 1;
            ++reload_count;
        }
        e.model = std::move(model);
        e.state = AssetState::Ready;
        e.error.clear();
        e.watched.clear();
        for (const auto& f : data.files) e.watched.emplace_back(f, mtime(f));
        for (const auto& w : data.warnings) log_warn(e.path + ": " + w);
    }

    void collect(bool block) {
        for (auto it = pending.begin(); it != pending.end();) {
            Pending& p = **it;
            if (!p.counter.done()) {
                if (!block) {
                    ++it;
                    continue;
                }
                desc.jobs->wait(p.counter);
            }
            auto found = entries.find(p.id.value);
            if (found != entries.end()) {
                Entry& e = found->second;
                e.reloading = false;
                if (p.data) {
                    finish(e, *p.data);
                } else if (e.model) { // failed reload: keep the last good version
                    log_error("hot reload failed, keeping the previous version: " + p.error);
                    for (auto& [file, time] : e.watched) time = mtime(file); // don't retry until edited again
                } else {
                    e.state = AssetState::Failed;
                    e.error = p.error;
                    log_error(p.error);
                    e.watched = {{e.full, mtime(e.full)}}; // retry when the file appears/changes
                }
            }
            it = pending.erase(it);
        }
    }

    void poll_files() {
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - last_poll).count() < desc.reload_poll_seconds) return;
        last_poll = now;
        for (auto& [key, e] : entries) {
            if (e.reloading || e.state == AssetState::Loading) continue;
            const bool changed = std::any_of(e.watched.begin(), e.watched.end(),
                                             [](const auto& w) { return mtime(w.first) != w.second; });
            if (!changed) continue;
            if (e.is_document) {
                read_document(e);
                continue;
            }
            e.reloading = true;
            if (e.state == AssetState::Failed) e.state = AssetState::Loading;
            start(AssetId{key}, e.path, e.full);
        }
    }
};

AssetManager::AssetManager(const AssetManagerDesc& desc) : impl_(std::make_unique<Impl>(desc)) {}

AssetManager::~AssetManager() {
    auto& m = *impl_;
    for (auto& p : m.pending) {
        if (m.desc.jobs) m.desc.jobs->wait(p->counter);
    }
    for (auto& [key, e] : m.entries) m.destroy_resources(e);
}

AssetId AssetManager::load_model(std::string_view relative_path) {
    auto& m = *impl_;
    const AssetId id = AssetId::from_path(relative_path);
    auto [it, inserted] = m.entries.try_emplace(id.value);
    if (!inserted) return id; // already loaded or loading
    Impl::Entry& e = it->second;
    e.path = std::string(relative_path);
    e.full = m.desc.root / relative_path;
    e.state = AssetState::Loading;
    m.start(id, e.path, e.full);
    return id;
}

AssetId AssetManager::load_document(std::string_view relative_path) {
    auto& m = *impl_;
    const AssetId id = AssetId::from_path(relative_path);
    auto [it, inserted] = m.entries.try_emplace(id.value);
    if (!inserted) return id;
    Impl::Entry& e = it->second;
    e.path = std::string(relative_path);
    e.full = m.desc.root / relative_path;
    e.is_document = true;
    m.read_document(e);
    return id;
}

const Document* AssetManager::document(AssetId id) const noexcept {
    auto it = impl_->entries.find(id.value);
    return it == impl_->entries.end() ? nullptr : it->second.document.get();
}

AssetState AssetManager::state(AssetId id) const noexcept {
    auto it = impl_->entries.find(id.value);
    return it == impl_->entries.end() ? AssetState::Unknown : it->second.state;
}

const Model* AssetManager::model(AssetId id) const noexcept {
    auto it = impl_->entries.find(id.value);
    return it == impl_->entries.end() || it->second.state != AssetState::Ready ? nullptr : it->second.model.get();
}

std::string_view AssetManager::error(AssetId id) const noexcept {
    auto it = impl_->entries.find(id.value);
    return it == impl_->entries.end() ? std::string_view{} : std::string_view(it->second.error);
}

std::string_view AssetManager::path(AssetId id) const noexcept {
    auto it = impl_->entries.find(id.value);
    return it == impl_->entries.end() ? std::string_view{} : std::string_view(it->second.path);
}

void AssetManager::update() {
    impl_->collect(false);
    if (impl_->desc.hot_reload) impl_->poll_files();
}

void AssetManager::wait_all() { impl_->collect(true); }

std::uint32_t AssetManager::reloads() const noexcept { return impl_->reload_count; }

bool AssetManager::from_pack() const noexcept { return impl_->pack != nullptr; }

std::optional<std::vector<std::uint8_t>> AssetManager::read(std::string_view relative_path, std::string* error) const {
    std::string message;
    auto bytes = impl_->read_bytes(std::string(relative_path), impl_->desc.root / relative_path, &message);
    if (!bytes && error) *error = message;
    return bytes;
}

} // namespace astral
