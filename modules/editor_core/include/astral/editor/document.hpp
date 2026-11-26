#pragma once
// GPU-free editor document model, persistent revisions, and atomic filesystem abstraction (ADR-0035).

#include <astral/core/json.hpp>
#include <astral/world/scene.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

/// Monotonic revision + content hash of an open editor document (rejects stale commands/proposals).
struct DocumentRevision {
    std::uint64_t number = 0;
    std::uint64_t hash = 0;

    [[nodiscard]] bool valid() const noexcept { return number != 0; }
    friend bool operator==(const DocumentRevision&, const DocumentRevision&) = default;
};

/// Computes deterministic 64-bit FNV-1a content hash over canonical JSON text.
[[nodiscard]] std::uint64_t hash_document_text(std::string_view text) noexcept;

/// Canonical pretty-printed JSON serialization used by editor documents for byte-for-byte undo/redo.
[[nodiscard]] std::string serialize_document(const Json& doc);

/// Abstract file I/O allowing atomic disk persistence and fault injection in headless tests.
class IEditorFileSystem {
public:
    virtual ~IEditorFileSystem() = default;
    [[nodiscard]] virtual std::optional<std::string> read_text(const std::filesystem::path& path, std::string* error = nullptr) const = 0;
    /// Atomically writes `text` to `path` (temp file + rename). On failure, target must remain untouched.
    virtual bool write_atomic(const std::filesystem::path& path, std::string_view text, std::string* error = nullptr) = 0;
    /// Deletes `path`; true when it is gone afterwards (also when it did not exist). Used to undo files a failed
    /// transaction created (ADR-0053).
    virtual bool remove(const std::filesystem::path& path, std::string* error = nullptr) = 0;
    [[nodiscard]] virtual std::vector<std::filesystem::path> list_files(const std::filesystem::path& root,
                                                                        const std::vector<std::string>& extensions) const = 0;
};

/// Real filesystem implementation using temp file + flush + atomic rename.
class LocalEditorFileSystem final : public IEditorFileSystem {
public:
    [[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path, std::string* error = nullptr) const override;
    bool write_atomic(const std::filesystem::path& path, std::string_view text, std::string* error = nullptr) override;
    bool remove(const std::filesystem::path& path, std::string* error = nullptr) override;
    [[nodiscard]] std::vector<std::filesystem::path> list_files(const std::filesystem::path& root,
                                                                const std::vector<std::string>& extensions) const override;
};

/// In-memory view of an editable scene/prefab document bound to a disk path and revision.
struct EditorDocument {
    std::filesystem::path path;
    Json root = Json::object();
    DocumentRevision revision{};

    /// Returns a pointer to the entity record with `record_id`, or nullptr if absent.
    [[nodiscard]] const Json* find_record(std::uint32_t record_id) const noexcept;
    /// Replaces the entity record with `record_id` in `"entities"`. False if absent.
    bool replace_record(std::uint32_t record_id, Json updated);
    /// Returns the next unused positive record id in `"entities"`.
    [[nodiscard]] std::uint32_t next_record_id() const noexcept;
    /// Recomputes `revision.hash` from canonical serialized JSON and increments `revision.number`.
    void bump_revision();
};

/// Loads a scene/prefab JSON document through `fs`. Returns nullopt with `*error` on I/O or format error.
[[nodiscard]] std::optional<EditorDocument> load_editor_document(const IEditorFileSystem& fs,
                                                                 const std::filesystem::path& path,
                                                                 std::string* error = nullptr);

} // namespace astral::editor
