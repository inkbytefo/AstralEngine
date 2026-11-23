#include <astral/editor/transaction.hpp>

#include <unordered_map>

namespace astral::editor {

namespace {

void diff_json_recursive(const std::string& prefix, const Json& a, const Json& b, std::vector<FieldChange>& out) {
    if (a == b) return;
    if (a.is_object() && b.is_object()) {
        for (const auto& [k, v] : a.as_object()) {
            const std::string p = prefix.empty() ? k : prefix + "." + k;
            const Json* bv = b.find(k);
            if (!bv) out.push_back({p, "removed", v.dump(), {}});
            else diff_json_recursive(p, v, *bv, out);
        }
        for (const auto& [k, v] : b.as_object()) {
            if (!a.find(k)) {
                const std::string p = prefix.empty() ? k : prefix + "." + k;
                out.push_back({p, "added", {}, v.dump()});
            }
        }
        return;
    }
    out.push_back({prefix, "modified", a.dump(), b.dump()});
}

std::unordered_map<std::uint32_t, const Json*> index_entities(const Json& doc) {
    std::unordered_map<std::uint32_t, const Json*> out;
    if (const Json* list = doc.find("entities"); list && list->is_array()) {
        for (std::size_t i = 0; i < list->size(); ++i) {
            const Json& rec = (*list)[i];
            const auto id = static_cast<std::uint32_t>(rec["id"].as_number(0.0));
            if (id != 0) out.emplace(id, &rec);
        }
    }
    return out;
}

bool persist_staged_files(IEditorFileSystem& fs,
                          const std::vector<std::pair<std::filesystem::path, std::string>>& staged_prefabs,
                          const std::filesystem::path& doc_path, const std::string& doc_text, std::string& error) {
    std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> written_backups;
    auto rollback_prefabs = [&]() {
        for (auto it = written_backups.rbegin(); it != written_backups.rend(); ++it) {
            if (it->second) (void)fs.write_atomic(it->first, *it->second, nullptr);
        }
    };
    for (const auto& [path, text] : staged_prefabs) {
        std::optional<std::string> prior = fs.read_text(path, nullptr);
        std::string io_err;
        if (!fs.write_atomic(path, text, &io_err)) {
            rollback_prefabs();
            error = "failed to write prefab " + path.string() + ": " + io_err;
            return false;
        }
        written_backups.emplace_back(path, std::move(prior));
    }
    std::string io_err;
    if (!fs.write_atomic(doc_path, doc_text, &io_err)) {
        rollback_prefabs();
        error = "atomic save failed (rolled back): " + io_err;
        return false;
    }
    return true;
}

} // namespace

std::string DocumentDiff::summary() const {
    if (changes.empty()) return "no changes";
    return std::to_string(changes.size()) + (changes.size() == 1 ? " change" : " changes");
}

DocumentDiff diff_documents(const Json& before, const Json& after) {
    DocumentDiff diff;
    const auto old_ents = index_entities(before);
    const auto new_ents = index_entities(after);
    for (const auto& [id, rec] : old_ents) {
        const std::string label = "entity " + std::to_string(id);
        auto it = new_ents.find(id);
        if (it == new_ents.end()) {
            diff.changes.push_back({label, "removed", rec->dump(), {}});
        } else {
            diff_json_recursive(label, *rec, *it->second, diff.changes);
        }
    }
    for (const auto& [id, rec] : new_ents) {
        if (!old_ents.count(id)) {
            diff.changes.push_back({"entity " + std::to_string(id), "added", {}, rec->dump()});
        }
    }
    return diff;
}

void AuditLog::record(AuditEntry entry) {
    entry.sequence = next_seq_++;
    entries_.push_back(std::move(entry));
}

std::string AuditLog::to_jsonl() const {
    std::string out;
    for (const AuditEntry& e : entries_) {
        Json j = Json::object();
        j["seq"] = static_cast<double>(e.sequence);
        j["actor"] = e.actor;
        j["action"] = e.action;
        j["summary"] = e.summary;
        j["target_path"] = e.target_path;
        j["base_revision"] = static_cast<double>(e.base_revision);
        j["new_revision"] = static_cast<double>(e.new_revision);
        j["change_count"] = static_cast<double>(e.change_count);
        j["ok"] = e.ok;
        if (!e.detail.empty()) j["detail"] = e.detail;
        out += j.dump() + "\n";
    }
    return out;
}

EditorSession::EditorSession(IEditorFileSystem& fs, const TypeRegistry& types, std::filesystem::path asset_root)
    : fs_(fs), types_(types), asset_root_(std::move(asset_root)) {}

bool EditorSession::open(const std::filesystem::path& path, std::string* error) {
    auto loaded = load_editor_document(fs_, path, error);
    if (!loaded) return false;
    doc_ = std::move(*loaded);
    open_ = true;
    dirty_ = false;
    staged_prefabs_.clear();
    const auto disk_text = fs_.read_text(doc_.path, nullptr);
    saved_disk_hash_ = disk_text ? hash_document_text(*disk_text) : doc_.revision.hash;
    snapshots_.clear();
    snapshots_.push_back({doc_.root, "initial load", {}});
    cursor_ = 0;
    return true;
}

bool EditorSession::reload(std::string* error) {
    if (!open_) {
        if (error) *error = "no document open";
        return false;
    }
    auto loaded = load_editor_document(fs_, doc_.path, error);
    if (!loaded) return false;
    const std::uint64_t next_num = doc_.revision.number + 1;
    doc_.root = std::move(loaded->root);
    doc_.revision = {next_num, hash_document_text(serialize_document(doc_.root))};
    const auto disk_text = fs_.read_text(doc_.path, nullptr);
    saved_disk_hash_ = disk_text ? hash_document_text(*disk_text) : doc_.revision.hash;
    dirty_ = false;
    staged_prefabs_.clear();
    snapshots_.clear();
    snapshots_.push_back({doc_.root, "reload", {}});
    cursor_ = 0;
    return true;
}

bool EditorSession::has_external_conflict() const {
    if (!open_) return false;
    const auto disk_text = fs_.read_text(doc_.path, nullptr);
    if (!disk_text) return false;
    return hash_document_text(*disk_text) != saved_disk_hash_;
}

bool EditorSession::save(std::string* error, bool force_overwrite) {
    if (!open_) {
        if (error) *error = "no document open";
        return false;
    }
    if (!force_overwrite && has_external_conflict()) {
        if (error) *error = "external modification detected on " + doc_.path.string();
        return false;
    }
    const std::string doc_text = serialize_document(doc_.root);
    std::string io_err;
    if (!persist_staged_files(fs_, staged_prefabs_, doc_.path, doc_text, io_err)) {
        if (error) *error = io_err;
        audit_.record({0, "user", "rollback", "Save failed", doc_.path.string(), doc_.revision.number,
                       doc_.revision.number, 0, false, io_err});
        return false;
    }
    staged_prefabs_.clear();
    saved_disk_hash_ = hash_document_text(doc_text);
    dirty_ = false;
    clear_autosave();
    audit_.record({0, "user", "save", "Saved document", doc_.path.string(), doc_.revision.number,
                   doc_.revision.number, 0, true, {}});
    return true;
}

std::filesystem::path EditorSession::autosave_path() const {
    if (!open_) return {};
    std::filesystem::path p = doc_.path;
    p += ".astral_autosave";
    return p;
}

bool EditorSession::write_autosave(std::string* error) {
    if (!open_) {
        if (error) *error = "no document open";
        return false;
    }
    return fs_.write_atomic(autosave_path(), serialize_document(doc_.root), error);
}

bool EditorSession::has_autosave() const {
    if (!open_) return false;
    return fs_.read_text(autosave_path(), nullptr).has_value();
}

bool EditorSession::restore_autosave(std::string* error) {
    if (!open_) {
        if (error) *error = "no document open";
        return false;
    }
    auto loaded = load_editor_document(fs_, autosave_path(), error);
    if (!loaded) return false;
    doc_.root = std::move(loaded->root);
    doc_.bump_revision();
    snapshots_.resize(cursor_ + 1);
    snapshots_.push_back({doc_.root, "restore autosave", {}});
    cursor_ = snapshots_.size() - 1;
    dirty_ = true;
    return true;
}

void EditorSession::clear_autosave() {
    if (!open_) return;
    std::error_code ec;
    std::filesystem::remove(autosave_path(), ec);
}

TransactionResult EditorSession::apply(const Transaction& tx) {
    TransactionResult res;
    if (!open_) return res.error = "no document open", res;
    if (tx.expected_revision.valid() && tx.expected_revision != doc_.revision) {
        res.stale_revision = true;
        res.error = "stale document revision: expected " + std::to_string(tx.expected_revision.number) +
                    ", current is " + std::to_string(doc_.revision.number);
        audit_.record({0, tx.author, "reject", tx.summary, doc_.path.string(), doc_.revision.number,
                       doc_.revision.number, 0, false, res.error});
        return res;
    }
    if (tx.commands.empty()) return res.error = "transaction has no commands", res;

    EditorDocument candidate = doc_;
    std::vector<std::pair<std::filesystem::path, std::string>> tx_prefabs;
    for (const EditorCommand& cmd : tx.commands) {
        if (cmd.kind == CommandKind::SavePrefab) {
            if (std::string err = validate_command(candidate, types_, cmd); !err.empty()) {
                res.error = std::move(err);
                res.rolled_back = true;
                audit_.record({0, tx.author, "rollback", tx.summary, doc_.path.string(), doc_.revision.number,
                               doc_.revision.number, 0, false, res.error});
                return res;
            }
            const std::filesystem::path target =
                asset_root_.empty() ? std::filesystem::path(cmd.asset_path) : asset_root_ / cmd.asset_path;
            tx_prefabs.emplace_back(target, serialize_document(cmd.value));
            continue;
        }
        if (std::string err = apply_command(candidate, types_, cmd, nullptr, asset_root_); !err.empty()) {
            res.error = std::move(err);
            res.rolled_back = true;
            audit_.record({0, tx.author, "rollback", tx.summary, doc_.path.string(), doc_.revision.number,
                           doc_.revision.number, 0, false, res.error});
            return res;
        }
    }

    const std::string doc_text = serialize_document(candidate.root);
    if (policy_ == PersistPolicy::Immediate) {
        if (!persist_staged_files(fs_, tx_prefabs, doc_.path, doc_text, res.error)) {
            res.rolled_back = true;
            audit_.record({0, tx.author, "rollback", tx.summary, doc_.path.string(), doc_.revision.number,
                           doc_.revision.number, 0, false, res.error});
            return res;
        }
        saved_disk_hash_ = hash_document_text(doc_text);
        dirty_ = false;
    } else {
        for (auto& p : tx_prefabs) staged_prefabs_.push_back(std::move(p));
        dirty_ = true;
    }

    res.diff = diff_documents(doc_.root, candidate.root);
    const std::uint64_t base_rev = doc_.revision.number;
    doc_.root = std::move(candidate.root);
    doc_.bump_revision();
    const bool coalesce = !tx.coalesce_key.empty() && cursor_ > 0 && cursor_ + 1 == snapshots_.size() &&
                          snapshots_[cursor_].coalesce_key == tx.coalesce_key;
    if (coalesce) {
        snapshots_[cursor_] = {doc_.root, tx.summary, tx.coalesce_key};
    } else {
        snapshots_.resize(cursor_ + 1);
        snapshots_.push_back({doc_.root, tx.summary, tx.coalesce_key});
        cursor_ = snapshots_.size() - 1;
    }
    res.ok = true;
    res.new_revision = doc_.revision;
    audit_.record({0, tx.author, "apply", tx.summary, doc_.path.string(), base_rev, doc_.revision.number,
                   res.diff.changes.size(), true, {}});
    return res;
}

TransactionResult EditorSession::restore_snapshot(std::size_t target_index, std::string_view action) {
    TransactionResult res;
    if (!open_ || target_index >= snapshots_.size()) {
        res.error = "nothing to " + std::string(action);
        return res;
    }
    const Json& target_root = snapshots_[target_index].root;
    const std::string text = serialize_document(target_root);
    if (policy_ == PersistPolicy::Immediate) {
        std::string io_err;
        if (!fs_.write_atomic(doc_.path, text, &io_err)) {
            res.error = std::string(action) + " save failed (rolled back): " + io_err;
            res.rolled_back = true;
            audit_.record({0, "user", "rollback", std::string(action), doc_.path.string(), doc_.revision.number,
                           doc_.revision.number, 0, false, res.error});
            return res;
        }
        saved_disk_hash_ = hash_document_text(text);
        dirty_ = false;
    } else {
        dirty_ = hash_document_text(text) != saved_disk_hash_;
    }
    res.diff = diff_documents(doc_.root, target_root);
    const std::uint64_t base_rev = doc_.revision.number;
    doc_.root = target_root;
    doc_.bump_revision();
    cursor_ = target_index;
    res.ok = true;
    res.new_revision = doc_.revision;
    audit_.record({0, "user", std::string(action), snapshots_[target_index].summary, doc_.path.string(),
                   base_rev, doc_.revision.number, res.diff.changes.size(), true, {}});
    return res;
}

TransactionResult EditorSession::undo() {
    return can_undo() ? restore_snapshot(cursor_ - 1, "undo") : TransactionResult{.error = "nothing to undo"};
}

TransactionResult EditorSession::redo() {
    return can_redo() ? restore_snapshot(cursor_ + 1, "redo") : TransactionResult{.error = "nothing to redo"};
}

} // namespace astral::editor
