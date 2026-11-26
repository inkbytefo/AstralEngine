#include <astral/editor/document.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace astral::editor {

std::uint64_t hash_document_text(std::string_view text) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}

std::string serialize_document(const Json& doc) {
    return doc.dump(2) + "\n";
}

std::optional<std::string> LocalEditorFileSystem::read_text(const std::filesystem::path& path, std::string* error) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path.string();
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool LocalEditorFileSystem::write_atomic(const std::filesystem::path& path, std::string_view text, std::string* error) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (error) *error = "cannot create directory for " + path.string() + ": " + ec.message();
        return false;
    }
    std::filesystem::path tmp = path;
    tmp += ".astral_tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "cannot open temp file " + tmp.string();
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(tmp, ec);
            if (error) *error = "write failed for " + tmp.string();
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        // On Windows, rename over an existing file can require replacing via remove/rename fallback if locked; try clean swap.
        std::error_code rm_ec;
        if (std::filesystem::exists(path, rm_ec)) {
            std::filesystem::path backup = path;
            backup += ".astral_bak";
            std::filesystem::rename(path, backup, rm_ec);
            if (!rm_ec) {
                ec.clear();
                std::filesystem::rename(tmp, path, ec);
                if (!ec) {
                    std::filesystem::remove(backup, rm_ec);
                    return true;
                }
                std::filesystem::rename(backup, path, rm_ec);
            }
        }
        std::filesystem::remove(tmp, rm_ec);
        if (error) *error = "atomic rename failed for " + path.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool LocalEditorFileSystem::remove(const std::filesystem::path& path, std::string* error) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        if (error) *error = "cannot remove " + path.string() + ": " + ec.message();
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> LocalEditorFileSystem::list_files(const std::filesystem::path& root,
                                                                     const std::vector<std::string>& extensions) const {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return out;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if (extensions.empty() || std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
            out.push_back(entry.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

const Json* EditorDocument::find_record(std::uint32_t record_id) const noexcept {
    const Json* list = root.find("entities");
    if (!list || !list->is_array()) return nullptr;
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& rec = (*list)[i];
        if (static_cast<std::uint32_t>(rec["id"].as_number(0.0)) == record_id) return &rec;
    }
    return nullptr;
}

bool EditorDocument::replace_record(std::uint32_t record_id, Json updated) {
    const Json* list = root.find("entities");
    if (!list || !list->is_array()) return false;
    bool replaced = false;
    Json next = Json::array();
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& rec = (*list)[i];
        if (static_cast<std::uint32_t>(rec["id"].as_number(0.0)) == record_id) {
            next.push_back(std::move(updated));
            replaced = true;
        } else {
            next.push_back(rec);
        }
    }
    if (replaced) root["entities"] = std::move(next);
    return replaced;
}

std::uint32_t EditorDocument::next_record_id() const noexcept {
    std::uint32_t highest = 0;
    if (const Json* list = root.find("entities"); list && list->is_array()) {
        for (std::size_t i = 0; i < list->size(); ++i) {
            const auto id = static_cast<std::uint32_t>((*list)[i]["id"].as_number(0.0));
            highest = std::max(highest, id);
        }
    }
    return highest + 1;
}

void EditorDocument::bump_revision() {
    ++revision.number;
    revision.hash = hash_document_text(serialize_document(root));
}

std::optional<EditorDocument> load_editor_document(const IEditorFileSystem& fs,
                                                   const std::filesystem::path& path,
                                                   std::string* error) {
    const auto text = fs.read_text(path, error);
    if (!text) return std::nullopt;
    std::string parse_err;
    auto parsed = Json::parse(*text, &parse_err);
    if (!parsed || !parsed->is_object()) {
        if (error) *error = path.string() + ": invalid JSON (" + parse_err + ")";
        return std::nullopt;
    }
    const std::string fmt = (*parsed)["format"].as_string();
    if (fmt != "astral.scene" && fmt != "astral.map" && fmt != "astral.entity") {
        if (error) *error = path.string() + ": unsupported document format \"" + fmt + "\"";
        return std::nullopt;
    }
    EditorDocument doc;
    doc.path = path;
    doc.root = std::move(*parsed);
    doc.revision = {1, hash_document_text(serialize_document(doc.root))};
    return doc;
}

} // namespace astral::editor
