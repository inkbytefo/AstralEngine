#pragma once
// Atomic editor transactions, field-level diffs, undo/redo history, and audit logging (ADR-0035).

#include <astral/editor/command.hpp>
#include <astral/editor/document.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

struct FieldChange {
    std::string path;   ///< e.g. "entity 2 / astral::Transform.position"
    std::string kind;   ///< "added", "removed", "modified"
    std::string before;
    std::string after;
};

struct DocumentDiff {
    std::vector<FieldChange> changes;
    [[nodiscard]] bool empty() const noexcept { return changes.empty(); }
    [[nodiscard]] std::string summary() const;
};

/// Computes a structured field-level diff between two scene/prefab JSON documents.
[[nodiscard]] DocumentDiff diff_documents(const Json& before, const Json& after);

enum class PersistPolicy : std::uint8_t {
    Immediate,  ///< Every apply/undo/redo writes atomically to disk immediately (default for headless E0 API).
    ManualSave, ///< Edits stay in-memory with dirty state & coalescing until explicit save() (Ctrl+S).
};

struct Transaction {
    std::string author = "user";        ///< "user" or "agent:<id>"
    std::string summary;                ///< Human-readable description
    DocumentRevision expected_revision; ///< Must match open document's revision (stale check)
    std::string coalesce_key;           ///< Optional key to merge consecutive interactive drags into one undo step
    std::vector<EditorCommand> commands;
};

struct TransactionResult {
    bool ok = false;
    bool stale_revision = false;
    bool rolled_back = false;
    std::string error;
    DocumentRevision new_revision{};
    DocumentDiff diff;
};

struct AuditEntry {
    std::uint64_t sequence = 0;
    std::string actor;
    std::string action; ///< "apply", "undo", "redo", "reject", "rollback"
    std::string summary;
    std::string target_path;
    std::uint64_t base_revision = 0;
    std::uint64_t new_revision = 0;
    std::size_t change_count = 0;
    bool ok = true;
    std::string detail;
};

class AuditLog {
public:
    void record(AuditEntry entry);
    [[nodiscard]] const std::vector<AuditEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] std::string to_jsonl() const;
    void clear() { entries_.clear(); }

private:
    std::vector<AuditEntry> entries_;
    std::uint64_t next_seq_ = 1;
};

/// Owns an open editable document with transactional persistence, rollback, undo/redo, and audit log.
class EditorSession {
public:
    EditorSession(IEditorFileSystem& fs, const TypeRegistry& types, std::filesystem::path asset_root = {});

    bool open(const std::filesystem::path& path, std::string* error = nullptr);
    /// Re-reads the open document from disk; returns false on I/O or parse failure.
    bool reload(std::string* error = nullptr);
    [[nodiscard]] bool is_open() const noexcept { return open_; }
    [[nodiscard]] const EditorDocument& document() const noexcept { return doc_; }
    [[nodiscard]] const std::filesystem::path& asset_root() const noexcept { return asset_root_; }
    [[nodiscard]] const TypeRegistry& types() const noexcept { return types_; }
    [[nodiscard]] IEditorFileSystem& filesystem() noexcept { return fs_; }
    [[nodiscard]] AuditLog& audit_log() noexcept { return audit_; }
    [[nodiscard]] const AuditLog& audit_log() const noexcept { return audit_; }

    /// Validates, applies, and persists (per PersistPolicy) `tx`. On any validation or I/O failure, rolls back cleanly.
    TransactionResult apply(const Transaction& tx);
    TransactionResult undo();
    TransactionResult redo();

    void set_persist_policy(PersistPolicy policy) noexcept { policy_ = policy; }
    [[nodiscard]] PersistPolicy persist_policy() const noexcept { return policy_; }
    [[nodiscard]] bool is_dirty() const noexcept { return dirty_; }
    /// True if the file on disk was modified externally since it was last loaded or saved by this session.
    [[nodiscard]] bool has_external_conflict() const;
    /// Atomically writes the current in-memory document (and any staged prefabs) to disk.
    bool save(std::string* error = nullptr, bool force_overwrite = false);

    /// Crash-recovery sidecar autosave (`<path>.astral_autosave`).
    [[nodiscard]] std::filesystem::path autosave_path() const;
    bool write_autosave(std::string* error = nullptr);
    [[nodiscard]] bool has_autosave() const;
    bool restore_autosave(std::string* error = nullptr);
    void clear_autosave();

    [[nodiscard]] bool can_undo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool can_redo() const noexcept { return cursor_ + 1 < snapshots_.size(); }
    [[nodiscard]] std::size_t pending_change_count() const noexcept { return dirty_ ? std::max<std::size_t>(1, cursor_) : 0; }

private:
    struct Snapshot {
        Json root;
        std::string summary;
        std::string coalesce_key;
    };

    TransactionResult restore_snapshot(std::size_t target_index, std::string_view action);

    IEditorFileSystem& fs_;
    const TypeRegistry& types_;
    std::filesystem::path asset_root_;
    EditorDocument doc_;
    bool open_ = false;
    PersistPolicy policy_ = PersistPolicy::Immediate;
    bool dirty_ = false;
    std::uint64_t saved_disk_hash_ = 0;
    std::vector<std::pair<std::filesystem::path, std::string>> staged_prefabs_;
    std::vector<Snapshot> snapshots_;
    std::size_t cursor_ = 0;
    AuditLog audit_;
};

} // namespace astral::editor
